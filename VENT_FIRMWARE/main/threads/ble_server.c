#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <stdlib.h>
// #include "motor.c"

#define MOTOR_CLOSE_PWM 170
#define MOTOR_OPEN_PWM 1050

char *BLE_TAG = "BLE_SERVER";
uint8_t ble_addr_type;
void ble_app_advertise(void);
uint16_t your_read_attr_handle = 0;
uint16_t g_conn_handle;
struct os_mbuf *om;
int rc = 0;
int VentID = 0;

void set_motor_duty(int duty);

float get_latest_avg_temperature();


void send_temperature_update(float temp) {
    // Send temperature packet
    char temp_message[50];
    snprintf(temp_message, sizeof(temp_message), "ID: %d, temp: %.1f", VentID, temp);
    om = ble_hs_mbuf_from_flat(temp_message, strlen(temp_message));
    if (om == NULL) {
        printf("Failed to allocate buffer for temperature notification\n");
    }
    rc = ble_gatts_notify_custom(g_conn_handle, your_read_attr_handle, om);
    printf("Sending notification: %s (result: %d)\n", temp_message, rc);
}

// Write data to ESP32 defined as server
static int device_write(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    // Create a null-terminated string from the received data
    char received_str[ctxt->om->om_len + 1];
    memcpy(received_str, ctxt->om->om_data, ctxt->om->om_len);
    received_str[ctxt->om->om_len] = '\0';
    
    printf("Data from the client: %s\n", received_str);

    // Check if the message starts with "Connected, Vent ID: "
    // This part is only relevant for initial vent setup
    const char* prefix = "Connected, Vent ID: ";
    if (strncmp(received_str, prefix, strlen(prefix)) == 0) {
        // Extract the ventID number, VentID is a global
        /* All packets sent going forward will be prefixed with VentID to tell the Central Hub which vent the
            packet is coming from
        */
        if (sscanf(received_str + strlen(prefix), "%d", &VentID) == 1) {
            printf("Extracted Vent ID: %d\n", VentID);
            
            // Send response using notification
            // TODO: Unable to read message content at the Pi at the moment
             // Send "Accepted" message back to the client
            const char *response = "Connected";
            om = ble_hs_mbuf_from_flat(response, strlen(response));
            if (om == NULL) {
                printf("Failed to allocate buffer for notification\n");
            }

            // TODO: remove later
            printf("ATTR HANDLE SHOULD BE SET!!!!\n");
            rc = ble_gatts_notify_custom(conn_handle, your_read_attr_handle, om);
            printf("Sending notification: %s (result: %d)\n", response, rc);
            g_conn_handle = conn_handle;

            // // // Sleep for 5 seconds
            // vTaskDelay(pdMS_TO_TICKS(5000));

        }
            
    }
    else {
        
        int received_open_percentage = (int) atof(received_str);
        ESP_LOGI(BLE_TAG, "VENT received message: %s, as int: %i", received_str, received_open_percentage);

        
        int desired_motor_pwm = ((MOTOR_OPEN_PWM-MOTOR_CLOSE_PWM)/100.0f)*received_open_percentage + MOTOR_CLOSE_PWM;

        set_motor_duty(desired_motor_pwm);

    }
    return 0;
}

// Read data from ESP32 defined as server
static int device_read(uint16_t con_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    your_read_attr_handle = attr_handle;  // Store the handle
    // os_mbuf_append(ctxt->om, "Data from the server", strlen("Data from the server"));
    return 0;
}

// Array of pointers to other service definitions
// UUID - Universal Unique Identifier
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = BLE_UUID16_DECLARE(0x180),                 // Define UUID for device type
     .characteristics = (struct ble_gatt_chr_def[]){
         {.uuid = BLE_UUID16_DECLARE(0xFEF4),           // Define UUID for reading
          .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_INDICATE,  // Add INDICATE flag
          .val_handle = &your_read_attr_handle,  // Store handle here
          .access_cb = device_read},
         {.uuid = BLE_UUID16_DECLARE(0xDEAD),           // Define UUID for writing
          .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
          .access_cb = device_write},
         {0}}},
    {0}};


static int ble_svc_gatt_handler(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
    printf("GATT event, op: %d\n", ctxt->op);
    return 0;
}

// BLE event handling
static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type)
    {
    case BLE_GAP_EVENT_SUBSCRIBE:
        printf("Subscribe event. Cur_notify=%d\n", event->subscribe.cur_notify);
        // Handle subscription change
        if (event->subscribe.attr_handle == your_read_attr_handle) {
            printf("Notifications %s\n", event->subscribe.cur_notify ? "enabled" : "disabled");
        }
        break;
    // Advertise if connected
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI("GAP", "BLE GAP EVENT CONNECT %s", event->connect.status == 0 ? "OK!" : "FAILED!");
        if (event->connect.status != 0)
        {
            ble_app_advertise();
        }
        break;
    // Advertise again after completion of the event
    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI("GAP", "BLE GAP EVENT");
        ble_app_advertise();
        break;
    default:
        break;
    }
    return 0;
}

// Define the BLE connection
void ble_app_advertise(void)
{
    // GAP - device name definition
    struct ble_hs_adv_fields fields;
    const char *device_name;
    memset(&fields, 0, sizeof(fields));
    device_name = ble_svc_gap_device_name(); // Read the BLE device name
    fields.name = (uint8_t *)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;
    ble_gap_adv_set_fields(&fields);

    // GAP - device connectivity definition
    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND; // connectable or non-connectable
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN; // discoverable or non-discoverable
    ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
}

// The application
void ble_app_on_sync(void)
{
    ble_hs_id_infer_auto(0, &ble_addr_type); // Determines the best address type automatically
    ble_app_advertise();                     // Define the BLE connection
}

// The infinite task
void host_task(void *param)
{
    nimble_port_run(); // This function will return only when nimble_port_stop() is executed
}

void ble_server_task_entry()
{

    ESP_LOGI(BLE_TAG, "BLE server task started.");

    nvs_flash_init();                          // 1 - Initialize NVS flash using
    esp_nimble_hci_init();                     // 2 - Initialize ESP controller
    nimble_port_init();                        // 3 - Initialize the host stack
    ble_svc_gap_device_name_set("BLE-Server2"); // 4 - Initialize NimBLE configuration - server name
    ble_svc_gap_init();                        // 4 - Initialize NimBLE configuration - gap service
    ble_svc_gatt_init();                       // 4 - Initialize NimBLE configuration - gatt service
    ble_gatts_count_cfg(gatt_svcs);            // 4 - Initialize NimBLE configuration - config gatt services
    ble_gatts_add_svcs(gatt_svcs);             // 4 - Initialize NimBLE configuration - queues gatt services.
    ble_hs_cfg.sync_cb = ble_app_on_sync;      // 5 - Initialize application
    nimble_port_freertos_init(host_task);      // 6 - Run the thread


    while(1){
        if (your_read_attr_handle == 0) vTaskDelay(1000);
        else {
            break;
        }
    }

    while (1) {
        ESP_LOGI(BLE_TAG, "BLE connected to device. Starting to transmit temperature...");
        
        send_temperature_update(get_latest_avg_temperature());

        vTaskDelay(500);

    }
}