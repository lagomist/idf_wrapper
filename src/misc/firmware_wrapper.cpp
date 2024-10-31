#include "firmware_wrapper.h"
#include <esp_ota_ops.h>
#include <esp_log.h>
#include <string.h>

namespace Wrapper {

namespace Firmware {

static constexpr char TAG[] = "Wrapper::Firmware";

int validate() {
	State state = Firmware::state();
	if (state != State::ESP_OTA_IMG_PENDING_VERIFY) {
		ESP_LOGW(TAG, "fw state is %s, cannot validate", stateString(state).data());
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
	

int rollbackAndReboot() {
	auto err = esp_ota_mark_app_invalid_rollback_and_reboot();
	ESP_LOGI(TAG, "rollback and reboot %s", esp_err_to_name(err));
	return err == ESP_OK ? 0 : -1;
}

State state() {
	State ota_state = State::ESP_OTA_IMG_UNDEFINED;
	esp_ota_get_state_partition(esp_ota_get_running_partition(), &ota_state);
	return ota_state;
}

State aotherState() {
	State ota_state = State::ESP_OTA_IMG_UNDEFINED;
	esp_ota_get_state_partition(esp_ota_get_next_update_partition(NULL), &ota_state);
	return ota_state;
}

std::string version() {
	//app version will not change since sys running
	return esp_app_get_description()->version;
}

std::string anotherVersion() {
	char version[32];
	esp_app_desc_t desc{};
	memset(version, 0, sizeof(version));
	int err = esp_ota_get_partition_description(esp_ota_get_next_update_partition(NULL), &desc);
	strcpy(version, err == ESP_OK ? desc.version : "not found");
	return version;
}

std::string info() {
	return std::string("this,") + std::string(version()) + 
			std::string(",sta,") + std::string(stateString(state())) +
			std::string(",other,") + std::string(anotherVersion()) + 
			std::string(",sta,") + std::string(stateString(aotherState()));
}

} // namespace

}
