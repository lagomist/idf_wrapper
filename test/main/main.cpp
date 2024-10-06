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

static void recv_cb(IBuf recv) {
    ESP_LOGI(TAG, "recv: %s", recv.data());
}

extern "C" void app_main()
{
    NvsWrapper::init("nvs");
    WifiWrapper::netif_init();
    WifiWrapper::Station::init();
    if (!WifiWrapper::store::is_provisioned()) {
        WifiWrapper::Station::provision("ESP-SOFTAP", "3325035137");
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
    if (!WifiWrapper::Station::is_connected())
        WifiWrapper::Station::disconnect();


    // BleWrapper::DefaultServer::init("ESP-TEST");
    // BleWrapper::DefaultServer::registerRecvCallback(recv_cb);
   
    BleWrapper::Client::init();
    BleWrapper::Client::createDefaultService("ESP-TEST");
    BleWrapper::Client::registerRecvCallback(recv_cb);
}