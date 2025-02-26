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

static const char *STATE_MACHINE_TAG = "STATE_MACHINE";

#define ESP_INTR_FLAG_DEFAULT 0
#define GPIO_INPUT_IO_0 33

void state_machine_task_entry(void *pvParameter);


typedef enum {
    BOOTUP,
    PAIRING,
    IDLE,
    ACTUATE,
    FAULT,
    STATE_COUNT
} State;
typedef enum {
    BOOTING,
    PAIRING_REQUEST,
    CONNECTED,
    COMMAND_RECV,
    DONE_ACTUATE,
    ERROR_OCCUR,
    EVENT_COUNT
} Event;

typedef struct {
    State nextState;
    void (*action)(void);
} Transition;

typedef struct {
    State currentState;
} StateMachine;

void enterBootup(void);
void enterPairing(void);
void enterIdle(void);
void enterActuate(void);
void enterFault(void);

// TODO: send acks after every message from rpi

// State transition table
Transition stateTable[STATE_COUNT][EVENT_COUNT] = {

    [BOOTUP][BOOTING] = {PAIRING, NULL},
    [BOOTUP][PAIRING_REQUEST] = {PAIRING, enterPairing},
    [BOOTUP][ERROR_OCCUR] = {FAULT, enterFault},

    [PAIRING][CONNECTED] = {IDLE, enterIdle},
    [PAIRING][ERROR_OCCUR] = {FAULT, enterFault},

    [IDLE][COMMAND_RECV] = {ACTUATE, enterActuate},
    [IDLE][PAIRING_REQUEST] = {PAIRING, enterPairing},
    [IDLE][ERROR_OCCUR] = {FAULT, enterFault},

    [ACTUATE][DONE_ACTUATE]= {ACTUATE, enterIdle},
    [ACTUATE][ERROR_OCCUR]= {FAULT, enterFault},

};

void enterBootup(void) {
    ESP_LOGI(STATE_MACHINE_TAG, "In bootup. Hold user button to start pairing...");
}

void enterPairing(void) {
    ESP_LOGI(STATE_MACHINE_TAG, "In pairing. Starting BLE server task...");

    // Create the ble server task
    xTaskCreate(&ble_server_task_entry, "ble_server_task", 4096, NULL, tskIDLE_PRIORITY, NULL);
}

void enterIdle(void) {
    ESP_LOGI(STATE_MACHINE_TAG, "In Idle starting motor...");

    // Create the motor task
    xTaskCreate(&motor_task_entry, "motor_task", 4096, NULL, 5, NULL);

}

void enterActuate(void) {
    ESP_LOGI(STATE_MACHINE_TAG, "In Actuate...");
}

void enterFault(void) {
    ESP_LOGE(STATE_MACHINE_TAG, "In fault state, shutting all threads...");
    // TODO: shut all threads
}

// Initialize state machine
void initStateMachine(StateMachine *sm, State initialState) {
    sm->currentState = initialState;
    if (stateTable[initialState][0].action) {
        stateTable[initialState][0].action();
    }
}

// Handle an event
void handleEvent(StateMachine *sm, Event event) {
    Transition transition = stateTable[sm->currentState][event];
    if (transition.action) {
        sm->currentState = transition.nextState;
        transition.action();
    } else {
        ESP_LOGE(STATE_MACHINE_TAG, "Invalid event %d in state %d\n", event, sm->currentState);
    }
}

StateMachine sm;

void IRAM_ATTR button_isr_handler(void* arg) {
 
    // update event
    if (sm.currentState == BOOTUP) {
        ESP_LOGI(STATE_MACHINE_TAG, "Sending event: %u", PAIRING_REQUEST);
        handleEvent(&sm, PAIRING_REQUEST);
    }
    else if (sm.currentState == PAIRING) {
        ESP_LOGI(STATE_MACHINE_TAG, "Sending event: %u", CONNECTED);
        handleEvent(&sm, CONNECTED);
    }

}

void state_machine_task_entry(void *pvParameter)
{
    // TODO: get button functionality working...
    // gpio_pad_select_gpio(GPIO_INPUT_IO_0);
    gpio_set_direction(GPIO_INPUT_IO_0, GPIO_MODE_INPUT);
    gpio_set_intr_type(GPIO_INPUT_IO_0, GPIO_INTR_POSEDGE);
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(GPIO_INPUT_IO_0, button_isr_handler, NULL);

    // Start State Machine
    ESP_LOGI(STATE_MACHINE_TAG, "Starting state machine...");

    initStateMachine(&sm, BOOTUP);


    // vTaskDelay(pdMS_TO_TICKS(1000));
    // ESP_LOGI(STATE_MACHINE_TAG, "Sending event: %u", PAIRING_REQUEST);
    // handleEvent(&sm, PAIRING_REQUEST);
    // vTaskDelay(pdMS_TO_TICKS(1000));
    // ESP_LOGI(STATE_MACHINE_TAG, "Sending event: %u", CONNECTED);
    // handleEvent(&sm, CONNECTED);



    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

}