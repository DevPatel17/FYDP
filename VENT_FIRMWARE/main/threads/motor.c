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

static const char *MOTOR_TAG = "Motor";


// Please consult the datasheet of your servo before changing the following parameters
#define SERVO_MIN_PULSEWIDTH_US 500  // Minimum pulse width in microsecond
#define SERVO_MAX_PULSEWIDTH_US 2500  // Maximum pulse width in microsecond
#define SERVO_MIN_DEGREE        -90   // Minimum angle
#define SERVO_MAX_DEGREE        90    // Maximum angle

#define SERVO_PULSE_GPIO             0        // GPIO connects to the PWM signal line
#define SERVO_TIMEBASE_RESOLUTION_HZ 1000000  // 1MHz, 1us per tick
#define SERVO_TIMEBASE_PERIOD        20000    // 20000 ticks, 20ms

#define LED_PIN 2
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_HIGH_SPEED_MODE
#define LEDC_OUTPUT_IO LED_PIN
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_DUTY_RES LEDC_TIMER_12_BIT // 12-bit resolution
#define LEDC_FREQUENCY 100 // Frequency in Hertz

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

void set_motor_duty(int duty)
{
    if (duty < 0 || duty > 4095) {
        printf("Invalid input. Please try again.\n");
    } else {
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
        ESP_LOGI(MOTOR_TAG, "DUTY: %d", duty);
    }
}

//min 170
//max 1050
// pin D2
void motor_task_entry(void *pvParameter)
{
    configure_motor_gpio();

    int duty = 0;
    while (1) {
        printf("Enter a number between 0 and 4095: ");
        if (scanf("%d", &duty) == 1) {
            set_motor_duty(duty);
        } else {
            printf("Failed to read input. Please try again.\n");
            // Clear the input buffer
            while (getchar() != '\n');
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Small delay to prevent task hogging
    }
}