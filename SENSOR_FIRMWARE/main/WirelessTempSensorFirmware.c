#include "driver/uart.h"
#include "sdkconfig.h"

#include "esp_console.h"
#include "sdkconfig.h"
#include "threads/ble_client.c"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"


// Function declarations
// esp_err_t configure_stdin_stdout(void);
void ble_task_entry(void *pvParameter);

void app_main(void)
{
    // Create the ble client task
    xTaskCreate(&ble_task_entry, "ble_client_task", 4096, NULL, tskIDLE_PRIORITY, NULL);

    // vTaskStartScheduler();
    // while(1) {
    //     ;
    // } // should never reach here: 
    // TODO: see this: https://www.freertos.org/a00132.html
}