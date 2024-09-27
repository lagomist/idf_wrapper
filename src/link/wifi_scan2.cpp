#include "wifi_scan2.h"
#include "serial.h"
#include <esp_wifi.h>
#include <esp_log.h>
#include <string.h>
#include <vector>

namespace wifi_scan {

constexpr static const char TAG[] = "wifi_scan";

static std::vector<wifi_ap_record_t> wifi_scan_get_ap_records(uint8_t max_record_num) {
	uint16_t ap_count = 0;
	std::vector<wifi_ap_record_t> ap_list;

	esp_wifi_scan_get_ap_num(&ap_count);
	if (ap_count == 0) {
		ESP_LOGW(TAG, "no AP found");
		return ap_list;
	}
	if(ap_count > max_record_num)
		ap_count = max_record_num;

	ap_list.resize(ap_count);
	if (esp_wifi_scan_get_ap_records(&ap_count, ap_list.data()) != ESP_OK) {
		ESP_LOGE(TAG, "got wifi list scan fail");
		ap_list.clear();
	}
	return ap_list;
}

static std::vector<wifi_ap_record_t> wifi_scan_guard(uint8_t max_record_num) {
	wifi_scan_config_t scanConf = {
		.ssid = NULL,
		.bssid = NULL,
		.channel = 0,
		.show_hidden = false
	};
	wifi::disconnect();
 	esp_wifi_scan_start(&scanConf, true);
	esp_wifi_scan_stop();
	auto ap_list = wifi_scan_get_ap_records(max_record_num);
	wifi::connect();
	return ap_list;
}


OBuf get_wifi_scan_list(uint8_t max_record_num, size_t size) {
	auto ap_list = wifi_scan_guard(max_record_num);
	if (ap_list.empty()) return {};

	OBuf out;
	for (const auto& e : ap_list) {
		auto len = strlen((char*)e.ssid);
		if (len == 0) {
			continue;
		}
		if (out.size() + len + 1 >= size)
			break;
		out.append(e.ssid, len);
		out.push_back('\n');
		ESP_LOGI(TAG, "auth,%02d,rssi,%02d,mac,%02X%02X%02X%02X%02X%02X,ssid,%s,",
			(int)e.authmode,
			e.rssi,
			e.bssid[0], e.bssid[1], e.bssid[2], e.bssid[3], e.bssid[4], e.bssid[5],
			e.ssid
		);
	}
	return out;
}

OBuf get_wifi_scan_list_new(uint8_t max_record_num, size_t size) {
	auto ap_list = wifi_scan_guard(max_record_num);
	if (ap_list.empty()) return {};

	OBuf out;
	for (const auto& e : ap_list) {
		OBuf item = wifi::APInfo(e).pack();
		if (out.size() + item.size() >= size)
			break;
		out += item;
		ESP_LOGI(TAG, "auth,%02d,rssi,%02d,mac,%02X%02X%02X%02X%02X%02X,ssid,%s,",
			(int)e.authmode,
			e.rssi,
			e.bssid[0], e.bssid[1], e.bssid[2], e.bssid[3], e.bssid[4], e.bssid[5],
			e.ssid
		);
	}
	return out;
}
	
} // namespace wifi_scan
