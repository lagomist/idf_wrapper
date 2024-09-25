#pragma once

#include <esp_app_desc.h>
//定义了ota固件状态枚举
#include <esp_flash_partitions.h>
#include <string>

namespace ota_fw {

using std::string_view, std::string;

// ESP_OTA_IMG_NEW             = 0x0U,         /*!< Monitor the first boot. In bootloader this state is changed to ESP_OTA_IMG_PENDING_VERIFY. */
// ESP_OTA_IMG_PENDING_VERIFY  = 0x1U,         /*!< First boot for this app was. If while the second boot this state is then it will be changed to ABORTED. */
// ESP_OTA_IMG_VALID           = 0x2U,         /*!< App was confirmed as workable. App can boot and work without limits. */
// ESP_OTA_IMG_INVALID         = 0x3U,         /*!< App was confirmed as non-workable. This app will not selected to boot at all. */
// ESP_OTA_IMG_ABORTED         = 0x4U,         /*!< App could not confirm the workable or non-workable. In bootloader IMG_PENDING_VERIFY state will be changed to IMG_ABORTED. This app will not selected to boot at all. */
// ESP_OTA_IMG_UNDEFINED       = 0xFFFFFFFFU,  /*!< Undefined. App can boot and work without limits. */
using State = esp_ota_img_states_t;

constexpr std::string_view STATE_NOT_FOUND = "not found";

constexpr std::string_view state_str(State state) {
	switch (state) {
		case State::ESP_OTA_IMG_NEW: return "new";
		case State::ESP_OTA_IMG_PENDING_VERIFY: return "pending_verify";
		case State::ESP_OTA_IMG_VALID: return "valid";
		case State::ESP_OTA_IMG_INVALID: return "invalid";
		case State::ESP_OTA_IMG_ABORTED: return "aborted";
		default: return "undefined";
	}
}

//固件确认或回滚
int validate();
int rollback_and_reboot();

//ota固件状态获取
State get_state();
State get_state_other();

//固件版本
string fw_version();
// 如果另一个ota分区为空, 则返回"not found"
string fw_version_other();

string info();
	
} // namespace ota_fw
