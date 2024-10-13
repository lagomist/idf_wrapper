#pragma once

#include <esp_wifi_types.h>
#include <string>

namespace WifiWrapper {

using Callback = void(*)();

enum class State : uint8_t {
	IDLE,
	WAITTING,
	CONNECTED,
	NO_AP_FOUND,
	ERROR,
};

using Mode = wifi_mode_t;

constexpr const char* stateString(State state) {
	switch (state) {
		case State::IDLE: return "idle";
		case State::WAITTING: return "waitting";
		case State::CONNECTED: return "connected";
		case State::NO_AP_FOUND: return "no_ap_found";
		case State::ERROR: return "error";
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

namespace Store {

bool is_provisioned();
std::string read_ssid();
std::string read_pswd();
void erase();
int write(std::string_view ssid, std::string_view pswd);

} /* namespace WifiWrapper::Store */


} /* namespace WifiWrapper */
