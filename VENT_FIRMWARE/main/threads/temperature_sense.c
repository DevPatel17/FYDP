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
 #include "GLOBAL_DEFINES.h"

//  #include "lut.h"
 
 
 #define GPIO_INPUT_IO_34     CONFIG_GPIO_INPUT_34
 #define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_IO_34))

 #define DEFAULT_VREF    1100        // Use adc_vref_to_gpio() to get a better estimate
#define NO_OF_SAMPLES   64          // Multisampling

// extern float ADC_TO_TEMP_LUT[4096];

static esp_adc_cal_characteristics_t *adc_chars;
static const adc_channel_t channel = ADC_CHANNEL_4;     // GPIO32 if ADC1
static const adc_bits_width_t width = ADC_WIDTH_BIT_12;
static const adc_atten_t atten = ADC_ATTEN_DB_11;
static const adc_unit_t unit = ADC_UNIT_1;
 
 static const char *TEMP_SENSE_TAG = "Temp_Sense";
 
 const float ADC_TO_TEMP_LUT[4096] = {
    -50.00f, -50.00f, -50.00f, -50.00f, -50.00f, -50.00f, -50.00f, -50.00f, 
    -50.00f, -50.00f, -50.00f, -50.00f, -50.00f, -50.00f, -50.00f, -50.00f, 
    -50.00f, -50.00f, -50.00f, -50.00f, -50.00f, -50.00f, -50.00f, -50.00f, 
    -50.00f, -50.00f, -50.00f, -50.00f, -50.00f, -50.00f, -50.00f, -50.00f, 
    -50.00f, -50.00f, -50.00f, -50.00f, -50.00f, -50.00f, -50.00f, -50.00f, 
    -50.00f, -50.00f, -50.00f, -50.00f, -50.00f, -50.00f, -50.00f, -50.00f, 
    -49.77f, -49.51f, -49.25f, -48.99f, -48.74f, -48.50f, -48.25f, -48.02f, 
    -47.78f, -47.55f, -47.32f, -47.10f, -46.88f, -46.66f, -46.44f, -46.23f, 
    -46.02f, -45.82f, -45.62f, -45.41f, -45.22f, -45.02f, -44.83f, -44.64f, 
    -44.45f, -44.26f, -44.08f, -43.90f, -43.72f, -43.54f, -43.36f, -43.19f, 
    -43.02f, -42.85f, -42.68f, -42.51f, -42.35f, -42.19f, -42.02f, -41.87f, 
    -41.71f, -41.55f, -41.39f, -41.24f, -41.09f, -40.94f, -40.79f, -40.64f, 
    -40.49f, -40.35f, -40.20f, -40.06f, -39.92f, -39.78f, -39.64f, -39.50f, 
    -39.36f, -39.23f, -39.09f, -38.96f, -38.83f, -38.69f, -38.56f, -38.43f, 
    -38.31f, -38.18f, -38.05f, -37.93f, -37.80f, -37.68f, -37.55f, -37.43f, 
    -37.31f, -37.19f, -37.07f, -36.95f, -36.83f, -36.72f, -36.60f, -36.48f, 
    -36.37f, -36.26f, -36.14f, -36.03f, -35.92f, -35.81f, -35.70f, -35.59f, 
    -35.48f, -35.37f, -35.26f, -35.15f, -35.05f, -34.94f, -34.84f, -34.73f, 
    -34.63f, -34.52f, -34.42f, -34.32f, -34.22f, -34.12f, -34.02f, -33.92f, 
    -33.82f, -33.72f, -33.62f, -33.52f, -33.42f, -33.33f, -33.23f, -33.14f, 
    -33.04f, -32.95f, -32.85f, -32.76f, -32.67f, -32.57f, -32.48f, -32.39f, 
    -32.30f, -32.21f, -32.12f, -32.03f, -31.94f, -31.85f, -31.76f, -31.67f, 
    -31.58f, -31.49f, -31.41f, -31.32f, -31.23f, -31.15f, -31.06f, -30.98f, 
    -30.89f, -30.81f, -30.72f, -30.64f, -30.56f, -30.47f, -30.39f, -30.31f, 
    -30.23f, -30.15f, -30.07f, -29.98f, -29.90f, -29.82f, -29.74f, -29.67f, 
    -29.59f, -29.51f, -29.43f, -29.35f, -29.27f, -29.20f, -29.12f, -29.04f, 
    -28.96f, -28.89f, -28.81f, -28.74f, -28.66f, -28.59f, -28.51f, -28.44f, 
    -28.36f, -28.29f, -28.21f, -28.14f, -28.07f, -27.99f, -27.92f, -27.85f, 
    -27.78f, -27.71f, -27.63f, -27.56f, -27.49f, -27.42f, -27.35f, -27.28f, 
    -27.21f, -27.14f, -27.07f, -27.00f, -26.93f, -26.86f, -26.79f, -26.73f, 
    -26.66f, -26.59f, -26.52f, -26.45f, -26.39f, -26.32f, -26.25f, -26.19f, 
    -26.12f, -26.05f, -25.99f, -25.92f, -25.86f, -25.79f, -25.73f, -25.66f, 
    -25.60f, -25.53f, -25.47f, -25.40f, -25.34f, -25.28f, -25.21f, -25.15f, 
    -25.09f, -25.02f, -24.96f, -24.90f, -24.83f, -24.77f, -24.71f, -24.65f, 
    -24.59f, -24.53f, -24.46f, -24.40f, -24.34f, -24.28f, -24.22f, -24.16f, 
    -24.10f, -24.04f, -23.98f, -23.92f, -23.86f, -23.80f, -23.74f, -23.68f, 
    -23.62f, -23.57f, -23.51f, -23.45f, -23.39f, -23.33f, -23.27f, -23.22f, 
    -23.16f, -23.10f, -23.04f, -22.99f, -22.93f, -22.87f, -22.81f, -22.76f, 
    -22.70f, -22.65f, -22.59f, -22.53f, -22.48f, -22.42f, -22.37f, -22.31f, 
    -22.25f, -22.20f, -22.14f, -22.09f, -22.03f, -21.98f, -21.93f, -21.87f, 
    -21.82f, -21.76f, -21.71f, -21.65f, -21.60f, -21.55f, -21.49f, -21.44f, 
    -21.39f, -21.33f, -21.28f, -21.23f, -21.18f, -21.12f, -21.07f, -21.02f, 
    -20.97f, -20.91f, -20.86f, -20.81f, -20.76f, -20.71f, -20.65f, -20.60f, 
    -20.55f, -20.50f, -20.45f, -20.40f, -20.35f, -20.30f, -20.25f, -20.20f, 
    -20.15f, -20.10f, -20.05f, -19.99f, -19.94f, -19.90f, -19.85f, -19.80f, 
    -19.75f, -19.70f, -19.65f, -19.60f, -19.55f, -19.50f, -19.45f, -19.40f, 
    -19.35f, -19.30f, -19.26f, -19.21f, -19.16f, -19.11f, -19.06f, -19.01f, 
    -18.97f, -18.92f, -18.87f, -18.82f, -18.78f, -18.73f, -18.68f, -18.63f, 
    -18.59f, -18.54f, -18.49f, -18.45f, -18.40f, -18.35f, -18.31f, -18.26f, 
    -18.21f, -18.17f, -18.12f, -18.07f, -18.03f, -17.98f, -17.94f, -17.89f, 
    -17.84f, -17.80f, -17.75f, -17.71f, -17.66f, -17.62f, -17.57f, -17.53f, 
    -17.48f, -17.44f, -17.39f, -17.35f, -17.30f, -17.26f, -17.21f, -17.17f, 
    -17.12f, -17.08f, -17.03f, -16.99f, -16.95f, -16.90f, -16.86f, -16.81f, 
    -16.77f, -16.73f, -16.68f, -16.64f, -16.60f, -16.55f, -16.51f, -16.47f, 
    -16.42f, -16.38f, -16.34f, -16.29f, -16.25f, -16.21f, -16.16f, -16.12f, 
    -16.08f, -16.04f, -15.99f, -15.95f, -15.91f, -15.87f, -15.83f, -15.78f, 
    -15.74f, -15.70f, -15.66f, -15.62f, -15.57f, -15.53f, -15.49f, -15.45f, 
    -15.41f, -15.37f, -15.32f, -15.28f, -15.24f, -15.20f, -15.16f, -15.12f, 
    -15.08f, -15.04f, -15.00f, -14.95f, -14.91f, -14.87f, -14.83f, -14.79f, 
    -14.75f, -14.71f, -14.67f, -14.63f, -14.59f, -14.55f, -14.51f, -14.47f, 
    -14.43f, -14.39f, -14.35f, -14.31f, -14.27f, -14.23f, -14.19f, -14.15f, 
    -14.11f, -14.07f, -14.03f, -13.99f, -13.95f, -13.91f, -13.88f, -13.84f, 
    -13.80f, -13.76f, -13.72f, -13.68f, -13.64f, -13.60f, -13.56f, -13.52f, 
    -13.49f, -13.45f, -13.41f, -13.37f, -13.33f, -13.29f, -13.26f, -13.22f, 
    -13.18f, -13.14f, -13.10f, -13.06f, -13.03f, -12.99f, -12.95f, -12.91f, 
    -12.88f, -12.84f, -12.80f, -12.76f, -12.72f, -12.69f, -12.65f, -12.61f, 
    -12.57f, -12.54f, -12.50f, -12.46f, -12.43f, -12.39f, -12.35f, -12.31f, 
    -12.28f, -12.24f, -12.20f, -12.17f, -12.13f, -12.09f, -12.06f, -12.02f, 
    -11.98f, -11.95f, -11.91f, -11.87f, -11.84f, -11.80f, -11.76f, -11.73f, 
    -11.69f, -11.66f, -11.62f, -11.58f, -11.55f, -11.51f, -11.47f, -11.44f, 
    -11.40f, -11.37f, -11.33f, -11.30f, -11.26f, -11.22f, -11.19f, -11.15f, 
    -11.12f, -11.08f, -11.05f, -11.01f, -10.98f, -10.94f, -10.90f, -10.87f, 
    -10.83f, -10.80f, -10.76f, -10.73f, -10.69f, -10.66f, -10.62f, -10.59f, 
    -10.55f, -10.52f, -10.48f, -10.45f, -10.41f, -10.38f, -10.35f, -10.31f, 
    -10.28f, -10.24f, -10.21f, -10.17f, -10.14f, -10.10f, -10.07f, -10.04f, 
    -10.00f, -9.97f, -9.93f, -9.90f, -9.86f, -9.83f, -9.80f, -9.76f, 
    -9.73f, -9.69f, -9.66f, -9.63f, -9.59f, -9.56f, -9.53f, -9.49f, 
    -9.46f, -9.42f, -9.39f, -9.36f, -9.32f, -9.29f, -9.26f, -9.22f, 
    -9.19f, -9.16f, -9.12f, -9.09f, -9.06f, -9.02f, -8.99f, -8.96f, 
    -8.92f, -8.89f, -8.86f, -8.83f, -8.79f, -8.76f, -8.73f, -8.69f, 
    -8.66f, -8.63f, -8.60f, -8.56f, -8.53f, -8.50f, -8.47f, -8.43f, 
    -8.40f, -8.37f, -8.33f, -8.30f, -8.27f, -8.24f, -8.21f, -8.17f, 
    -8.14f, -8.11f, -8.08f, -8.04f, -8.01f, -7.98f, -7.95f, -7.92f, 
    -7.88f, -7.85f, -7.82f, -7.79f, -7.76f, -7.72f, -7.69f, -7.66f, 
    -7.63f, -7.60f, -7.56f, -7.53f, -7.50f, -7.47f, -7.44f, -7.41f, 
    -7.38f, -7.34f, -7.31f, -7.28f, -7.25f, -7.22f, -7.19f, -7.16f, 
    -7.12f, -7.09f, -7.06f, -7.03f, -7.00f, -6.97f, -6.94f, -6.91f, 
    -6.87f, -6.84f, -6.81f, -6.78f, -6.75f, -6.72f, -6.69f, -6.66f, 
    -6.63f, -6.60f, -6.57f, -6.53f, -6.50f, -6.47f, -6.44f, -6.41f, 
    -6.38f, -6.35f, -6.32f, -6.29f, -6.26f, -6.23f, -6.20f, -6.17f, 
    -6.14f, -6.11f, -6.08f, -6.05f, -6.02f, -5.98f, -5.95f, -5.92f, 
    -5.89f, -5.86f, -5.83f, -5.80f, -5.77f, -5.74f, -5.71f, -5.68f, 
    -5.65f, -5.62f, -5.59f, -5.56f, -5.53f, -5.50f, -5.47f, -5.44f, 
    -5.41f, -5.38f, -5.35f, -5.32f, -5.29f, -5.27f, -5.24f, -5.21f, 
    -5.18f, -5.15f, -5.12f, -5.09f, -5.06f, -5.03f, -5.00f, -4.97f, 
    -4.94f, -4.91f, -4.88f, -4.85f, -4.82f, -4.79f, -4.76f, -4.73f, 
    -4.71f, -4.68f, -4.65f, -4.62f, -4.59f, -4.56f, -4.53f, -4.50f, 
    -4.47f, -4.44f, -4.41f, -4.39f, -4.36f, -4.33f, -4.30f, -4.27f, 
    -4.24f, -4.21f, -4.18f, -4.15f, -4.13f, -4.10f, -4.07f, -4.04f, 
    -4.01f, -3.98f, -3.95f, -3.92f, -3.90f, -3.87f, -3.84f, -3.81f, 
    -3.78f, -3.75f, -3.72f, -3.70f, -3.67f, -3.64f, -3.61f, -3.58f, 
    -3.55f, -3.53f, -3.50f, -3.47f, -3.44f, -3.41f, -3.38f, -3.36f, 
    -3.33f, -3.30f, -3.27f, -3.24f, -3.21f, -3.19f, -3.16f, -3.13f, 
    -3.10f, -3.07f, -3.05f, -3.02f, -2.99f, -2.96f, -2.93f, -2.91f, 
    -2.88f, -2.85f, -2.82f, -2.79f, -2.77f, -2.74f, -2.71f, -2.68f, 
    -2.66f, -2.63f, -2.60f, -2.57f, -2.55f, -2.52f, -2.49f, -2.46f, 
    -2.43f, -2.41f, -2.38f, -2.35f, -2.32f, -2.30f, -2.27f, -2.24f, 
    -2.21f, -2.19f, -2.16f, -2.13f, -2.10f, -2.08f, -2.05f, -2.02f, 
    -2.00f, -1.97f, -1.94f, -1.91f, -1.89f, -1.86f, -1.83f, -1.80f, 
    -1.78f, -1.75f, -1.72f, -1.70f, -1.67f, -1.64f, -1.61f, -1.59f, 
    -1.56f, -1.53f, -1.51f, -1.48f, -1.45f, -1.43f, -1.40f, -1.37f, 
    -1.35f, -1.32f, -1.29f, -1.26f, -1.24f, -1.21f, -1.18f, -1.16f, 
    -1.13f, -1.10f, -1.08f, -1.05f, -1.02f, -1.00f, -0.97f, -0.94f, 
    -0.92f, -0.89f, -0.86f, -0.84f, -0.81f, -0.78f, -0.76f, -0.73f, 
    -0.70f, -0.68f, -0.65f, -0.62f, -0.60f, -0.57f, -0.55f, -0.52f, 
    -0.49f, -0.47f, -0.44f, -0.41f, -0.39f, -0.36f, -0.33f, -0.31f, 
    -0.28f, -0.26f, -0.23f, -0.20f, -0.18f, -0.15f, -0.12f, -0.10f, 
    -0.07f, -0.05f, -0.02f, 0.01f, 0.03f, 0.06f, 0.08f, 0.11f, 
    0.14f, 0.16f, 0.19f, 0.21f, 0.24f, 0.27f, 0.29f, 0.32f, 
    0.34f, 0.37f, 0.40f, 0.42f, 0.45f, 0.47f, 0.50f, 0.53f, 
    0.55f, 0.58f, 0.60f, 0.63f, 0.65f, 0.68f, 0.71f, 0.73f, 
    0.76f, 0.78f, 0.81f, 0.83f, 0.86f, 0.89f, 0.91f, 0.94f, 
    0.96f, 0.99f, 1.01f, 1.04f, 1.06f, 1.09f, 1.12f, 1.14f, 
    1.17f, 1.19f, 1.22f, 1.24f, 1.27f, 1.29f, 1.32f, 1.34f, 
    1.37f, 1.40f, 1.42f, 1.45f, 1.47f, 1.50f, 1.52f, 1.55f, 
    1.57f, 1.60f, 1.62f, 1.65f, 1.67f, 1.70f, 1.72f, 1.75f, 
    1.77f, 1.80f, 1.82f, 1.85f, 1.87f, 1.90f, 1.93f, 1.95f, 
    1.98f, 2.00f, 2.03f, 2.05f, 2.08f, 2.10f, 2.13f, 2.15f, 
    2.18f, 2.20f, 2.23f, 2.25f, 2.28f, 2.30f, 2.33f, 2.35f, 
    2.38f, 2.40f, 2.42f, 2.45f, 2.47f, 2.50f, 2.52f, 2.55f, 
    2.57f, 2.60f, 2.62f, 2.65f, 2.67f, 2.70f, 2.72f, 2.75f, 
    2.77f, 2.80f, 2.82f, 2.85f, 2.87f, 2.90f, 2.92f, 2.94f, 
    2.97f, 2.99f, 3.02f, 3.04f, 3.07f, 3.09f, 3.12f, 3.14f, 
    3.17f, 3.19f, 3.22f, 3.24f, 3.26f, 3.29f, 3.31f, 3.34f, 
    3.36f, 3.39f, 3.41f, 3.44f, 3.46f, 3.48f, 3.51f, 3.53f, 
    3.56f, 3.58f, 3.61f, 3.63f, 3.65f, 3.68f, 3.70f, 3.73f, 
    3.75f, 3.78f, 3.80f, 3.82f, 3.85f, 3.87f, 3.90f, 3.92f, 
    3.95f, 3.97f, 3.99f, 4.02f, 4.04f, 4.07f, 4.09f, 4.12f, 
    4.14f, 4.16f, 4.19f, 4.21f, 4.24f, 4.26f, 4.28f, 4.31f, 
    4.33f, 4.36f, 4.38f, 4.40f, 4.43f, 4.45f, 4.48f, 4.50f, 
    4.52f, 4.55f, 4.57f, 4.60f, 4.62f, 4.64f, 4.67f, 4.69f, 
    4.72f, 4.74f, 4.76f, 4.79f, 4.81f, 4.84f, 4.86f, 4.88f, 
    4.91f, 4.93f, 4.96f, 4.98f, 5.00f, 5.03f, 5.05f, 5.07f, 
    5.10f, 5.12f, 5.15f, 5.17f, 5.19f, 5.22f, 5.24f, 5.26f, 
    5.29f, 5.31f, 5.34f, 5.36f, 5.38f, 5.41f, 5.43f, 5.45f, 
    5.48f, 5.50f, 5.52f, 5.55f, 5.57f, 5.60f, 5.62f, 5.64f, 
    5.67f, 5.69f, 5.71f, 5.74f, 5.76f, 5.78f, 5.81f, 5.83f, 
    5.85f, 5.88f, 5.90f, 5.92f, 5.95f, 5.97f, 6.00f, 6.02f, 
    6.04f, 6.07f, 6.09f, 6.11f, 6.14f, 6.16f, 6.18f, 6.21f, 
    6.23f, 6.25f, 6.28f, 6.30f, 6.32f, 6.35f, 6.37f, 6.39f, 
    6.42f, 6.44f, 6.46f, 6.49f, 6.51f, 6.53f, 6.56f, 6.58f, 
    6.60f, 6.63f, 6.65f, 6.67f, 6.70f, 6.72f, 6.74f, 6.77f, 
    6.79f, 6.81f, 6.83f, 6.86f, 6.88f, 6.90f, 6.93f, 6.95f, 
    6.97f, 7.00f, 7.02f, 7.04f, 7.07f, 7.09f, 7.11f, 7.14f, 
    7.16f, 7.18f, 7.21f, 7.23f, 7.25f, 7.27f, 7.30f, 7.32f, 
    7.34f, 7.37f, 7.39f, 7.41f, 7.44f, 7.46f, 7.48f, 7.50f, 
    7.53f, 7.55f, 7.57f, 7.60f, 7.62f, 7.64f, 7.67f, 7.69f, 
    7.71f, 7.73f, 7.76f, 7.78f, 7.80f, 7.83f, 7.85f, 7.87f, 
    7.89f, 7.92f, 7.94f, 7.96f, 7.99f, 8.01f, 8.03f, 8.05f, 
    8.08f, 8.10f, 8.12f, 8.15f, 8.17f, 8.19f, 8.21f, 8.24f, 
    8.26f, 8.28f, 8.31f, 8.33f, 8.35f, 8.37f, 8.40f, 8.42f, 
    8.44f, 8.46f, 8.49f, 8.51f, 8.53f, 8.56f, 8.58f, 8.60f, 
    8.62f, 8.65f, 8.67f, 8.69f, 8.71f, 8.74f, 8.76f, 8.78f, 
    8.80f, 8.83f, 8.85f, 8.87f, 8.90f, 8.92f, 8.94f, 8.96f, 
    8.99f, 9.01f, 9.03f, 9.05f, 9.08f, 9.10f, 9.12f, 9.14f, 
    9.17f, 9.19f, 9.21f, 9.23f, 9.26f, 9.28f, 9.30f, 9.32f, 
    9.35f, 9.37f, 9.39f, 9.41f, 9.44f, 9.46f, 9.48f, 9.50f, 
    9.53f, 9.55f, 9.57f, 9.59f, 9.62f, 9.64f, 9.66f, 9.68f, 
    9.71f, 9.73f, 9.75f, 9.77f, 9.80f, 9.82f, 9.84f, 9.86f, 
    9.89f, 9.91f, 9.93f, 9.95f, 9.98f, 10.00f, 10.02f, 10.04f, 
    10.06f, 10.09f, 10.11f, 10.13f, 10.15f, 10.18f, 10.20f, 10.22f, 
    10.24f, 10.27f, 10.29f, 10.31f, 10.33f, 10.36f, 10.38f, 10.40f, 
    10.42f, 10.44f, 10.47f, 10.49f, 10.51f, 10.53f, 10.56f, 10.58f, 
    10.60f, 10.62f, 10.64f, 10.67f, 10.69f, 10.71f, 10.73f, 10.76f, 
    10.78f, 10.80f, 10.82f, 10.84f, 10.87f, 10.89f, 10.91f, 10.93f, 
    10.96f, 10.98f, 11.00f, 11.02f, 11.04f, 11.07f, 11.09f, 11.11f, 
    11.13f, 11.15f, 11.18f, 11.20f, 11.22f, 11.24f, 11.27f, 11.29f, 
    11.31f, 11.33f, 11.35f, 11.38f, 11.40f, 11.42f, 11.44f, 11.46f, 
    11.49f, 11.51f, 11.53f, 11.55f, 11.57f, 11.60f, 11.62f, 11.64f, 
    11.66f, 11.69f, 11.71f, 11.73f, 11.75f, 11.77f, 11.80f, 11.82f, 
    11.84f, 11.86f, 11.88f, 11.91f, 11.93f, 11.95f, 11.97f, 11.99f, 
    12.02f, 12.04f, 12.06f, 12.08f, 12.10f, 12.13f, 12.15f, 12.17f, 
    12.19f, 12.21f, 12.24f, 12.26f, 12.28f, 12.30f, 12.32f, 12.34f, 
    12.37f, 12.39f, 12.41f, 12.43f, 12.45f, 12.48f, 12.50f, 12.52f, 
    12.54f, 12.56f, 12.59f, 12.61f, 12.63f, 12.65f, 12.67f, 12.70f, 
    12.72f, 12.74f, 12.76f, 12.78f, 12.81f, 12.83f, 12.85f, 12.87f, 
    12.89f, 12.91f, 12.94f, 12.96f, 12.98f, 13.00f, 13.02f, 13.05f, 
    13.07f, 13.09f, 13.11f, 13.13f, 13.15f, 13.18f, 13.20f, 13.22f, 
    13.24f, 13.26f, 13.29f, 13.31f, 13.33f, 13.35f, 13.37f, 13.39f, 
    13.42f, 13.44f, 13.46f, 13.48f, 13.50f, 13.53f, 13.55f, 13.57f, 
    13.59f, 13.61f, 13.63f, 13.66f, 13.68f, 13.70f, 13.72f, 13.74f, 
    13.77f, 13.79f, 13.81f, 13.83f, 13.85f, 13.87f, 13.90f, 13.92f, 
    13.94f, 13.96f, 13.98f, 14.00f, 14.03f, 14.05f, 14.07f, 14.09f, 
    14.11f, 14.13f, 14.16f, 14.18f, 14.20f, 14.22f, 14.24f, 14.26f, 
    14.29f, 14.31f, 14.33f, 14.35f, 14.37f, 14.39f, 14.42f, 14.44f, 
    14.46f, 14.48f, 14.50f, 14.53f, 14.55f, 14.57f, 14.59f, 14.61f, 
    14.63f, 14.66f, 14.68f, 14.70f, 14.72f, 14.74f, 14.76f, 14.79f, 
    14.81f, 14.83f, 14.85f, 14.87f, 14.89f, 14.92f, 14.94f, 14.96f, 
    14.98f, 15.00f, 15.02f, 15.04f, 15.07f, 15.09f, 15.11f, 15.13f, 
    15.15f, 15.17f, 15.20f, 15.22f, 15.24f, 15.26f, 15.28f, 15.30f, 
    15.33f, 15.35f, 15.37f, 15.39f, 15.41f, 15.43f, 15.46f, 15.48f, 
    15.50f, 15.52f, 15.54f, 15.56f, 15.59f, 15.61f, 15.63f, 15.65f, 
    15.67f, 15.69f, 15.71f, 15.74f, 15.76f, 15.78f, 15.80f, 15.82f, 
    15.84f, 15.87f, 15.89f, 15.91f, 15.93f, 15.95f, 15.97f, 16.00f, 
    16.02f, 16.04f, 16.06f, 16.08f, 16.10f, 16.12f, 16.15f, 16.17f, 
    16.19f, 16.21f, 16.23f, 16.25f, 16.28f, 16.30f, 16.32f, 16.34f, 
    16.36f, 16.38f, 16.40f, 16.43f, 16.45f, 16.47f, 16.49f, 16.51f, 
    16.53f, 16.56f, 16.58f, 16.60f, 16.62f, 16.64f, 16.66f, 16.68f, 
    16.71f, 16.73f, 16.75f, 16.77f, 16.79f, 16.81f, 16.84f, 16.86f, 
    16.88f, 16.90f, 16.92f, 16.94f, 16.96f, 16.99f, 17.01f, 17.03f, 
    17.05f, 17.07f, 17.09f, 17.12f, 17.14f, 17.16f, 17.18f, 17.20f, 
    17.22f, 17.24f, 17.27f, 17.29f, 17.31f, 17.33f, 17.35f, 17.37f, 
    17.39f, 17.42f, 17.44f, 17.46f, 17.48f, 17.50f, 17.52f, 17.55f, 
    17.57f, 17.59f, 17.61f, 17.63f, 17.65f, 17.67f, 17.70f, 17.72f, 
    17.74f, 17.76f, 17.78f, 17.80f, 17.83f, 17.85f, 17.87f, 17.89f, 
    17.91f, 17.93f, 17.95f, 17.98f, 18.00f, 18.02f, 18.04f, 18.06f, 
    18.08f, 18.10f, 18.13f, 18.15f, 18.17f, 18.19f, 18.21f, 18.23f, 
    18.25f, 18.28f, 18.30f, 18.32f, 18.34f, 18.36f, 18.38f, 18.41f, 
    18.43f, 18.45f, 18.47f, 18.49f, 18.51f, 18.53f, 18.56f, 18.58f, 
    18.60f, 18.62f, 18.64f, 18.66f, 18.68f, 18.71f, 18.73f, 18.75f, 
    18.77f, 18.79f, 18.81f, 18.84f, 18.86f, 18.88f, 18.90f, 18.92f, 
    18.94f, 18.96f, 18.99f, 19.01f, 19.03f, 19.05f, 19.07f, 19.09f, 
    19.11f, 19.14f, 19.16f, 19.18f, 19.20f, 19.22f, 19.24f, 19.27f, 
    19.29f, 19.31f, 19.33f, 19.35f, 19.37f, 19.39f, 19.42f, 19.44f, 
    19.46f, 19.48f, 19.50f, 19.52f, 19.54f, 19.57f, 19.59f, 19.61f, 
    19.63f, 19.65f, 19.67f, 19.70f, 19.72f, 19.74f, 19.76f, 19.78f, 
    19.80f, 19.82f, 19.85f, 19.87f, 19.89f, 19.91f, 19.93f, 19.95f, 
    19.98f, 20.00f, 20.02f, 20.04f, 20.06f, 20.08f, 20.10f, 20.13f, 
    20.15f, 20.17f, 20.19f, 20.21f, 20.23f, 20.26f, 20.28f, 20.30f, 
    20.32f, 20.34f, 20.36f, 20.38f, 20.41f, 20.43f, 20.45f, 20.47f, 
    20.49f, 20.51f, 20.54f, 20.56f, 20.58f, 20.60f, 20.62f, 20.64f, 
    20.66f, 20.69f, 20.71f, 20.73f, 20.75f, 20.77f, 20.79f, 20.82f, 
    20.84f, 20.86f, 20.88f, 20.90f, 20.92f, 20.94f, 20.97f, 20.99f, 
    21.01f, 21.03f, 21.05f, 21.07f, 21.10f, 21.12f, 21.14f, 21.16f, 
    21.18f, 21.20f, 21.23f, 21.25f, 21.27f, 21.29f, 21.31f, 21.33f, 
    21.35f, 21.38f, 21.40f, 21.42f, 21.44f, 21.46f, 21.48f, 21.51f, 
    21.53f, 21.55f, 21.57f, 21.59f, 21.61f, 21.64f, 21.66f, 21.68f, 
    21.70f, 21.72f, 21.74f, 21.77f, 21.79f, 21.81f, 21.83f, 21.85f, 
    21.87f, 21.90f, 21.92f, 21.94f, 21.96f, 21.98f, 22.00f, 22.02f, 
    22.05f, 22.07f, 22.09f, 22.11f, 22.13f, 22.15f, 22.18f, 22.20f, 
    22.22f, 22.24f, 22.26f, 22.28f, 22.31f, 22.33f, 22.35f, 22.37f, 
    22.39f, 22.41f, 22.44f, 22.46f, 22.48f, 22.50f, 22.52f, 22.54f, 
    22.57f, 22.59f, 22.61f, 22.63f, 22.65f, 22.68f, 22.70f, 22.72f, 
    22.74f, 22.76f, 22.78f, 22.81f, 22.83f, 22.85f, 22.87f, 22.89f, 
    22.91f, 22.94f, 22.96f, 22.98f, 23.00f, 23.02f, 23.04f, 23.07f, 
    23.09f, 23.11f, 23.13f, 23.15f, 23.17f, 23.20f, 23.22f, 23.24f, 
    23.26f, 23.28f, 23.31f, 23.33f, 23.35f, 23.37f, 23.39f, 23.41f, 
    23.44f, 23.46f, 23.48f, 23.50f, 23.52f, 23.54f, 23.57f, 23.59f, 
    23.61f, 23.63f, 23.65f, 23.68f, 23.70f, 23.72f, 23.74f, 23.76f, 
    23.78f, 23.81f, 23.83f, 23.85f, 23.87f, 23.89f, 23.92f, 23.94f, 
    23.96f, 23.98f, 24.00f, 24.02f, 24.05f, 24.07f, 24.09f, 24.11f, 
    24.13f, 24.16f, 24.18f, 24.20f, 24.22f, 24.24f, 24.27f, 24.29f, 
    24.31f, 24.33f, 24.35f, 24.37f, 24.40f, 24.42f, 24.44f, 24.46f, 
    24.48f, 24.51f, 24.53f, 24.55f, 24.57f, 24.59f, 24.62f, 24.64f, 
    24.66f, 24.68f, 24.70f, 24.73f, 24.75f, 24.77f, 24.79f, 24.81f, 
    24.84f, 24.86f, 24.88f, 24.90f, 24.92f, 24.95f, 24.97f, 24.99f, 
    25.01f, 25.03f, 25.05f, 25.08f, 25.10f, 25.12f, 25.14f, 25.16f, 
    25.19f, 25.21f, 25.23f, 25.25f, 25.28f, 25.30f, 25.32f, 25.34f, 
    25.36f, 25.39f, 25.41f, 25.43f, 25.45f, 25.47f, 25.50f, 25.52f, 
    25.54f, 25.56f, 25.58f, 25.61f, 25.63f, 25.65f, 25.67f, 25.69f, 
    25.72f, 25.74f, 25.76f, 25.78f, 25.80f, 25.83f, 25.85f, 25.87f, 
    25.89f, 25.92f, 25.94f, 25.96f, 25.98f, 26.00f, 26.03f, 26.05f, 
    26.07f, 26.09f, 26.11f, 26.14f, 26.16f, 26.18f, 26.20f, 26.23f, 
    26.25f, 26.27f, 26.29f, 26.31f, 26.34f, 26.36f, 26.38f, 26.40f, 
    26.43f, 26.45f, 26.47f, 26.49f, 26.51f, 26.54f, 26.56f, 26.58f, 
    26.60f, 26.63f, 26.65f, 26.67f, 26.69f, 26.71f, 26.74f, 26.76f, 
    26.78f, 26.80f, 26.83f, 26.85f, 26.87f, 26.89f, 26.91f, 26.94f, 
    26.96f, 26.98f, 27.00f, 27.03f, 27.05f, 27.07f, 27.09f, 27.12f, 
    27.14f, 27.16f, 27.18f, 27.21f, 27.23f, 27.25f, 27.27f, 27.29f, 
    27.32f, 27.34f, 27.36f, 27.38f, 27.41f, 27.43f, 27.45f, 27.47f, 
    27.50f, 27.52f, 27.54f, 27.56f, 27.59f, 27.61f, 27.63f, 27.65f, 
    27.68f, 27.70f, 27.72f, 27.74f, 27.77f, 27.79f, 27.81f, 27.83f, 
    27.86f, 27.88f, 27.90f, 27.92f, 27.95f, 27.97f, 27.99f, 28.01f, 
    28.04f, 28.06f, 28.08f, 28.10f, 28.13f, 28.15f, 28.17f, 28.19f, 
    28.22f, 28.24f, 28.26f, 28.28f, 28.31f, 28.33f, 28.35f, 28.37f, 
    28.40f, 28.42f, 28.44f, 28.46f, 28.49f, 28.51f, 28.53f, 28.56f, 
    28.58f, 28.60f, 28.62f, 28.65f, 28.67f, 28.69f, 28.71f, 28.74f, 
    28.76f, 28.78f, 28.80f, 28.83f, 28.85f, 28.87f, 28.90f, 28.92f, 
    28.94f, 28.96f, 28.99f, 29.01f, 29.03f, 29.05f, 29.08f, 29.10f, 
    29.12f, 29.15f, 29.17f, 29.19f, 29.21f, 29.24f, 29.26f, 29.28f, 
    29.31f, 29.33f, 29.35f, 29.37f, 29.40f, 29.42f, 29.44f, 29.47f, 
    29.49f, 29.51f, 29.53f, 29.56f, 29.58f, 29.60f, 29.63f, 29.65f, 
    29.67f, 29.69f, 29.72f, 29.74f, 29.76f, 29.79f, 29.81f, 29.83f, 
    29.85f, 29.88f, 29.90f, 29.92f, 29.95f, 29.97f, 29.99f, 30.02f, 
    30.04f, 30.06f, 30.08f, 30.11f, 30.13f, 30.15f, 30.18f, 30.20f, 
    30.22f, 30.25f, 30.27f, 30.29f, 30.32f, 30.34f, 30.36f, 30.38f, 
    30.41f, 30.43f, 30.45f, 30.48f, 30.50f, 30.52f, 30.55f, 30.57f, 
    30.59f, 30.62f, 30.64f, 30.66f, 30.69f, 30.71f, 30.73f, 30.75f, 
    30.78f, 30.80f, 30.82f, 30.85f, 30.87f, 30.89f, 30.92f, 30.94f, 
    30.96f, 30.99f, 31.01f, 31.03f, 31.06f, 31.08f, 31.10f, 31.13f, 
    31.15f, 31.17f, 31.20f, 31.22f, 31.24f, 31.27f, 31.29f, 31.31f, 
    31.34f, 31.36f, 31.38f, 31.41f, 31.43f, 31.45f, 31.48f, 31.50f, 
    31.52f, 31.55f, 31.57f, 31.59f, 31.62f, 31.64f, 31.66f, 31.69f, 
    31.71f, 31.74f, 31.76f, 31.78f, 31.81f, 31.83f, 31.85f, 31.88f, 
    31.90f, 31.92f, 31.95f, 31.97f, 31.99f, 32.02f, 32.04f, 32.06f, 
    32.09f, 32.11f, 32.14f, 32.16f, 32.18f, 32.21f, 32.23f, 32.25f, 
    32.28f, 32.30f, 32.32f, 32.35f, 32.37f, 32.40f, 32.42f, 32.44f, 
    32.47f, 32.49f, 32.51f, 32.54f, 32.56f, 32.59f, 32.61f, 32.63f, 
    32.66f, 32.68f, 32.70f, 32.73f, 32.75f, 32.78f, 32.80f, 32.82f, 
    32.85f, 32.87f, 32.89f, 32.92f, 32.94f, 32.97f, 32.99f, 33.01f, 
    33.04f, 33.06f, 33.09f, 33.11f, 33.13f, 33.16f, 33.18f, 33.21f, 
    33.23f, 33.25f, 33.28f, 33.30f, 33.33f, 33.35f, 33.37f, 33.40f, 
    33.42f, 33.45f, 33.47f, 33.49f, 33.52f, 33.54f, 33.57f, 33.59f, 
    33.61f, 33.64f, 33.66f, 33.69f, 33.71f, 33.73f, 33.76f, 33.78f, 
    33.81f, 33.83f, 33.85f, 33.88f, 33.90f, 33.93f, 33.95f, 33.98f, 
    34.00f, 34.02f, 34.05f, 34.07f, 34.10f, 34.12f, 34.15f, 34.17f, 
    34.19f, 34.22f, 34.24f, 34.27f, 34.29f, 34.32f, 34.34f, 34.36f, 
    34.39f, 34.41f, 34.44f, 34.46f, 34.49f, 34.51f, 34.53f, 34.56f, 
    34.58f, 34.61f, 34.63f, 34.66f, 34.68f, 34.71f, 34.73f, 34.75f, 
    34.78f, 34.80f, 34.83f, 34.85f, 34.88f, 34.90f, 34.93f, 34.95f, 
    34.98f, 35.00f, 35.02f, 35.05f, 35.07f, 35.10f, 35.12f, 35.15f, 
    35.17f, 35.20f, 35.22f, 35.25f, 35.27f, 35.30f, 35.32f, 35.35f, 
    35.37f, 35.39f, 35.42f, 35.44f, 35.47f, 35.49f, 35.52f, 35.54f, 
    35.57f, 35.59f, 35.62f, 35.64f, 35.67f, 35.69f, 35.72f, 35.74f, 
    35.77f, 35.79f, 35.82f, 35.84f, 35.87f, 35.89f, 35.92f, 35.94f, 
    35.97f, 35.99f, 36.02f, 36.04f, 36.07f, 36.09f, 36.12f, 36.14f, 
    36.17f, 36.19f, 36.22f, 36.24f, 36.27f, 36.29f, 36.32f, 36.34f, 
    36.37f, 36.39f, 36.42f, 36.44f, 36.47f, 36.49f, 36.52f, 36.54f, 
    36.57f, 36.59f, 36.62f, 36.64f, 36.67f, 36.69f, 36.72f, 36.74f, 
    36.77f, 36.80f, 36.82f, 36.85f, 36.87f, 36.90f, 36.92f, 36.95f, 
    36.97f, 37.00f, 37.02f, 37.05f, 37.07f, 37.10f, 37.13f, 37.15f, 
    37.18f, 37.20f, 37.23f, 37.25f, 37.28f, 37.30f, 37.33f, 37.35f, 
    37.38f, 37.41f, 37.43f, 37.46f, 37.48f, 37.51f, 37.53f, 37.56f, 
    37.58f, 37.61f, 37.64f, 37.66f, 37.69f, 37.71f, 37.74f, 37.76f, 
    37.79f, 37.82f, 37.84f, 37.87f, 37.89f, 37.92f, 37.94f, 37.97f, 
    38.00f, 38.02f, 38.05f, 38.07f, 38.10f, 38.13f, 38.15f, 38.18f, 
    38.20f, 38.23f, 38.26f, 38.28f, 38.31f, 38.33f, 38.36f, 38.39f, 
    38.41f, 38.44f, 38.46f, 38.49f, 38.52f, 38.54f, 38.57f, 38.59f, 
    38.62f, 38.65f, 38.67f, 38.70f, 38.72f, 38.75f, 38.78f, 38.80f, 
    38.83f, 38.86f, 38.88f, 38.91f, 38.93f, 38.96f, 38.99f, 39.01f, 
    39.04f, 39.07f, 39.09f, 39.12f, 39.14f, 39.17f, 39.20f, 39.22f, 
    39.25f, 39.28f, 39.30f, 39.33f, 39.36f, 39.38f, 39.41f, 39.43f, 
    39.46f, 39.49f, 39.51f, 39.54f, 39.57f, 39.59f, 39.62f, 39.65f, 
    39.67f, 39.70f, 39.73f, 39.75f, 39.78f, 39.81f, 39.83f, 39.86f, 
    39.89f, 39.91f, 39.94f, 39.97f, 39.99f, 40.02f, 40.05f, 40.07f, 
    40.10f, 40.13f, 40.16f, 40.18f, 40.21f, 40.24f, 40.26f, 40.29f, 
    40.32f, 40.34f, 40.37f, 40.40f, 40.42f, 40.45f, 40.48f, 40.51f, 
    40.53f, 40.56f, 40.59f, 40.61f, 40.64f, 40.67f, 40.69f, 40.72f, 
    40.75f, 40.78f, 40.80f, 40.83f, 40.86f, 40.89f, 40.91f, 40.94f, 
    40.97f, 40.99f, 41.02f, 41.05f, 41.08f, 41.10f, 41.13f, 41.16f, 
    41.19f, 41.21f, 41.24f, 41.27f, 41.30f, 41.32f, 41.35f, 41.38f, 
    41.41f, 41.43f, 41.46f, 41.49f, 41.52f, 41.54f, 41.57f, 41.60f, 
    41.63f, 41.65f, 41.68f, 41.71f, 41.74f, 41.76f, 41.79f, 41.82f, 
    41.85f, 41.87f, 41.90f, 41.93f, 41.96f, 41.99f, 42.01f, 42.04f, 
    42.07f, 42.10f, 42.13f, 42.15f, 42.18f, 42.21f, 42.24f, 42.27f, 
    42.29f, 42.32f, 42.35f, 42.38f, 42.41f, 42.43f, 42.46f, 42.49f, 
    42.52f, 42.55f, 42.57f, 42.60f, 42.63f, 42.66f, 42.69f, 42.71f, 
    42.74f, 42.77f, 42.80f, 42.83f, 42.86f, 42.88f, 42.91f, 42.94f, 
    42.97f, 43.00f, 43.03f, 43.05f, 43.08f, 43.11f, 43.14f, 43.17f, 
    43.20f, 43.23f, 43.25f, 43.28f, 43.31f, 43.34f, 43.37f, 43.40f, 
    43.43f, 43.45f, 43.48f, 43.51f, 43.54f, 43.57f, 43.60f, 43.63f, 
    43.66f, 43.68f, 43.71f, 43.74f, 43.77f, 43.80f, 43.83f, 43.86f, 
    43.89f, 43.92f, 43.94f, 43.97f, 44.00f, 44.03f, 44.06f, 44.09f, 
    44.12f, 44.15f, 44.18f, 44.21f, 44.24f, 44.26f, 44.29f, 44.32f, 
    44.35f, 44.38f, 44.41f, 44.44f, 44.47f, 44.50f, 44.53f, 44.56f, 
    44.59f, 44.62f, 44.64f, 44.67f, 44.70f, 44.73f, 44.76f, 44.79f, 
    44.82f, 44.85f, 44.88f, 44.91f, 44.94f, 44.97f, 45.00f, 45.03f, 
    45.06f, 45.09f, 45.12f, 45.15f, 45.18f, 45.21f, 45.24f, 45.27f, 
    45.30f, 45.33f, 45.36f, 45.39f, 45.42f, 45.45f, 45.48f, 45.51f, 
    45.54f, 45.57f, 45.60f, 45.63f, 45.66f, 45.69f, 45.72f, 45.75f, 
    45.78f, 45.81f, 45.84f, 45.87f, 45.90f, 45.93f, 45.96f, 45.99f, 
    46.02f, 46.05f, 46.08f, 46.11f, 46.14f, 46.17f, 46.20f, 46.23f, 
    46.26f, 46.29f, 46.32f, 46.35f, 46.38f, 46.41f, 46.44f, 46.47f, 
    46.51f, 46.54f, 46.57f, 46.60f, 46.63f, 46.66f, 46.69f, 46.72f, 
    46.75f, 46.78f, 46.81f, 46.84f, 46.87f, 46.91f, 46.94f, 46.97f, 
    47.00f, 47.03f, 47.06f, 47.09f, 47.12f, 47.15f, 47.18f, 47.22f, 
    47.25f, 47.28f, 47.31f, 47.34f, 47.37f, 47.40f, 47.43f, 47.46f, 
    47.50f, 47.53f, 47.56f, 47.59f, 47.62f, 47.65f, 47.68f, 47.72f, 
    47.75f, 47.78f, 47.81f, 47.84f, 47.87f, 47.91f, 47.94f, 47.97f, 
    48.00f, 48.03f, 48.06f, 48.10f, 48.13f, 48.16f, 48.19f, 48.22f, 
    48.25f, 48.29f, 48.32f, 48.35f, 48.38f, 48.41f, 48.45f, 48.48f, 
    48.51f, 48.54f, 48.57f, 48.61f, 48.64f, 48.67f, 48.70f, 48.74f, 
    48.77f, 48.80f, 48.83f, 48.86f, 48.90f, 48.93f, 48.96f, 48.99f, 
    49.03f, 49.06f, 49.09f, 49.12f, 49.16f, 49.19f, 49.22f, 49.25f, 
    49.29f, 49.32f, 49.35f, 49.39f, 49.42f, 49.45f, 49.48f, 49.52f, 
    49.55f, 49.58f, 49.61f, 49.65f, 49.68f, 49.71f, 49.75f, 49.78f, 
    49.81f, 49.85f, 49.88f, 49.91f, 49.95f, 49.98f, 50.01f, 50.04f, 
    50.08f, 50.11f, 50.14f, 50.18f, 50.21f, 50.24f, 50.28f, 50.31f, 
    50.35f, 50.38f, 50.41f, 50.45f, 50.48f, 50.51f, 50.55f, 50.58f, 
    50.61f, 50.65f, 50.68f, 50.72f, 50.75f, 50.78f, 50.82f, 50.85f, 
    50.88f, 50.92f, 50.95f, 50.99f, 51.02f, 51.05f, 51.09f, 51.12f, 
    51.16f, 51.19f, 51.23f, 51.26f, 51.29f, 51.33f, 51.36f, 51.40f, 
    51.43f, 51.47f, 51.50f, 51.53f, 51.57f, 51.60f, 51.64f, 51.67f, 
    51.71f, 51.74f, 51.78f, 51.81f, 51.85f, 51.88f, 51.92f, 51.95f, 
    51.99f, 52.02f, 52.06f, 52.09f, 52.13f, 52.16f, 52.20f, 52.23f, 
    52.27f, 52.30f, 52.34f, 52.37f, 52.41f, 52.44f, 52.48f, 52.51f, 
    52.55f, 52.58f, 52.62f, 52.65f, 52.69f, 52.72f, 52.76f, 52.80f, 
    52.83f, 52.87f, 52.90f, 52.94f, 52.97f, 53.01f, 53.05f, 53.08f, 
    53.12f, 53.15f, 53.19f, 53.23f, 53.26f, 53.30f, 53.33f, 53.37f, 
    53.41f, 53.44f, 53.48f, 53.51f, 53.55f, 53.59f, 53.62f, 53.66f, 
    53.70f, 53.73f, 53.77f, 53.81f, 53.84f, 53.88f, 53.91f, 53.95f, 
    53.99f, 54.02f, 54.06f, 54.10f, 54.14f, 54.17f, 54.21f, 54.25f, 
    54.28f, 54.32f, 54.36f, 54.39f, 54.43f, 54.47f, 54.50f, 54.54f, 
    54.58f, 54.62f, 54.65f, 54.69f, 54.73f, 54.77f, 54.80f, 54.84f, 
    54.88f, 54.92f, 54.95f, 54.99f, 55.03f, 55.07f, 55.10f, 55.14f, 
    55.18f, 55.22f, 55.26f, 55.29f, 55.33f, 55.37f, 55.41f, 55.45f, 
    55.48f, 55.52f, 55.56f, 55.60f, 55.64f, 55.67f, 55.71f, 55.75f, 
    55.79f, 55.83f, 55.87f, 55.91f, 55.94f, 55.98f, 56.02f, 56.06f, 
    56.10f, 56.14f, 56.18f, 56.21f, 56.25f, 56.29f, 56.33f, 56.37f, 
    56.41f, 56.45f, 56.49f, 56.53f, 56.57f, 56.61f, 56.64f, 56.68f, 
    56.72f, 56.76f, 56.80f, 56.84f, 56.88f, 56.92f, 56.96f, 57.00f, 
    57.04f, 57.08f, 57.12f, 57.16f, 57.20f, 57.24f, 57.28f, 57.32f, 
    57.36f, 57.40f, 57.44f, 57.48f, 57.52f, 57.56f, 57.60f, 57.64f, 
    57.68f, 57.72f, 57.76f, 57.80f, 57.84f, 57.88f, 57.92f, 57.96f, 
    58.01f, 58.05f, 58.09f, 58.13f, 58.17f, 58.21f, 58.25f, 58.29f, 
    58.33f, 58.37f, 58.42f, 58.46f, 58.50f, 58.54f, 58.58f, 58.62f, 
    58.66f, 58.71f, 58.75f, 58.79f, 58.83f, 58.87f, 58.91f, 58.96f, 
    59.00f, 59.04f, 59.08f, 59.12f, 59.16f, 59.21f, 59.25f, 59.29f, 
    59.33f, 59.38f, 59.42f, 59.46f, 59.50f, 59.55f, 59.59f, 59.63f, 
    59.67f, 59.72f, 59.76f, 59.80f, 59.84f, 59.89f, 59.93f, 59.97f, 
    60.02f, 60.06f, 60.10f, 60.15f, 60.19f, 60.23f, 60.28f, 60.32f, 
    60.36f, 60.41f, 60.45f, 60.49f, 60.54f, 60.58f, 60.62f, 60.67f, 
    60.71f, 60.76f, 60.80f, 60.84f, 60.89f, 60.93f, 60.98f, 61.02f, 
    61.06f, 61.11f, 61.15f, 61.20f, 61.24f, 61.29f, 61.33f, 61.38f, 
    61.42f, 61.47f, 61.51f, 61.55f, 61.60f, 61.64f, 61.69f, 61.73f, 
    61.78f, 61.83f, 61.87f, 61.92f, 61.96f, 62.01f, 62.05f, 62.10f, 
    62.14f, 62.19f, 62.23f, 62.28f, 62.33f, 62.37f, 62.42f, 62.46f, 
    62.51f, 62.56f, 62.60f, 62.65f, 62.70f, 62.74f, 62.79f, 62.83f, 
    62.88f, 62.93f, 62.97f, 63.02f, 63.07f, 63.12f, 63.16f, 63.21f, 
    63.26f, 63.30f, 63.35f, 63.40f, 63.45f, 63.49f, 63.54f, 63.59f, 
    63.64f, 63.68f, 63.73f, 63.78f, 63.83f, 63.87f, 63.92f, 63.97f, 
    64.02f, 64.07f, 64.11f, 64.16f, 64.21f, 64.26f, 64.31f, 64.36f, 
    64.41f, 64.45f, 64.50f, 64.55f, 64.60f, 64.65f, 64.70f, 64.75f, 
    64.80f, 64.85f, 64.90f, 64.94f, 64.99f, 65.04f, 65.09f, 65.14f, 
    65.19f, 65.24f, 65.29f, 65.34f, 65.39f, 65.44f, 65.49f, 65.54f, 
    65.59f, 65.64f, 65.69f, 65.74f, 65.80f, 65.85f, 65.90f, 65.95f, 
    66.00f, 66.05f, 66.10f, 66.15f, 66.20f, 66.25f, 66.31f, 66.36f, 
    66.41f, 66.46f, 66.51f, 66.56f, 66.62f, 66.67f, 66.72f, 66.77f, 
    66.82f, 66.88f, 66.93f, 66.98f, 67.03f, 67.08f, 67.14f, 67.19f, 
    67.24f, 67.30f, 67.35f, 67.40f, 67.45f, 67.51f, 67.56f, 67.61f, 
    67.67f, 67.72f, 67.77f, 67.83f, 67.88f, 67.94f, 67.99f, 68.04f, 
    68.10f, 68.15f, 68.21f, 68.26f, 68.32f, 68.37f, 68.42f, 68.48f, 
    68.53f, 68.59f, 68.64f, 68.70f, 68.75f, 68.81f, 68.86f, 68.92f, 
    68.97f, 69.03f, 69.09f, 69.14f, 69.20f, 69.25f, 69.31f, 69.37f, 
    69.42f, 69.48f, 69.53f, 69.59f, 69.65f, 69.70f, 69.76f, 69.82f, 
    69.87f, 69.93f, 69.99f, 70.05f, 70.10f, 70.16f, 70.22f, 70.28f, 
    70.33f, 70.39f, 70.45f, 70.51f, 70.57f, 70.62f, 70.68f, 70.74f, 
    70.80f, 70.86f, 70.92f, 70.98f, 71.03f, 71.09f, 71.15f, 71.21f, 
    71.27f, 71.33f, 71.39f, 71.45f, 71.51f, 71.57f, 71.63f, 71.69f, 
    71.75f, 71.81f, 71.87f, 71.93f, 71.99f, 72.05f, 72.11f, 72.17f, 
    72.23f, 72.30f, 72.36f, 72.42f, 72.48f, 72.54f, 72.60f, 72.67f, 
    72.73f, 72.79f, 72.85f, 72.91f, 72.98f, 73.04f, 73.10f, 73.16f, 
    73.23f, 73.29f, 73.35f, 73.42f, 73.48f, 73.54f, 73.61f, 73.67f, 
    73.73f, 73.80f, 73.86f, 73.93f, 73.99f, 74.06f, 74.12f, 74.18f, 
    74.25f, 74.31f, 74.38f, 74.44f, 74.51f, 74.58f, 74.64f, 74.71f, 
    74.77f, 74.84f, 74.90f, 74.97f, 75.04f, 75.10f, 75.17f, 75.24f, 
    75.30f, 75.37f, 75.44f, 75.50f, 75.57f, 75.64f, 75.71f, 75.78f, 
    75.84f, 75.91f, 75.98f, 76.05f, 76.12f, 76.19f, 76.25f, 76.32f, 
    76.39f, 76.46f, 76.53f, 76.60f, 76.67f, 76.74f, 76.81f, 76.88f, 
    76.95f, 77.02f, 77.09f, 77.16f, 77.23f, 77.30f, 77.37f, 77.45f, 
    77.52f, 77.59f, 77.66f, 77.73f, 77.80f, 77.88f, 77.95f, 78.02f, 
    78.09f, 78.17f, 78.24f, 78.31f, 78.39f, 78.46f, 78.53f, 78.61f, 
    78.68f, 78.76f, 78.83f, 78.91f, 78.98f, 79.05f, 79.13f, 79.20f, 
    79.28f, 79.36f, 79.43f, 79.51f, 79.58f, 79.66f, 79.74f, 79.81f, 
    79.89f, 79.97f, 80.04f, 80.12f, 80.20f, 80.28f, 80.35f, 80.43f, 
    80.51f, 80.59f, 80.67f, 80.75f, 80.82f, 80.90f, 80.98f, 81.06f, 
    81.14f, 81.22f, 81.30f, 81.38f, 81.46f, 81.54f, 81.62f, 81.71f, 
    81.79f, 81.87f, 81.95f, 82.03f, 82.11f, 82.20f, 82.28f, 82.36f, 
    82.44f, 82.53f, 82.61f, 82.69f, 82.78f, 82.86f, 82.95f, 83.03f, 
    83.12f, 83.20f, 83.29f, 83.37f, 83.46f, 83.54f, 83.63f, 83.71f, 
    83.80f, 83.89f, 83.97f, 84.06f, 84.15f, 84.24f, 84.32f, 84.41f, 
    84.50f, 84.59f, 84.68f, 84.77f, 84.85f, 84.94f, 85.03f, 85.12f, 
    85.21f, 85.30f, 85.39f, 85.49f, 85.58f, 85.67f, 85.76f, 85.85f, 
    85.94f, 86.04f, 86.13f, 86.22f, 86.32f, 86.41f, 86.50f, 86.60f, 
    86.69f, 86.79f, 86.88f, 86.98f, 87.07f, 87.17f, 87.26f, 87.36f, 
    87.46f, 87.55f, 87.65f, 87.75f, 87.84f, 87.94f, 88.04f, 88.14f, 
    88.24f, 88.34f, 88.44f, 88.54f, 88.64f, 88.74f, 88.84f, 88.94f, 
    89.04f, 89.14f, 89.24f, 89.34f, 89.45f, 89.55f, 89.65f, 89.76f, 
    89.86f, 89.96f, 90.07f, 90.17f, 90.28f, 90.38f, 90.49f, 90.60f, 
    90.70f, 90.81f, 90.92f, 91.02f, 91.13f, 91.24f, 91.35f, 91.46f, 
    91.57f, 91.68f, 91.79f, 91.90f, 92.01f, 92.12f, 92.23f, 92.34f, 
    92.45f, 92.57f, 92.68f, 92.79f, 92.91f, 93.02f, 93.14f, 93.25f, 
    93.37f, 93.48f, 93.60f, 93.72f, 93.83f, 93.95f, 94.07f, 94.19f, 
    94.30f, 94.42f, 94.54f, 94.66f, 94.78f, 94.90f, 95.03f, 95.15f, 
    95.27f, 95.39f, 95.52f, 95.64f, 95.76f, 95.89f, 96.01f, 96.14f, 
    96.26f, 96.39f, 96.52f, 96.65f, 96.77f, 96.90f, 97.03f, 97.16f, 
    97.29f, 97.42f, 97.55f, 97.68f, 97.82f, 97.95f, 98.08f, 98.21f, 
    98.35f, 98.48f, 98.62f, 98.75f, 98.89f, 99.03f, 99.16f, 99.30f, 
    99.44f, 99.58f, 99.72f, 99.86f, 100.00f, 100.14f, 100.29f, 100.43f, 
    100.57f, 100.72f, 100.86f, 101.01f, 101.15f, 101.30f, 101.44f, 101.59f, 
    101.74f, 101.89f, 102.04f, 102.19f, 102.34f, 102.49f, 102.65f, 102.80f, 
    102.95f, 103.11f, 103.26f, 103.42f, 103.58f, 103.73f, 103.89f, 104.05f, 
    104.21f, 104.37f, 104.53f, 104.69f, 104.86f, 105.02f, 105.19f, 105.35f, 
    105.52f, 105.68f, 105.85f, 106.02f, 106.19f, 106.36f, 106.53f, 106.70f, 
    106.88f, 107.05f, 107.23f, 107.40f, 107.58f, 107.75f, 107.93f, 108.11f, 
    108.29f, 108.47f, 108.66f, 108.84f, 109.02f, 109.21f, 109.40f, 109.58f, 
    109.77f, 109.96f, 110.15f, 110.34f, 110.54f, 110.73f, 110.92f, 111.12f, 
    111.32f, 111.52f, 111.71f, 111.92f, 112.12f, 112.32f, 112.52f, 112.73f, 
    112.94f, 113.14f, 113.35f, 113.56f, 113.78f, 113.99f, 114.20f, 114.42f, 
    114.64f, 114.85f, 115.07f, 115.29f, 115.52f, 115.74f, 115.97f, 116.19f, 
    116.42f, 116.65f, 116.88f, 117.12f, 117.35f, 117.59f, 117.83f, 118.07f, 
    118.31f, 118.55f, 118.80f, 119.04f, 119.29f, 119.54f, 119.79f, 120.05f, 
    120.30f, 120.56f, 120.82f, 121.08f, 121.34f, 121.61f, 121.88f, 122.14f, 
    122.42f, 122.69f, 122.96f, 123.24f, 123.52f, 123.80f, 124.09f, 124.38f, 
    124.67f, 124.96f, 125.25f, 125.55f, 125.85f, 126.15f, 126.45f, 126.76f, 
    127.07f, 127.38f, 127.69f, 128.01f, 128.33f, 128.66f, 128.98f, 129.31f, 
    129.64f, 129.98f, 130.32f, 130.66f, 131.00f, 131.35f, 131.70f, 132.06f, 
    132.42f, 132.78f, 133.14f, 133.51f, 133.89f, 134.26f, 134.65f, 135.03f, 
    135.42f, 135.81f, 136.21f, 136.61f, 137.02f, 137.43f, 137.85f, 138.27f, 
    138.69f, 139.12f, 139.56f, 140.00f, 140.44f, 140.89f, 141.35f, 141.81f, 
    142.28f, 142.76f, 143.23f, 143.72f, 144.21f, 144.71f, 145.22f, 145.73f, 
    146.25f, 146.78f, 147.31f, 147.85f, 148.40f, 148.96f, 149.52f, 150.00f, 
    150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 
    150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 
    150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 
    150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 
    150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 
    150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 
    150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 
    150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 
    150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 
    150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 150.00f, 150.00f
};

 // Define the reference voltage (in volts)
