#include "ota_wrapper.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include <esp_crt_bundle.h>

#include <atomic>

namespace OtaWrapper {

static const char *TAG = "ota_wrapper";

static esp_https_ota_handle_t _https_ota_handle = nullptr;
static std::atomic<Status> _status = Status::IDLE;
static std::atomic_int32_t _total_len = 0;


static esp_err_t validate_image_header(esp_app_desc_t *new_app_info) {
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;
    esp_ota_get_partition_description(running, &running_app_info);
    _total_len = 0;

    /* compare app version */
    if (memcmp(new_app_info->version, running_app_info.version, sizeof(new_app_info->version)) == 0) {
        ESP_LOGW(TAG, "App version is the same as a new.");
        return 1;
    }

    _total_len = esp_https_ota_get_image_size(_https_ota_handle);

    return ESP_OK;
}

static void ota_process_task(void *pvParameter) {
    esp_app_desc_t app_desc;
    _status = Status::PROCESSING;
    ESP_LOGI(TAG, "Starting OTA service");

    esp_err_t err = esp_https_ota_get_img_desc(_https_ota_handle, &app_desc);
    if (err != ESP_OK) {
        goto exit;
    }

    err = validate_image_header(&app_desc);
    if (err != ESP_OK) {
        esp_https_ota_abort(_https_ota_handle);
        _status = Status::SAMENESS;
        goto exit;
    }

    while (1) {
        err = esp_https_ota_perform(_https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
    }

    if (err != ESP_OK) {
        goto exit;
    }

    if (esp_https_ota_is_complete_data_received(_https_ota_handle) != true) {
        // the OTA image was not completely received and user can customise the response to this situation.
        ESP_LOGE(TAG, "Complete data was not received.");
        err = ESP_FAIL;
        goto exit;
    } else if (esp_https_ota_finish(_https_ota_handle) != true){
        err = ESP_FAIL;
        goto exit;
    }

    ESP_LOGI(TAG, "OTA download finished.");
    _status = Status::SUCCESS;

exit:
    if (err != ESP_OK) {
        esp_https_ota_abort(_https_ota_handle);
        _status = Status::FAILURE;
    }
    vTaskDelete(nullptr);
}


Status status() {
	return _status;
}

int getSize() {
    return _total_len;
}

uint8_t getPercentage() {
    uint8_t percent = 0;
    int len = esp_https_ota_get_image_len_read(_https_ota_handle);
	if (len < 0 || getSize() < 0) {
		return 100;
	}

    percent = (len  * 100) / getSize();
    return percent;
}

void start(std::string_view url) {
    esp_http_client_config_t http_config;
    memset(&http_config, 0, sizeof(http_config));
    http_config.url = url.data();
    http_config.timeout_ms = 10000;
    http_config.buffer_size = 2048;
    http_config.skip_cert_common_name_check = true;
    http_config.crt_bundle_attach = esp_crt_bundle_attach;
    http_config.keep_alive_enable = true;

	// https_ota_config
	esp_https_ota_config_t ota_config;
    memset(&ota_config, 0, sizeof(ota_config));
    ota_config.http_config = &http_config;

    esp_err_t err = esp_https_ota_begin(&ota_config, &_https_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "ESP HTTPS OTA Begin failed");
        esp_https_ota_abort(_https_ota_handle);
        return;
    }
    _status = Status::BEGIN;

    xTaskCreate(ota_process_task, "ota_process_task", 6 * 1024, nullptr, 12, nullptr);
}

void abort() {
    esp_https_ota_abort(_https_ota_handle);
}

}
