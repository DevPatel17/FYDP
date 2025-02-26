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
#include "driver/adc.h"
#include "esp_adc_cal.h"


#define GPIO_INPUT_IO_34     CONFIG_GPIO_INPUT_34
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_IO_34))

static const char *TEMP_SENSE_TAG = "Temp_Sense";

extern float ADC_TO_TEMP_LUT[4096];

// Function declarations
void temp_sense_task_entry(void *pvParameter);


void temp_sense_task_entry(void *pvParameter)
{

    int duty = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000)); // delay to prevent task hogging


        uint32_t raw_adc =  adc1_get_raw(ADC6_CHANNEL_1);




    }
}