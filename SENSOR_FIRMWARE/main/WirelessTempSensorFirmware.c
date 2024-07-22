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

#define DEVICE_NAME "BLE-Client"
#define WRITE_CHAR_UUID 0xDEAD
#define DISCOVERY_RETRY_DELAY_MS 1000  // Delay before retrying discovery

char *TAG = "BLE Client";

static uint16_t conn_handle;
static uint8_t own_addr_type;
static uint16_t char_handle = 0;

void ble_app_scan(void);
int write_to_characteristic(uint16_t conn_handle, uint16_t char_handle, const uint8_t *data, size_t length);

static int on_characteristic_discovered(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr, void *arg) {
    if (error->status != 0) {
        ESP_LOGE(TAG, "Characteristic discovery failed, status: %d", error->status);
        // Retry characteristic discovery if needed
        vTaskDelay(pdMS_TO_TICKS(DISCOVERY_RETRY_DELAY_MS));
        int rc = ble_gattc_disc_all_chrs(conn_handle, 1, 0xFFFF, on_characteristic_discovered, NULL);
        if (rc != 0) {
            ESP_LOGE(TAG, "Retry characteristic discovery failed, rc=%d", rc);
        }
        return 0;
    }

    ESP_LOGI(TAG, "Characteristic discovered: def_handle=%d, val_handle=%d, uuid=0x%04x", 
             chr->def_handle, chr->val_handle, chr->uuid.u16.value);

    if (chr->uuid.u16.value == WRITE_CHAR_UUID) {
        ESP_LOGI(TAG, "Write characteristic discovered");
        char_handle = chr->val_handle;

        // Send "hello, world!" message
        const char *msg = "hello, world!";
        int rc = ble_gattc_write_flat(conn_handle, char_handle, msg, strlen(msg), NULL, NULL);
        if (rc != 0) {
            ESP_LOGE(TAG, "Error writing to characteristic: %d", rc);
        } else {
            ESP_LOGI(TAG, "Write request sent successfully");
        }
    }

    return 0;
}

static int on_service_discovered(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_svc *service, void *arg) {
    if (error->status != 0) {
        ESP_LOGE(TAG, "Service discovery failed, status: %d", error->status);
        // Retry service discovery if needed
        vTaskDelay(pdMS_TO_TICKS(DISCOVERY_RETRY_DELAY_MS));
        int rc = ble_gattc_disc_all_svcs(conn_handle, on_service_discovered, NULL);
        if (rc != 0) {
            ESP_LOGE(TAG, "Retry service discovery failed, rc=%d", rc);
        }
        return -1;
    }

    ESP_LOGI(TAG, "Service discovered: start_handle=%u, end_handle=%u", service->start_handle, service->end_handle);
    ble_gattc_disc_all_chrs(conn_handle, service->start_handle, service->end_handle, on_characteristic_discovered, NULL);

    return 0;
}

static int ble_gap_event(struct ble_gap_event *event, void *arg) {
    struct ble_hs_adv_fields fields;
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                ESP_LOGI(TAG, "Connected");
                conn_handle = event->connect.conn_handle;
                int rc = ble_gattc_disc_all_svcs(conn_handle, on_service_discovered, NULL);
                if (rc != 0) {
                    ESP_LOGE(TAG, "Failed to start service discovery, rc=%d", rc);
                }
            } else {
                ESP_LOGE(TAG, "Connection failed; status=%d", event->connect.status);
                ble_app_scan();
            }
            return 0;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Disconnected; reason=%d", event->disconnect.reason);
            ble_app_scan();
            return 0;

        // NimBLE event discovery
        case BLE_GAP_EVENT_DISC:
            ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);
            if (fields.name_len > 0)
            {
                printf("Name: %.*s\n", fields.name_len, fields.name);
            
            if (strstr((const char *)event->disc.data, "BLE-Server"))
            {
                // Stop scanning before attempting to connect
                ble_gap_disc_cancel();

                ESP_LOGI(TAG, "Target device found, initiating connection");
                // print_ble_address(&event->disc.addr);
                int ret = ble_gap_connect(own_addr_type, &event->disc.addr, INT32_MAX, NULL, ble_gap_event, NULL);
                printf("Connection ret int: %i \n", ret);

            }
            }
            return 0;
            break;

        default:
            return 0;
    }
}

void ble_app_scan(void) {
    struct ble_gap_disc_params disc_params;
    memset(&disc_params, 0, sizeof(disc_params));
    disc_params.itvl = BLE_HCI_SCAN_ITVL_DEF;
    disc_params.window = BLE_HCI_SCAN_WINDOW_DEF;
    disc_params.filter_policy = BLE_HCI_SCAN_FILT_NO_WL;
    disc_params.limited = 0;
    disc_params.passive = 1;
    int rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &disc_params, ble_gap_event, NULL);
    assert(rc == 0);
}

void ble_app_on_sync(void) {
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    assert(rc == 0);
    ble_app_scan();
}

void host_task(void *param) {
    nimble_port_run();
}

void app_main(void) {
    nvs_flash_init();
    esp_nimble_hci_init();
    nimble_port_init();
    ble_svc_gap_device_name_set(DEVICE_NAME);
    ble_svc_gap_init();
    ble_svc_gatt_init();
    printf("NEW CODE: \n");
    ble_hs_cfg.sync_cb = ble_app_on_sync;
    nimble_port_freertos_init(host_task);
}

int write_to_characteristic(uint16_t conn_handle, uint16_t char_handle, const uint8_t *data, size_t length) {
    int status = ble_gattc_write_flat(conn_handle, char_handle, data, length, NULL, NULL);
    if (status != 0) {
        ESP_LOGE(TAG, "Error writing to characteristic: %d", status);
    } else {
        ESP_LOGI(TAG, "Write request sent successfully");
    }
    return status;
}
