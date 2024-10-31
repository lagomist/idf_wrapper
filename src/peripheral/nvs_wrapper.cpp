#include "nvs_wrapper.h"
// #include <nvs.h>
#include <nvs_flash.h>
#include <nvs_handle.hpp>
#include <esp_log.h>


namespace Wrapper {

namespace NVS {

constexpr static const char TAG[] = "Wrapper::NVS";

static std::unique_ptr<nvs::NVSHandle> _handle = nullptr;

static int open(std::string_view part_name) {
	int err = -1;
	if (part_name.empty()) return 0;
	_handle = nvs::open_nvs_handle(part_name.data(), NVS_READWRITE, &err);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
		return -1;
	}
	ESP_LOGI(TAG, "nvs handle opened");
	return 0;
}

int erase(const char name[]) {
	if (getSize(name) < 0)
		return 0;
	int err = _handle->erase_item(name);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "erase of %s error: %s", name, esp_err_to_name(err));
		return -1;
	}
	ESP_LOGI(TAG, "erase %s success", name);
	return 0;
}

int write(const char name[], const void* buf, uint32_t len) {
	int err = _handle->set_blob(name, buf, len);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Writing %s error: %s", name, esp_err_to_name(err));
		return -1;
	}
	ESP_LOGI(TAG, "Writing %s done", name);
	return len;
}

int read(const char name[], void* buf, uint32_t size) {
	int err = -1;
	err = _handle->get_blob(name, buf, size);

	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Reading %s error: %s", name, esp_err_to_name(err));
		return -1;
	}
	ESP_LOGI(TAG, "Reading %s done", name);
	return size;
}

int getSize(const char name[]) {
	size_t size = 0;
	int err = _handle->get_item_size(nvs::ItemType::BLOB, name, size);
	if (err == ESP_ERR_NVS_NOT_FOUND) {
		ESP_LOGW(TAG, "not found %s", name);
		return -2;
	}
	else if (err != ESP_OK) {
		ESP_LOGE(TAG, "Getting size of %s error: %s", name, esp_err_to_name(err));
		return -1;
	}
	return size;
}

void traversal(std::string_view part_name, std::function<void(std::string_view)> pred) {
	nvs_iterator_t it = NULL;
	esp_err_t res = nvs_entry_find(part_name.data(), nullptr, NVS_TYPE_ANY, &it);
	while (res == ESP_OK) {
		nvs_entry_info_t info;
		if (nvs_entry_info(it, &info) != ESP_OK)
			break;
		pred(info.key);
		res = nvs_entry_next(&it);
	}
	nvs_release_iterator(it);
}


int init(std::string_view part_name) {
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
	return open(part_name);
}

int deinit() {
	_handle.reset();
	return 0;
}

} // namespace nvs

}
