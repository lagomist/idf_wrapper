#include "wifi_info_store.h"
#include "nvs2.h"
#include <esp_log.h>
#include <string.h>
#include <atomic>

namespace wifi_info_store {

constexpr static const char TAG[] = "wifi_info_store";

//nvs名字千万不要改，否则无法向前兼容
constexpr static const char _name_wifi_info[] = "wifi_prov";

static struct {
	char ssid[32] = {};
	char pswd[64] = {};
	char bssid[6] = {};
} _prov;

static std::atomic<Callback> _prov_cb = nullptr;

bool is_provisioned() {
	return _prov.ssid[0];
}

std::string get_ssid() {
	// 检查最后一字节是否有值, 如果有值则截断
	return {_prov.ssid, _prov.ssid[sizeof(_prov.ssid) - 1] ? sizeof(_prov.ssid) : strlen(_prov.ssid)};
}

std::string get_pswd() {
	// 检查最后一字节是否有值, 如果有值则截断
	return {_prov.pswd, _prov.pswd[sizeof(_prov.pswd) - 1] ? sizeof(_prov.pswd) : strlen(_prov.pswd)};
}

std::string get_bssid() {
	return {_prov.bssid, sizeof(_prov.bssid)};
}

void set_prov_cb(Callback cb) {
	_prov_cb = cb;
}

void nvs_write(std::string_view ssid, std::string_view pswd, std::string_view bssid) {
	_prov = {};
	memcpy(_prov.ssid, ssid.data(), std::min(sizeof(_prov.ssid), ssid.size()));
	memcpy(_prov.pswd, pswd.data(), std::min(sizeof(_prov.pswd), pswd.size()));
	memcpy(_prov.bssid, bssid.data(), sizeof(_prov.bssid));
	nvs::write(_name_wifi_info, _prov);
	if (_prov_cb)
		_prov_cb.load()();
}

void powerup_load() {
	_prov = {};
	if (nvs::read(_name_wifi_info, _prov) < 0)
		ESP_LOGW(TAG, "has not yet been provisioned");
	else
		ESP_LOGW(TAG, "load wifi info, ssid [%.32s] pswd [%.64s]", _prov.ssid, _prov.pswd);
}

void nvs_erase() {
	_prov = {};
	nvs::erase(_name_wifi_info);
	ESP_LOGW(TAG, "erase");
}

}
