#include "wifi2.h"
#include "os_api.h"
#include "utility.h"
#include <lwip/netdb.h>
#include <lwip/dns.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <string.h>
#include <atomic>

namespace wifi {

static constexpr char TAG[] = "wifi2";

static std::atomic<esp_netif_t*> _esp_netif = nullptr;
static std::atomic<Callback> _connect_cb = +[](){};
static std::atomic<Callback> _disconnect_cb = +[](){};
static std::atomic<State> _state = State::NO_AP_FOUND;
static std::atomic_bool	_auto_reconnect = false;

#ifdef PROJECT_SMBNA
static os::Timer _wifi_timer([](os::Timer& t) {
	esp_wifi_connect();
},10000, false, "wifi reconnection");
#endif // 低功耗处理，用于wifi断开后延时重连

static void set_state(State state) {
	_state = state;
}

static void wifi_set_config(std::string_view ssid, std::string_view pswd, std::string_view bssid) {
	wifi_config_t sta_cfg = {
		.sta = {
#ifdef PROJECT_SMBNA
			.listen_interval =20,
#endif // 低功耗处理
			.threshold = {
				.rssi = 0,
				.authmode = pswd.size() ? WIFI_AUTH_WEP : WIFI_AUTH_OPEN,
			},
			.pmf_cfg = {
				.capable = true,
				.required = false,
			},
		},
	};
	memcpy(sta_cfg.sta.ssid, ssid.data(), std::min(sizeof(sta_cfg.sta.ssid), ssid.size()));
	memcpy(sta_cfg.sta.password, pswd.data(), std::min(sizeof(sta_cfg.sta.password), pswd.size()));
	if (bssid != wifi_info_store::DUMMY_BSSID) {
		sta_cfg.sta.bssid_set = true;
		memcpy(sta_cfg.sta.bssid, bssid.data(), sizeof(sta_cfg.sta.bssid));
	}
	ESP_LOGI(TAG, "connect,ssid,%.*s,pswd,%.*s,bssid,%02X%02X%02X%02X%02X%02X,",
		ssid.size(), ssid.data(), 
		pswd.size(), pswd.data(),
		bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]
	);
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
}

static void sta_disconnect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
	auto data = (wifi_event_sta_disconnected_t*)event_data;

	set_state(State::NO_AP_FOUND);
	
	ESP_LOGE(TAG, "disconnected,reason,%d,rssi,%d,", data->reason, data->rssi);
	_disconnect_cb.load()();

#ifdef PROJECT_SMBNA // 低功耗处理，用于wifi断开后延时重连
	if (_auto_reconnect){
		esp_wifi_disconnect();
		_wifi_timer.start();
	}
#else
	if (_auto_reconnect)
		esp_wifi_connect();
#endif //

}

static void got_ip_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
	// ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
	ip_addr_t dnsserver;
	inet_pton(AF_INET, "8.8.8.8", &dnsserver);
	dns_setserver(1, &dnsserver);
	set_state(State::OK);
	_connect_cb.load()();
}

void connect() {
	if (!wifi_info_store::is_provisioned())
		return;
	connect(wifi_info_store::get_ssid(), wifi_info_store::get_pswd());
}

void connect(std::string_view ssid, std::string_view pswd, std::string_view bssid) {
	disconnect();
	_auto_reconnect = true;
	set_state(State::WAITTING);
	wifi_set_config(ssid, pswd, bssid);
	esp_wifi_connect();
}

void disconnect() {
	_auto_reconnect = false;
	esp_wifi_disconnect();
}

State prov(std::string_view ssid, std::string_view pswd, std::string_view bssid, uint32_t timeout_ms) {
	wifi::connect(ssid, pswd, bssid);
	utility::retry([]() {
		return state() != wifi::State::OK ? -1 : 0;
	}, 10, timeout_ms / 10);
	switch (state()) {
		// 在超时时间内连上, 就写入nvs
		case State::OK:
			wifi_info_store::nvs_write(ssid, pswd, bssid);
			break;
		// 否则回滚到之前的配置
		default:
			// 之前有配过网就沿用, 否则不再重试连接
			if (wifi_info_store::is_provisioned())
				wifi_set_config(wifi_info_store::get_ssid(), wifi_info_store::get_pswd(), wifi_info_store::get_bssid());
			else
				disconnect();
			break;
	}
	ESP_LOGW(TAG, "prov res %s", wifi::state_str(state()));
	return state();
}

bool is_connected() {
	return esp_netif_is_netif_up(_esp_netif);
}

void set_connect_cb(Callback cb) {
	_connect_cb = cb;
}

void set_disconnect_cb(Callback cb) {
	_disconnect_cb = cb;
}

int get_rssi() {
	wifi_ap_record_t ap_info;
	esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
	return ret == ESP_OK ? ap_info.rssi : -100;
}

State state() {
	return _state;
}

void* get_esp_netif() {
	return _esp_netif;
}

uint32_t get_ip() {
	esp_netif_ip_info_t ip_info{};
	esp_netif_get_ip_info(_esp_netif, &ip_info);
	return ip_info.ip.addr;
}

APInfo get_ap_info() {
	wifi_ap_record_t ap_info{};
	esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
	if (ret == ESP_OK)
		return APInfo(ap_info);
	return APInfo(wifi_info_store::get_ssid(), wifi_info_store::get_pswd());
}

void init(std::string_view host_name, bool modem_sleep_mode, uint16_t sleep_sec) {
	if (_esp_netif)
		return;
	_esp_netif = esp_netif_create_default_wifi_sta();
	wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
	init_cfg.nvs_enable = false;
	ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	// ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, sta_connect_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, sta_disconnect_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, got_ip_handler, NULL));
	esp_wifi_start();
	if (modem_sleep_mode) {
		ESP_ERROR_CHECK(esp_wifi_set_inactive_time(WIFI_IF_STA, sleep_sec));
		esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
		ESP_LOGI(TAG, "esp_wifi_set_ps().");
	}
	esp_netif_set_hostname(_esp_netif, host_name.data());
	connect();
}

void deinit() {
	if (_esp_netif == nullptr)
		return;
	esp_err_t err = esp_wifi_stop();
	if (err == ESP_ERR_WIFI_NOT_INIT) {
        return;
    }
	// esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, sta_connect_handler);
	esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, sta_disconnect_handler);
	esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, got_ip_handler);
	esp_wifi_deinit();
	esp_netif_destroy_default_wifi(_esp_netif);
	_esp_netif = nullptr;
}

}//namespace wifi_sta
