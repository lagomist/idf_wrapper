#pragma once

#include <string>
#include <string_view>

namespace wifi_info_store {

using Callback = void(*)();

constexpr std::string_view DUMMY_BSSID = {"\x00\x00\x00\x00\x00\x00", 6};

bool is_provisioned();

std::string get_ssid();
std::string get_pswd();
std::string get_bssid();

void set_prov_cb(Callback cb);

void nvs_write(std::string_view ssid, std::string_view pswd, std::string_view bssid = DUMMY_BSSID);
void powerup_load();
void nvs_erase();

}
