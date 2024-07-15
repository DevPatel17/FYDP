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

#include "esp_err.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"
#include "sdkconfig.h"

#include "esp_console.h"
#include "sdkconfig.h"
#include "threads/motor.c"

#define PROMPT_STR CONFIG_IDF_TARGET
#define TASK_PRIO_3         3
#define TASK_PRIO_2         2
#define COMP_LOOP_PERIOD    5000

// Function declarations
esp_err_t configure_stdin_stdout(void);
void motor_task_entry(void *pvParameter);

void app_main(void)
{
    // Configure UART for standard I/O
    configure_stdin_stdout();
    // Create the motor task
    xTaskCreate(&motor_task_entry, "motor_task", 4096, NULL, 5, NULL);
}

esp_err_t configure_stdin_stdout(void)
{
    static bool configured = false;
    if (configured) {
        return ESP_OK;
    } 
    // Initialize VFS & UART for stdio
    setvbuf(stdin, NULL, _IONBF, 0);
    // Install UART driver
    ESP_ERROR_CHECK(uart_driver_install((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0));
    // Use UART driver for VFS
    esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
    esp_vfs_dev_uart_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CR);
    esp_vfs_dev_uart_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CRLF);
    configured = true;
    return ESP_OK;
}