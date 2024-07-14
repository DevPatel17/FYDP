/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/mcpwm_prelude.h"
#include <driver/ledc.h>

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_err.h"

#include <stdio.h>

// #include "protocol_examples_common.h"
#include "esp_err.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"
#include "sdkconfig.h"


static const char *TAG = "example";

// Please consult the datasheet of your servo before changing the following parameters
#define SERVO_MIN_PULSEWIDTH_US 500  // Minimum pulse width in microsecond
#define SERVO_MAX_PULSEWIDTH_US 2500  // Maximum pulse width in microsecond
#define SERVO_MIN_DEGREE        -90   // Minimum angle
#define SERVO_MAX_DEGREE        90    // Maximum angle

#define SERVO_PULSE_GPIO             0        // GPIO connects to the PWM signal line
#define SERVO_TIMEBASE_RESOLUTION_HZ 1000000  // 1MHz, 1us per tick
#define SERVO_TIMEBASE_PERIOD        20000    // 20000 ticks, 20ms

static inline uint32_t example_angle_to_compare(int angle)
{
    return (angle - SERVO_MIN_DEGREE) * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) / (SERVO_MAX_DEGREE - SERVO_MIN_DEGREE) + SERVO_MIN_PULSEWIDTH_US;
}

#define LED_PIN 2
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_HIGH_SPEED_MODE
#define LEDC_OUTPUT_IO LED_PIN
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_DUTY_RES LEDC_TIMER_12_BIT // 12-bit resolution
#define LEDC_FREQUENCY 100 // Frequency in Hertz

esp_err_t example_configure_stdin_stdout(void)
{
    static bool configured = false;
    if (configured) {
      return ESP_OK;
    }
    // Initialize VFS & UART so we can use std::cout/cin
    setvbuf(stdin, NULL, _IONBF, 0);
    /* Install UART driver for interrupt-driven reads and writes */
    ESP_ERROR_CHECK( uart_driver_install( (uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM,
            256, 0, 0, NULL, 0) );
    /* Tell VFS to use UART driver */
    esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
    esp_vfs_dev_uart_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    esp_vfs_dev_uart_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CRLF);
    configured = true;
    return ESP_OK;
}

void app_main(void)
{

    // set up uart for stdin:
    example_configure_stdin_stdout();


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

    int prev_pre_duty = -2;
    int prev_duty = -1;
    int duty = 0;
    // min motor: 170 (0 DEG)
    // max motor; 1050 (360 DEG)
    while (1) {

        printf("Enter a number between 0 and 4095: ");
        scanf("%d", &duty);

        if (duty < 0 || duty > 4095) {
            printf("Invalid input. Please try again.\n");
            // vTaskDelay(pdMS_TO_TICKS(5000));
        }

        else {
            ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
            ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
            ESP_LOGI(TAG, "DUTY: %d", duty);
            // vTaskDelay(pdMS_TO_TICKtS(5000));           
        }
        // Gradually increase the brightness
        // for (int duty = 1700; duty <= 4060; duty+=100) {
        //     ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
        //     ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
        //     ESP_LOGI(TAG, "DUTY: %d", duty);
        //     vTaskDelay(pdMS_TO_TICKS(100));
        // }
        // // Gradually decrease the brightness
        // for (int duty = 4060; duty >= 1700; duty-=100) {
        //     ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
        //     ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
        //     ESP_LOGI(TAG, "DUTY: %d", duty);
        //     vTaskDelay(pdMS_TO_TICKS(100));
        // }

    }
}