#include "nvs_wrapper.h"
#include "wifi_wrapper.h"
#include "ble_wrapper.h"
#include "socket_wrapper.h"
#include "firmware_wrapper.h"

#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>


#define AP_SSID     "T-SIMCAM"
#define AP_PAWD     "nexblue2024!"

#define TAG         "main"

static void recv_cb(IBuf recv) {
    ESP_LOGI(TAG, "recv: %.*s", recv.size(), recv.data());
}

extern "C" void app_main()
{
    ESP_LOGI(TAG, "%s", Firmware::info().c_str());
    NvsWrapper::init("nvs");
    WifiWrapper::netif_init();
    WifiWrapper::Station::init();
    if (!WifiWrapper::Store::is_provisioned()) {
        WifiWrapper::Station::provision("ESP-SOFTAP", "3325035137");
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
    if (!WifiWrapper::Station::is_connected())
        WifiWrapper::Station::disconnect();

    BleWrapper::DefaultServer::init("ESP-TEST");
    BleWrapper::DefaultServer::registerRecvCallback(recv_cb);

    TCPSocket socket;
    socket.server(8080);
    socket.accept(socket.fd());
    char buf[64];

    while (1)
    {
        int len = socket.recv(buf, 64);
        if (len > 0) {
            ESP_LOGI(TAG, "%.*s", len, buf);
        }
    }
    
}