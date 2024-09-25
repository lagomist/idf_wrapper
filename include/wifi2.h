#pragma once

#include "serial.h"
#include "wifi_info_store.h"
#include <esp_wifi_types.h>
#include <string>

namespace wifi {

using Callback = void(*)();

enum class State : uint8_t {
	OK,
	ERR,
	WAITTING,
	PSWD_ERR,
	NO_AP_FOUND,
};

struct APInfo : public wifi_ap_record_t {
	APInfo(const wifi_ap_record_t& raw) : wifi_ap_record_t(std::move(raw)) {}

	APInfo(const std::string& ssid, const std::string& pswd) : 
	wifi_ap_record_t({
		.rssi = -100,
		.authmode = pswd.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WEP
	}) { memcpy(this->ssid, ssid.data(), ssid.size()); }

	serial::obuf_t pack() {
		size_t ssid_len = strlen((char*)ssid);
		return serial::pack(
			(uint8_t)(ssid_len + 8), (uint8_t)authmode, rssi, 
			serial::ibuf_t{bssid, sizeof(bssid)},
			serial::ibuf_t{ssid, ssid_len}
		);
	}
};

constexpr const char* state_str(State state) {
	switch (state) {
		case State::OK: return "ok";
		case State::ERR: return "error";
		case State::WAITTING: return "waitting";
		case State::PSWD_ERR: return "pswd_err";
		case State::NO_AP_FOUND: return "no_ap_found";
		default: return "unknown";
	}
}

void connect(std::string_view ssid, std::string_view pswd, std::string_view bssid = wifi_info_store::DUMMY_BSSID);
void connect();
void disconnect();

State prov(std::string_view ssid, std::string_view pswd, std::string_view bssid = wifi_info_store::DUMMY_BSSID, uint32_t timeout_ms = 10000);

void set_connect_cb(Callback cb);
void set_disconnect_cb(Callback cb);

//是否连接到路由器，已有ip
bool is_connected();
State state();
void* get_esp_netif();

uint32_t get_ip();
int get_rssi();
APInfo get_ap_info();

void init(std::string_view host_name, bool modem_sleep_mode = false, uint16_t sleep_sec = 6);
void deinit();

}
