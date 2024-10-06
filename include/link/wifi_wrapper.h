#pragma once

#include <esp_wifi_types.h>
#include <string>

namespace WifiWrapper {

using Callback = void(*)();

enum class State : uint8_t {
	OK,
	ERR,
	WAITTING,
	PSWD_ERR,
	NO_AP_FOUND,
};

using Mode = wifi_mode_t;

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

void netif_init();
State state();
Mode get_mode();
std::string get_ip();


namespace Station {

void connect(std::string_view ssid, std::string_view pswd);
void connect();
void disconnect();

State provision(std::string_view ssid, std::string_view pswd, uint32_t timeout_ms = 10000);

void set_connect_cb(Callback cb);
void set_disconnect_cb(Callback cb);

bool is_connected();
int get_rssi();

void init();
void deinit();

} /* namespace WifiWrapper::Station */

namespace Softap {

void init(std::string_view ssid, std::string_view pswd);
void deinit();

} /* namespace WifiWrapper::Softap */


namespace Apsta {

void connect(std::string_view ssid, std::string_view pswd);
void connect();
void disconnect();

State provision(std::string_view ssid, std::string_view pswd, uint32_t timeout_ms = 10000);

void set_connect_cb(Callback cb);
void set_disconnect_cb(Callback cb);

bool is_connected();
int get_rssi();

void init(std::string_view ssid, std::string_view pswd);
void deinit();

} /* namespace WifiWrapper::Apsta */

namespace store {

bool is_provisioned();
std::string read_ssid();
std::string read_pswd();
void erase();
int write(std::string_view ssid, std::string_view pswd);

} /* namespace WifiWrapper::store */


} /* namespace WifiWrapper */
