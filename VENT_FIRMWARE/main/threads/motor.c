/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "driver/mcpwm_prelude.h"
#include <driver/ledc.h>
#include "esp_err.h"
#include <stdio.h>
#include "esp_vfs_dev.h"
#include "driver/uart.h"
#include "sdkconfig.h"
#include "esp_console.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "GLOBAL_DEFINES.h"

static const char *MOTOR_TAG = "Motor";

#define MOTOR_CLOSE_PWM 180
#define MOTOR_OPEN_PWM 560

// Please consult the datasheet of your servo before changing the following parameters
#define SERVO_MIN_PULSEWIDTH_US 500  // Minimum pulse width in microsecond
#define SERVO_MAX_PULSEWIDTH_US 2500  // Maximum pulse width in microsecond
#define SERVO_MIN_DEGREE        -90   // Minimum angle
#define SERVO_MAX_DEGREE        90    // Maximum angle

#define SERVO_PULSE_GPIO             0        // GPIO connects to the PWM signal line
#define SERVO_TIMEBASE_RESOLUTION_HZ 1000000  // 1MHz, 1us per tick
#define SERVO_TIMEBASE_PERIOD        20000    // 20000 ticks, 20ms

#define BUTTON_1_PIN GPIO_NUM_5
#define BUTTON_2_PIN GPIO_NUM_18

#define LED_PIN 2
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_HIGH_SPEED_MODE
#define LEDC_OUTPUT_IO LED_PIN
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_DUTY_RES LEDC_TIMER_12_BIT // 12-bit resolution
#define LEDC_FREQUENCY 100 // Frequency in Hertz

// Create a queue to communicate with the interrupt handler
static QueueHandle_t gpio_evt_queue = NULL;

// Variables for debouncing
static uint64_t last_button_time = 0;
uint64_t last_button_1_time = 0;
uint64_t last_button_2_time = 0;


// Function to get current time in milliseconds
static uint64_t get_time_ms() {
    return esp_timer_get_time() / 1000;
}

// GPIO interrupt handler - this function executes when the interrupt occurs
static void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    
    // Add current time to the queue along with GPIO number for more accurate timing
    uint64_t current_time = esp_timer_get_time() / 1000;
    
    // Create a structure to hold both GPIO number and timestamp
    struct button_evt_t {
        uint32_t gpio;
        uint64_t time;
    };
    
    struct button_evt_t evt_data;
    evt_data.gpio = gpio_num;
    evt_data.time = current_time;
    
    // Send to queue with higher priority
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(gpio_evt_queue, &evt_data, &xHigherPriorityTaskWoken);
    
    // Yield to higher priority task if one became available
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}


// Function declarations
esp_err_t configure_motor_gpio(void);
void set_motor_duty(int duty);
void motor_task_entry(void *pvParameter);

esp_err_t configure_motor_gpio(void)
{
    // Configure the LEDC timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .duty_resolution = LEDC_DUTY_RES,
        .timer_num = LEDC_TIMER,
        .freq_hz = LEDC_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    // Configure the LEDC channel
    ledc_channel_config_t ledc_channel = {
        .gpio_num = LEDC_OUTPUT_IO,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&ledc_channel);

    // Initialize fading service
    ledc_fade_func_install(0);
    return ESP_OK;
}

void configure_button_gpio(void)
{
    // Create a queue with correct structure size
    gpio_evt_queue = xQueueCreate(10, sizeof(struct {
        uint32_t gpio;
        uint64_t time;
    }));
    
    // Configure button pins for input with internal pull-up and interrupt
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_ANYEDGE;  // Trigger on both rising and falling edges
    io_conf.mode = GPIO_MODE_INPUT;         // Set as input mode
    io_conf.pin_bit_mask = (1ULL << BUTTON_1_PIN) | (1ULL << BUTTON_2_PIN);  // Bit mask for both pins
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;     // Enable pull-up
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; // Disable pull-down
    gpio_config(&io_conf);
    
    // Install GPIO ISR service with higher priority
    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
    
    // Hook ISR handler for specific GPIO pins
    gpio_isr_handler_add(BUTTON_1_PIN, gpio_isr_handler, (void*) BUTTON_1_PIN);
    gpio_isr_handler_add(BUTTON_2_PIN, gpio_isr_handler, (void*) BUTTON_2_PIN);
}


