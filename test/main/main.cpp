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

    BleWrapper::DefaultServer::init("ESP-TEST");
    BleWrapper::DefaultServer::registerRecvCallback(recv_cb);

    while (1)
    {
        if (WifiWrapper::State() == WifiWrapper::State::CONNECTED) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    
    SocketWrapper::Server TcpServer(SocketWrapper::Protocol::TCP);
    int res = TcpServer.init(8888);
    if (res < 0) {
        ESP_LOGE(TAG, "tcp server init failed.");
        return;
    }
    int sock;
    while (1) {
        sock = TcpServer.accept();
        if (sock < 0) {
            ESP_LOGE(TAG, "tcp server accept failed.");
            break;
        } else {
            SocketWrapper::Socket con_client(sock);
            while (1) {
                char buf[128];
                int len = con_client.recv(buf, sizeof(buf));
                if (len > 0) {
                    ESP_LOGI(TAG, "tcp recv: %.*s", len, buf);
                }
            }
        }
    }
}
