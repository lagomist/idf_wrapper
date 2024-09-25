#include "mqtt_app_cert_download.h"
#include "mqtt.h"
#include "reboot.h"
#include "dev_desc.h"
#include "utility.h"
#include "ArduinoJson.h"
#include "string.h"
#include <esp_log.h>

namespace mqtt_app_cert_download {

constexpr static const char TAG[] = "mqtt_app_cert_download";

static int _msg_id = 0;

static DynamicJsonDocument _cert_info(4096);

static void subscribe_cb(int msg_id) {
	if (msg_id != _msg_id)
		return;
	mqtt::publish("$aws/certificates/create/json", IBuf{(uint8_t*)"{}", 3});
	ESP_LOGW(TAG, "request certificates");
}

static std::string certificates_create_json_accepted(std::string_view token) {
	DynamicJsonDocument doc(1024);
	JsonObject root = doc.to<JsonObject>();
	std::string out;
	auto parameters  = doc.createNestedObject("parameters");
	root["certificateOwnershipToken"] = token;
	parameters["SerialNumber"] = dev_desc::get_sn();
	serializeJson(doc, out);
	return out;
}

static void claim_provisioning(std::string_view cert, std::string_view priv_key) {
	dev_desc::write_mqtt_cert(cert, priv_key);
	reboot::delay_reboot(reboot::Reason::CERT_UPDATE, 100);
}

static void claim_provisioning_rejected_recv(std::string_view topic, IBuf data, int qos) {
	ESP_LOGE(TAG, "certificates create rejected [%.*s]", data.size(), data.data());
	_cert_info.clear();
}

static void claim_provisioning_accepted_recv(std::string_view topic, IBuf data, int qos) {
	ESP_LOGW(TAG, "claim provisioning accepted");
	if (_cert_info.size()) {
		claim_provisioning(_cert_info["certificatePem"], _cert_info["privateKey"]);
		_cert_info.clear();
	}
	else {
		ESP_LOGE(TAG, "certificates not create");
	}
}

static void create_rejected_recv(std::string_view topic, IBuf data, int qos) {
	ESP_LOGE(TAG, "claim provisioning rejected [%.*s]", data.size(), data.data());
	_cert_info.clear();
}

static void create_accepted_recv(std::string_view topic, IBuf data, int qos) {
	ESP_LOGW(TAG, "certificates create accepted");
	deserializeJson(_cert_info, data);
	auto json = certificates_create_json_accepted(_cert_info["certificateOwnershipToken"]);
	mqtt::publish("$aws/provisioning-templates/claim_provisioning/provision/json", {(uint8_t*)json.data(), json.size()});
}

static void subscribe_topic() {
    mqtt::subscribe("$aws/provisioning-templates/claim_provisioning/provision/json/rejected", claim_provisioning_rejected_recv);
    mqtt::subscribe("$aws/provisioning-templates/claim_provisioning/provision/json/accepted", claim_provisioning_accepted_recv);
    mqtt::subscribe("$aws/certificates/create/json/rejected", create_rejected_recv);
    _msg_id = mqtt::subscribe("$aws/certificates/create/json/accepted", create_accepted_recv);
}

void init() {
	mqtt::set_subscribed_cb(subscribe_cb);
	subscribe_topic();
}

void deinit() {
	mqtt::reset_subscribed_cb(subscribe_cb);
}

}
