#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "cJSON.h"

#include "blufi_wrapper.h"
#include "gatt_server_wrapper.h"
#include "wifi_wrapper.h"

#define CONNECT_WAIT_TIME_MS    10000
#define RECV_BUFFER_SIZE        96


static const char *TAG = "blufi_wrapper";

static QueueHandle_t blufi_queue = NULL;
static char recv_buf[RECV_BUFFER_SIZE] = {0};


static void blufi_gatts_recv_callback(gatts_wrapper_recv_t recv)
{
    if (recv.len < RECV_BUFFER_SIZE) {
        memcpy(recv_buf, recv.data, recv.len);
        xQueueSend(blufi_queue, &recv, 10 / portTICK_PERIOD_MS);
    }
}


static void blufi_handle_task(void *pvParameters)
{
    gatts_wrapper_recv_t recv;
    while (1) {
        xQueueReceive(blufi_queue, &recv, portMAX_DELAY);
        recv_buf[recv.len] = '\0';
        ESP_LOGI(TAG, "blufi recv data: %s", recv_buf);
        cJSON *root = cJSON_Parse(recv_buf);

        cJSON *ssid = cJSON_GetObjectItem(root, "ssid");
        cJSON *password = cJSON_GetObjectItem(root, "password");

        if(ssid != NULL && password != NULL) {
            if(ssid->type == cJSON_String && password->type == cJSON_String) {
                ESP_LOGI(TAG, "begain to reset wifi");
                wifi_wrapper_account_cfg_t config;
                char respond[32] = {0};
                memcpy(config.ssid, ssid->valuestring, sizeof(config.ssid));
                memcpy(config.password, password->valuestring, sizeof(config.password));
                wifi_wrapper_connect_reset(&config);
                wifi_wrapper_connect(CONN_5X);

                if (wifi_wrapper_wait_connected(pdMS_TO_TICKS(CONNECT_WAIT_TIME_MS))) {
                    sprintf(respond, "wifi connect successfully!");
                    ESP_LOGI(TAG, "%s", respond);
                    wifi_wrapper_nvs_store_account(&config);
                } else {
                    sprintf(respond, "wifi connect failed!");
                    ESP_LOGW(TAG, "%s", respond);
                }
                wifi_wrapper_disconnect();
                esp_ble_gatts_send_indicate(recv.gatts_if, recv.conn_id, recv.handle,
                                strlen(respond), (uint8_t *)respond, false);
            }
        }

        cJSON_Delete(root);  // 释放cJSON对象
    }
}


void blufi_wrapper_create_service()
{
    esp_bt_uuid_t service = {
        .len = ESP_UUID_LEN_16,
        .uuid.uuid16 = BLUFI_WRAPPER_SVC_UUID,
    };
    gatts_wrapper_add_service(BLUFI_WRAPPER_APP_ID, &service);

    gatts_wrapper_char_config_t char_config = {
        .index = 0, 
        .perm = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
        .property = ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
        .uuid.len = ESP_UUID_LEN_16,
        .uuid.uuid.uuid16 = BLUFI_WRAPPER_CHAR_UUID,
    };
    gatts_wrapper_service_add_char(BLUFI_WRAPPER_APP_ID, &char_config);
    gatts_wrapper_service_register_callback(BLUFI_WRAPPER_APP_ID, blufi_gatts_recv_callback);


    blufi_queue = xQueueCreate(1, sizeof(gatts_wrapper_recv_t));
    xTaskCreate(blufi_handle_task, "blufi_handle_task", 4 * 1024, NULL, 16, NULL);
}
