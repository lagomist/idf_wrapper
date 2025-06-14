#include "wifi_wrapper.h"
#include "nvs_wrapper.h"
#include "wrapper_config.h"

#include "esp_mac.h"
#include "esp_system.h"
#include "esp_event.h"
#include "dhcpserver/dhcpserver.h"
#include <lwip/netdb.h>
#include <lwip/dns.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <string.h>
#include <atomic>

namespace Wrapper {

namespace WiFi {


constexpr static const char* TAG = "Wrapper::WiFI";

static std::atomic<esp_netif_t*> _ap_netif = nullptr;
static std::atomic<esp_netif_t*> _sta_netif = nullptr;
static std::atomic<Callback> _connect_cb = +[](){};
static std::atomic<Callback> _disconnect_cb = +[](){};
static std::atomic<State> _state = State::IDLE;
static std::atomic_bool	_auto_reconnect = false;
static std::atomic<Mode> _wifi_mode = WIFI_MODE_NULL;

namespace Store {

constexpr static const char* NVS_KEY_WIFI_SSID = "wifi_ssid";
constexpr static const char* NVS_KEY_WIFI_PSWD = "wifi_pswd";

bool is_provisioned() {
    int len = Wrapper::NVS::getSize(NVS_KEY_WIFI_SSID);
	return len > 0;
}

std::string read_ssid() {
    char ssid[32] = {};
    Wrapper::NVS::read(NVS_KEY_WIFI_SSID, ssid);
	// 检查最后一字节是否有值, 如果有值则截断
	return {ssid, ssid[sizeof(ssid) - 1] ? sizeof(ssid) : strlen(ssid)};
}

std::string read_pswd() {
    char pswd[64] = {};
    Wrapper::NVS::read(NVS_KEY_WIFI_PSWD, pswd);
	// 检查最后一字节是否有值, 如果有值则截断
	return {pswd, pswd[sizeof(pswd) - 1] ? sizeof(pswd) : strlen(pswd)};
}

int write(std::string_view ssid, std::string_view pswd) {
    int res;
	res = Wrapper::NVS::write(NVS_KEY_WIFI_SSID, ssid.data(), ssid.size());
    res = Wrapper::NVS::write(NVS_KEY_WIFI_PSWD, pswd.data(), pswd.size());
    return res;
}

void erase() {
	Wrapper::NVS::erase(NVS_KEY_WIFI_SSID);
    Wrapper::NVS::erase(NVS_KEY_WIFI_PSWD);
	ESP_LOGW(TAG, "Store erased.");
}

} /* namespace Store */

static void wifi_sta_config(std::string_view ssid, std::string_view pswd) {
	wifi_config_t wifi_cfg = {};
    if (_wifi_mode != WIFI_MODE_STA && _wifi_mode != WIFI_MODE_APSTA) {
        ESP_LOGE(TAG, "mode config error!");
        return;
    }
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    memcpy(wifi_cfg.sta.ssid, ssid.data(), std::min(sizeof(wifi_cfg.sta.ssid), ssid.size()));
    memcpy(wifi_cfg.sta.password, pswd.data(), std::min(sizeof(wifi_cfg.sta.password), pswd.size()));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
	ESP_LOGI(TAG, "sta ssid:%.*s, password:%.*s", ssid.size(), ssid.data(), pswd.size(), pswd.data());
}

static void wifi_ap_config(std::string_view ssid, std::string_view pswd) {
	wifi_config_t wifi_cfg = {};
    if (_wifi_mode != WIFI_MODE_AP && _wifi_mode != WIFI_MODE_APSTA) {
        ESP_LOGE(TAG, "mode config error!");
        return;
    }
    wifi_cfg.ap.channel = WrapperConfig::WIFI_CHANNEL;
    wifi_cfg.ap.max_connection = WrapperConfig::WIFI_MAXCON;
    wifi_cfg.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    wifi_cfg.ap.ssid_hidden = false;
    wifi_cfg.ap.beacon_interval = 100;
    wifi_cfg.ap.ssid_len = ssid.length();
    memcpy(wifi_cfg.ap.ssid, ssid.data(), std::min(sizeof(wifi_cfg.ap.ssid), ssid.size()));
    memcpy(wifi_cfg.ap.password, pswd.data(), std::min(sizeof(wifi_cfg.ap.password), pswd.size()));
    if (pswd.size() == 0) {
        wifi_cfg.ap.authmode = WIFI_AUTH_OPEN;
    }
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg));
    ESP_LOGI(TAG, "ap ssid:%.*s, password:%.*s", ssid.size(), ssid.data(), pswd.size(), pswd.data());

}



