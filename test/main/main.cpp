#include "nvs_wrapper.h"
#include "wifi_wrapper.h"

#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#define AP_SSID     "T-SIMCAM"
#define AP_PAWD     "nexblue2024!"



extern "C" void app_main()
{
    NvsWrapper::init("nvs");
    WifiWrapper::netif_init();
    WifiWrapper::softap::init(AP_SSID, AP_PAWD);
}