void set_motor_duty(int duty)
{
    if (duty < 0 || duty > 4095) {
        printf("Invalid input. Please try again.\n");
    } else {
        
        // note: duty cycles have been flipped
        if (duty == MOTOR_CLOSE_PWM) ESP_LOGI(MOTOR_TAG, "** Closing Vent -> %s", (VENT_COLOR == BROWN_VENT) ? "Brown Vent" : "White Vent");
        else if (duty == MOTOR_OPEN_PWM) ESP_LOGI(MOTOR_TAG, "** Opening Vent -> %s", (VENT_COLOR == BROWN_VENT) ? "Brown Vent" : "White Vent");

        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
        ESP_LOGI(MOTOR_TAG, "DUTY: %d", duty);
    }
}

// pass in int 0-100 representing percentage of vent open
void set_motor_position(int precentage_open)
{
    if (precentage_open > 100) precentage_open = 100;
    if (precentage_open < 0) precentage_open = 0;

    precentage_open = 100 - precentage_open;
            
    int desired_motor_pwm = ((MOTOR_OPEN_PWM-MOTOR_CLOSE_PWM)/100.0f)*precentage_open + MOTOR_CLOSE_PWM;

    set_motor_duty(desired_motor_pwm);
}

//min 170
//max 1050
// pin D2
void motor_task_entry(void *pvParameter)
{
    configure_motor_gpio();
    configure_button_gpio();

    int duty = 0;
    int position = 0;

    struct {
        uint32_t gpio;
        uint64_t time;
    } evt_data;

    // Increase debounce time
    const uint64_t DEBOUNCE_TIME = 200; // Increase to 200ms for better debouncing

    int percentage = 0;


    while (1) {
        // Wait for an item from the queue
        if (xQueueReceive(gpio_evt_queue, &evt_data, portMAX_DELAY)) {
            uint32_t gpio_num = evt_data.gpio;
            uint64_t event_time = evt_data.time;
            
            // Read button state - LOW (0) means pressed with pull-up
            bool button_pressed = (gpio_get_level((gpio_num_t)gpio_num) == 0);
            
            // Only process button press events (not releases)
            if (button_pressed) {
                // ESP_LOGI(MOTOR_TAG, "Button event: GPIO %d, State: %s", 
                //          gpio_num, button_pressed ? "Pressed" : "Released");
                
                // Handle Button 1
                if (gpio_num == BUTTON_1_PIN) {
                    if ((event_time - last_button_1_time) > DEBOUNCE_TIME) {
                        ESP_LOGI(MOTOR_TAG, "Button 1 pressed (debounced)");
                        // close motor
                        set_motor_position(0);
                        last_button_1_time = event_time;
                    } else {
                        ESP_LOGD(MOTOR_TAG, "Button 1 debounced (ignored)");
                    }
                }
                // Handle Button 2
                else if (gpio_num == BUTTON_2_PIN) {
                    if ((event_time - last_button_2_time) > DEBOUNCE_TIME) {
                        ESP_LOGI(MOTOR_TAG, "Button 2 pressed (debounced)");
                        // open motor
                        set_motor_position(percentage);
                        last_button_2_time = event_time;

                        percentage = (percentage == 0) ? 100 : 0;
                    } else {
                        ESP_LOGD(MOTOR_TAG, "Button 2 debounced (ignored)");
                    }
                }
            }
        }

        // printf("Enter a number between 0 and 4095: ");
        // if (scanf("%d", &duty) == 1) {
        //     set_motor_duty(duty);
        // } else {
        //     printf("Failed to read input. Please try again.\n");
        //     // Clear the input buffer
        //     while (getchar() != '\n');
        // }
        // vTaskDelay(pdMS_TO_TICKS(100)); // Small delay to prevent task hogging
    }
}