#define V_REF 3.3


// ring buffer code
#define RING_BUFF_SIZE 5

struct Temp_Data {
    float latest_temps[RING_BUFF_SIZE];
    float latest_avg_temp;
    int idx;
};

struct Temp_Data temp_data;

void insert_temp(struct Temp_Data *temp_data, float value) {

    temp_data->latest_temps[temp_data->idx++] = value;

    if (temp_data->idx >= RING_BUFF_SIZE) temp_data->idx = 0;

    float sum = 0;
    for (int i = 0; i < RING_BUFF_SIZE; i++) {
        sum += temp_data->latest_temps[i];
    }

    temp_data->latest_avg_temp = (sum / RING_BUFF_SIZE);
}

float get_latest_avg_temperature() {
    return temp_data.latest_avg_temp;
}
 

// Function to convert ADC value to voltage
double adcToVoltage(uint16_t adcValue, uint16_t adcMaxValue) {
    return (adcValue / (double)adcMaxValue) * V_REF;
}

uint16_t voltageToADC(double voltage, uint16_t adcMaxValue) {
    return (uint16_t)((voltage / V_REF) * adcMaxValue);
}

 
 // Function declarations
 void temp_sense_task_entry(void *pvParameter);

 void adc_init() {
    adc1_config_width(width);
    adc1_config_channel_atten(channel, atten);
    
    // Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(unit, atten, width, DEFAULT_VREF, adc_chars);
}

 void temp_sense_task_entry(void *pvParameter)
 {

    adc_init();
 
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000)); // delay to prevent task hogging


        uint32_t raw_adc =  adc1_get_raw(ADC1_CHANNEL_4);

        double voltage = adcToVoltage(raw_adc, 4095U);

        // slightly pad the voltage and adc
        if (voltage < 2.65f) voltage += VOLTAGE_PADDING;

        uint32_t padded_adc = voltageToADC(voltage, 4095U);


        float temp = ADC_TO_TEMP_LUT[padded_adc];
        insert_temp(&temp_data, temp);

        ESP_LOGI(TEMP_SENSE_TAG, "raw_adc read %lu, padded_adc read %lu, voltage: %.5f, temp: %f", raw_adc, padded_adc, voltage, temp);

    }
 }