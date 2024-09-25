#include "ota_fw.h"
#include <esp_ota_ops.h>
#include <esp_log.h>
#include <string.h>

namespace ota_fw {
static constexpr char TAG[] = "ota_fw";

int validate() {
	State state = get_state();
	if (state != State::ESP_OTA_IMG_PENDING_VERIFY) {
		ESP_LOGW(TAG, "fw state is %s, cannot validate", state_str(state).data());
		return -1;
	}
	auto err = esp_ota_mark_app_valid_cancel_rollback();
	if (err != ESP_OK) {
		ESP_LOGW(TAG, "mark_app_valid failed %s", esp_err_to_name(err));
		return -1;
	}
	ESP_LOGI(TAG, "fw verified, rollback cancelled");
	return 0;
}
	

int rollback_and_reboot() {
	auto err = esp_ota_mark_app_invalid_rollback_and_reboot();
	ESP_LOGI(TAG, "rollback and reboot %s", esp_err_to_name(err));
	return err == ESP_OK ? 0 : -1;
}

State get_state() {
	State ota_state = State::ESP_OTA_IMG_UNDEFINED;
	esp_ota_get_state_partition(esp_ota_get_running_partition(), &ota_state);
	return ota_state;
}

State get_state_other() {
	State ota_state = State::ESP_OTA_IMG_UNDEFINED;
	esp_ota_get_state_partition(esp_ota_get_next_update_partition(NULL), &ota_state);
	return ota_state;
}

string fw_version() {
	//app version will not change since sys running
	return esp_app_get_description()->version;
}

string fw_version_other() {
	char version[32];
	esp_app_desc_t desc{};
	memset(version, 0, sizeof(version));
	int err = esp_ota_get_partition_description(esp_ota_get_next_update_partition(NULL), &desc);
	strcpy(version, err == ESP_OK ? desc.version : STATE_NOT_FOUND.data());
	return version;
}

string info() {
	return string("this,") + string(fw_version()) + 
			string(",sta,") + string(state_str(get_state())) +
			string(",other,") + string(fw_version_other()) + 
			string(",sta,") + string(state_str(get_state_other()));
}

} // namespace ota_fw
