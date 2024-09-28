#include "nvs_wrapper.h"
#include "wifi_wrapper.h"
#include "ble_wrapper.h"

#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>


#define AP_SSID     "T-SIMCAM"
#define AP_PAWD     "nexblue2024!"

#define TAG         "main"

static void recv_cb(BleWrapper::server::RecvEvtParam recv) {
    ESP_LOGI(TAG, "recv: %s", recv.data);
}

extern "C" void app_main()
{
    NvsWrapper::init("nvs");
    WifiWrapper::netif_init();
    WifiWrapper::station::init();
    if (!WifiWrapper::store::is_provisioned()) {
        WifiWrapper::station::provision("ESP-SOFTAP", "3325035137");
    }


    BleWrapper::server::init("ESP-TEST");
    BleWrapper::server::start_default_service(0);
    BleWrapper::server::register_recv_cb(0, recv_cb);

    vTaskDelay(pdMS_TO_TICKS(5000));
    if (!WifiWrapper::station::is_connected())
        WifiWrapper::station::disconnect();

    uint8_t data[30];
    uint8_t i = 0;
    while (1)
    {
        sprintf((char *)data, "adv test data: %d", i++);
        BleWrapper::server::adv_update(data, strlen((char *)data));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
}