static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    /* ---------------WiFi AP event----------------*/
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        ESP_LOGI(TAG, "Softap start");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }

    /* ---------------WiFi Station event----------------*/

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        /* wifi disconnected event */
        auto data = (wifi_event_sta_disconnected_t*)event_data;
        if (_state == State::CONNECTED) {
            ESP_LOGE(TAG, "disconnected reason:%d,rssi:%d", data->reason, data->rssi);
            _disconnect_cb.load()();
        }
        if (_auto_reconnect) {
            esp_wifi_connect();
        }
        _state = State::NO_AP_FOUND;
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_STOP) {
        /* wifi stop event */
        ESP_LOGD(TAG, "wifi stoped.");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "wifi connected.");
        /* wifi connection successful */
        _state = State::CONNECTED;
        _connect_cb.load()();

#if IP_NAPT
        {
            esp_netif_dns_info_t dns;
            if (esp_netif_get_dns_info(_sta_netif, ESP_NETIF_DNS_MAIN, &dns) == ESP_OK) {
                esp_netif_set_dns_info(_ap_netif, ESP_NETIF_DNS_MAIN, &dns);
                ESP_LOGI(TAG, "set dns to:" IPSTR, IP2STR(&(dns.ip.u_addr.ip4)));
            }
        }
#endif
    }
}


static void wifi_deinit(esp_netif_t* netif) {
    if (netif == nullptr)
		return;
	esp_err_t err = esp_wifi_stop();
	if (err == ESP_ERR_WIFI_NOT_INIT) {
        return;
    }
	esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler);
	esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler);
	esp_wifi_deinit();
	esp_netif_destroy_default_wifi(netif);
}

uint32_t get_ip() {
	esp_netif_ip_info_t ip_info{};
    switch (_wifi_mode) {
    case WIFI_MODE_AP:
        esp_netif_get_ip_info(_ap_netif, &ip_info);
        break;
    case WIFI_MODE_STA:
    case WIFI_MODE_APSTA:
        esp_netif_get_ip_info(_sta_netif, &ip_info);
        break;
    default: break;
    }
	return ip_info.ip.addr;
}

std::string get_ip_str() {
    std::string ip_str = {};
    ip_str.resize(16);
    uint32_t ip_addr = get_ip();
    inet_ntoa_r(ip_addr, ip_str.data(), ip_str.size());
	return ip_str;
}

Mode get_mode() {
    return _wifi_mode;
}

State state() {
	return _state;
}

namespace Station {

int get_rssi() {
	wifi_ap_record_t ap_info;
	esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
	return ret == ESP_OK ? ap_info.rssi : -100;
}

bool is_connected() {
	return esp_netif_is_netif_up(_sta_netif);
}

void set_connect_cb(Callback cb) {
	_connect_cb = cb;
}

void set_disconnect_cb(Callback cb) {
	_disconnect_cb = cb;
}

void disconnect() {
	_auto_reconnect = false;
	esp_wifi_disconnect();
}

void connect(std::string_view ssid, std::string_view pswd) {
	disconnect();
	_auto_reconnect = true;
	_state = State::WAITTING;
	wifi_sta_config(ssid, pswd);
	esp_wifi_connect();
}

void connect() {
    if (WiFi::Store::is_provisioned())
        connect(WiFi::Store::read_ssid(), WiFi::Store::read_pswd());
}

State provision(std::string_view ssid, std::string_view pswd, uint32_t timeout_ms) {
	connect(ssid, pswd);
    for(int i = 0; i < 10; i++) {
		int ret = state() != State::CONNECTED ? -1 : 0;
		if(ret >= 0)
			break;
        vTaskDelay(pdMS_TO_TICKS(timeout_ms / 10));
	}
	switch (state()) {
    case State::CONNECTED:
        WiFi::Store::write(ssid, pswd);
        break;
    default:
        // 否则回滚到之前的配置
        if (WiFi::Store::is_provisioned())
            wifi_sta_config(WiFi::Store::read_ssid(), WiFi::Store::read_pswd());
        else
            disconnect();
        break;
	}
	ESP_LOGW(TAG, "provision %s", stateString(state()));
	return state();
}

void init() {
	if (_sta_netif || _ap_netif)
		return;
	_sta_netif = esp_netif_create_default_wifi_sta();
	wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, NULL));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    _wifi_mode = WIFI_MODE_STA;
    esp_wifi_start();
    if (WiFi::Store::is_provisioned()) {
        connect(WiFi::Store::read_ssid(), WiFi::Store::read_pswd());
    }
    ESP_LOGI(TAG, "Station init finished.");
}

