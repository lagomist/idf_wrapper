#include "nvs_wrapper.h"
#include "fs_wrapper.h"
#include "wifi_wrapper.h"
#include "ble_wrapper.h"
#include "socket_wrapper.h"
#include "firmware_wrapper.h"
#include "time_wrapper.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>


#define TAG         "main"


extern "C" void app_main()
{
    Wrapper::NVS::init("nvs");
    Wrapper::FileSystem::mount();

    ESP_LOGI(TAG, "date: %s", Wrapper::Time::date().data());

    Wrapper::FileSystem::mkdir("test");
    Wrapper::FileSystem::list("/");

    Wrapper::FileSystem::write("test.txt", "hello world!");

    std::string content = Wrapper::FileSystem::cat("test.txt");
    ESP_LOGI(TAG, "content: %.*s", content.size(), content.data());
}
