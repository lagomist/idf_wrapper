#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "freertos/event_groups.h"

#include "wifi_wrapper.h"
#include "http_ota_wrapper.h"


static const char *TAG = "http_ota_wrapper";
static EventGroupHandle_t ota_event_group;
static esp_https_ota_config_t ota_config;
static esp_http_client_config_t client_config;
static char ota_upgrade_url[256];
static int interval_ms = 0;
static http_ota_wrapper_mode_t mode;

#define OTA_SMAE_BIT    BIT0
#define OTA_FINISH_BIT  BIT1
#define OTA_FAIL_BIT    BIT2

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
        break;
    }
    return ESP_OK;
}

static esp_err_t validate_image_header(esp_app_desc_t *new_app_info)
{
    if (new_app_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;
    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
        ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
    }

    /* compare app version */
    if (memcmp(new_app_info->version, running_app_info.version, sizeof(new_app_info->version)) == 0) {
        ESP_LOGW(TAG, "App version is the same as a new.");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static int http_ota_process(esp_https_ota_config_t *ota_config)
{
    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(ota_config, &https_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "ESP HTTPS OTA Begin failed");
        esp_https_ota_abort(https_ota_handle);
        return OTA_FAIL;
    }

    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
    if (err != ESP_OK) {
        return OTA_FAIL;
    }
    
    err = validate_image_header(&app_desc);
    if (err != ESP_OK) {
        esp_https_ota_abort(https_ota_handle);
        return OTA_SAME;
    }

    while (1) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
    }

    if (err != ESP_OK) {
        return OTA_FAIL;
    }

    if (esp_https_ota_is_complete_data_received(https_ota_handle) != true) {
        // the OTA image was not completely received and user can customise the response to this situation.
        ESP_LOGE(TAG, "Complete data was not received.");
        return OTA_FAIL;
    } else {
        err = esp_https_ota_finish(https_ota_handle);
        if (err != ESP_OK) {
            return OTA_FAIL;
        }
    }
    return OTA_FINISH;
}


int http_ota_wrapper_process()
{
    if (wifi_wrapper_wait_connected(0) == false) {
        return -1;
    }
    /* clear all event bits */
    xEventGroupClearBits(ota_event_group, OTA_SMAE_BIT | OTA_FAIL_BIT | OTA_FINISH_BIT);

    int ret = http_ota_process(&ota_config);
    switch (ret) {
    case OTA_FAIL:
        ESP_LOGW(TAG, "OTA upgrade failed.");
        xEventGroupSetBits(ota_event_group, OTA_FAIL_BIT);
        break;
    case OTA_FINISH:
        ESP_LOGI(TAG, "OTA upgrade successful.");
        xEventGroupSetBits(ota_event_group, OTA_FINISH_BIT);
        break;
    case OTA_SAME:
        ESP_LOGI(TAG, "Cancel app update.");
        xEventGroupSetBits(ota_event_group, OTA_SMAE_BIT);
        break;

    default:
        break;
    }
    return ret;
}

void http_ota_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Starting OTA service");

    while (1) {
        wifi_wrapper_wait_connected(portMAX_DELAY);

        int ret = http_ota_wrapper_process();
        if (ret == OTA_FINISH) {
            ESP_LOGI(TAG, "Rebooting...");
            vTaskDelay(pdMS_TO_TICKS(3000));
            esp_restart();
        }
        vTaskDelay(pdMS_TO_TICKS(interval_ms));
    }

}



http_ota_wrapper_ret_t http_ota_wrapper_wait_result()
{
    http_ota_wrapper_ret_t status;
    EventBits_t bits = xEventGroupWaitBits(ota_event_group,
                        OTA_FINISH_BIT | OTA_SMAE_BIT | OTA_FAIL_BIT,
                        pdFALSE, pdFALSE, portMAX_DELAY);
    if (bits & OTA_SMAE_BIT) {
        status = OTA_SAME;
    } else if (bits & OTA_FINISH_BIT){
        status = OTA_FINISH;
    } else {
        status = OTA_FAIL;
    }

    return status;
}

void http_ota_wrapper_service_config(http_ota_wrapper_cfg_t *config)
{
    strncpy(ota_upgrade_url, config->url, 256);
    interval_ms = config->interval;
    mode = config->mode;

    client_config.url = ota_upgrade_url;
    client_config.event_handler = _http_event_handler;
    client_config.timeout_ms = 3000;
    client_config.keep_alive_enable = true;

    ota_config.http_config = &client_config;

    ota_event_group = xEventGroupCreate();
}


void http_ota_wrapper_service_start()
{
    if (mode != OTA_AUTOMATIC) return;
    
    /* Ensure to disable any WiFi power save mode, this allows best throughput
     * and hence timings for overall OTA operation.
     */
    esp_wifi_set_ps(WIFI_PS_NONE);

    xTaskCreate(&http_ota_task, "http_ota_task", 6 * 1024, NULL, 5, NULL);
}