void deinit() {
    wifi_deinit(_sta_netif);
	_sta_netif = nullptr;
    _wifi_mode = WIFI_MODE_NULL;
}

} /* namespace WiFi::Station */


namespace Softap {

void init(std::string_view ssid, std::string_view pswd) {
	if (_sta_netif || _ap_netif)
		return;
	_ap_netif = esp_netif_create_default_wifi_ap();
	wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, NULL));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    _wifi_mode = WIFI_MODE_AP;
    wifi_ap_config(ssid, pswd);
    esp_wifi_start();
    ESP_LOGI(TAG, "Softap init finished.");
}

void deinit() {
    wifi_deinit(_ap_netif);
	_ap_netif = nullptr;
    _wifi_mode = WIFI_MODE_NULL;
}

} /* namespace WiFi::Softap */


namespace Apsta {

int get_rssi() {
	wifi_ap_record_t ap_info;
	esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
	return ret == ESP_OK ? ap_info.rssi : -100;
}

bool is_connected() {
	return esp_netif_is_netif_up(_sta_netif);
}

void set_connect_cb(Callback cb) {
	_connect_cb = cb;
}

void set_disconnect_cb(Callback cb) {
	_disconnect_cb = cb;
}

void disconnect() {
	_auto_reconnect = false;
	esp_wifi_disconnect();
}

void connect(std::string_view ssid, std::string_view pswd) {
	disconnect();
	_auto_reconnect = true;
	_state = State::WAITTING;
	wifi_sta_config(ssid, pswd);
	esp_wifi_connect();
}

void connect() {
    if (WiFi::Store::is_provisioned())
        connect(WiFi::Store::read_ssid(), WiFi::Store::read_pswd());
}

State provision(std::string_view ssid, std::string_view pswd, uint32_t timeout_ms) {
	connect(ssid, pswd);
    for(int i = 0; i < 10; i++) {
		int ret = state() != State::CONNECTED ? -1 : 0;
		if(ret >= 0)
			break;
        vTaskDelay(pdMS_TO_TICKS(timeout_ms / 10));
	}
	switch (state()) {
    // 在超时时间内连上, 就写入nvs
    case State::CONNECTED :
        WiFi::Store::write(ssid, pswd);
        break;
    // 否则回滚到之前的配置
    default:
        // 之前有配过网就沿用, 否则不再重试连接
        if (WiFi::Store::is_provisioned())
            wifi_sta_config(WiFi::Store::read_ssid(), WiFi::Store::read_pswd());
        else
            disconnect();
        break;
	}
	ESP_LOGW(TAG, "provision %s", stateString(state()));
	return state();
}

void init(std::string_view ssid, std::string_view pswd) {
	if (_sta_netif || _ap_netif)
		return;
    _ap_netif = esp_netif_create_default_wifi_ap();
    _sta_netif = esp_netif_create_default_wifi_sta();
	wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, NULL));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    _wifi_mode = WIFI_MODE_APSTA;
    wifi_ap_config(ssid, pswd);
    esp_wifi_start();
    if (WiFi::Store::is_provisioned()) {
        connect(WiFi::Store::read_ssid(), WiFi::Store::read_pswd());
    }
#if IP_NAPT
    {
        // Enable DNS (offer) for dhcp server
        dhcps_offer_t dhcps_dns_value = OFFER_DNS;
        esp_netif_dhcps_option(_ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &dhcps_dns_value, sizeof(dhcps_dns_value));

        // Set custom dns server address for dhcp server
        esp_netif_dns_info_t dnsserver;
        dnsserver.ip.u_addr.ip4.addr = esp_ip4addr_aton(WrapperConfig::WIFI_DNS);
        dnsserver.ip.type = ESP_IPADDR_TYPE_V4;
        esp_netif_set_dns_info(_ap_netif, ESP_NETIF_DNS_MAIN, &dnsserver);

        // !!! 必须启动esp_wifi_start后再设置，否则ap无网络
        esp_netif_napt_enable(_ap_netif);
        ESP_LOGI(TAG, "NAPT is enabled.");
    }
#endif
    ESP_LOGI(TAG, "Apsta init finished.");
}

void deinit() {
    wifi_deinit(_ap_netif);
    esp_netif_destroy_default_wifi(_sta_netif);
	_ap_netif = nullptr;
    _sta_netif = nullptr;
    _wifi_mode = WIFI_MODE_NULL;
}

} /* namespace WiFi::Apsta */

void netif_init() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
}

} /* namespcae WiFi */

} /* namespcae Wrapper */
