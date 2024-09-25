#include "ota.h"
#include "ota_fw.h"
#include "utility.h"
#include <atomic>
#include <esp_https_ota.h>
#include <esp_crt_bundle.h>
#include <esp_log.h>
//for unique_ptr
#include <memory>

namespace ota {

static constexpr char TAG[] = "ota";

//结构体较大，设为动态分配，节省内部ram
struct OTAData {
	OTAData(std::string_view _url, const MD5& _md5) : url(_url), md5(_md5) {}

	std::string url;
	MD5 md5 = {};
	utility::MD5_Context md5_ctx = {};

	//用于计算进度百分比
	std::atomic_int32_t last_len = 0;
	std::atomic_int32_t total_len = 0;

	//以下两项用于调试
	std::atomic<float> speed = 0;

	Status status = Status::BEGIN;
	esp_https_ota_handle_t https_ota_handle = nullptr;
	//上次下载完一段数据的时间戳，用于计算下载速度
	uint64_t last_timestamp = 0;
	bool is_image_verified = false;
};

static os::task::Cfg _task_cfg;
static Callback _successful_cb = +[](){};
static Callback _failed_cb = +[](){};
static std::atomic<Callback> _start_cb = nullptr;

static os::task::Task _task_handle;
static std::unique_ptr<OTAData> _ota_data = nullptr;

#ifdef PROJECT_SMBNA //电表版夹子低功耗处理。
static std::atomic<uint32_t> _delay_ms = 8000;
#endif

static void reset() {
	_ota_data.reset();
	ESP_LOGI(TAG, "Delete OTA task");
	_task_handle.del();
}

void set_ota_start_cb(Callback start_cb){
	if(start_cb)
		_start_cb = start_cb;
}

static void calc_speed(int len) {
	uint64_t now = os::time_ms();
	_ota_data->speed = (float)(len - _ota_data->last_len) / ((now - _ota_data->last_timestamp) / 1000.0f);
	#ifdef PROJECT_SMBNA
	if(_ota_data->speed < 5000)
		_delay_ms = 10;
	else if(_ota_data->speed < 10000)
		_delay_ms = 4000;
	else
		_delay_ms = 8000;
	#endif
	ESP_LOGI(TAG, "download %d bytes, speed %.2f bytes/s", len, _ota_data->speed.load());
	_ota_data->last_timestamp = now;
}

static int validate_image_header(decrypt_cb_arg_t *args, void *user_ctx) {
	if (_ota_data->is_image_verified)
		return 0;

	const int app_desc_offset = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t);

	if (args->data_in_len < app_desc_offset + sizeof(esp_app_desc_t)) {
		ESP_LOGE(TAG, "header length err, exp %d but %d", app_desc_offset, args->data_in_len);
		return -1;
	}

	esp_app_desc_t *app_info = (esp_app_desc_t *) &args->data_in[app_desc_offset];

	if (app_info->magic_word != ESP_APP_DESC_MAGIC_WORD) {
        ESP_LOGE(TAG, "Incorrect app descriptor magic, exp [%08X] but [%08lX]", ESP_APP_DESC_MAGIC_WORD, app_info->magic_word);
        return -1;
    }

	if (std::string_view{app_info->project_name} != std::string_view{PROJECT_NAME}) {
		ESP_LOGE(TAG, "project not match, exp [%s] but [%.*s]", PROJECT_NAME, sizeof(app_info->project_name), app_info->project_name);
		return -1;
	}

	ESP_LOGI(TAG, "Running fw version [%s] New fw version [%s]", ota_fw::fw_version().data(), app_info->version);

	_ota_data->is_image_verified = true;
	_ota_data->total_len = esp_https_ota_get_image_size(_ota_data->https_ota_handle);
	// 第一包计算速度需要在这初始化时间戳
	_ota_data->last_timestamp = os::time_ms();

	return 0;
}

static esp_err_t decrypt_cb(decrypt_cb_arg_t *args, void *user_ctx) {
	if (validate_image_header(args, user_ctx) < 0)
		return ESP_FAIL;

	// esp_https_ota_perform returns after every read operation
	int len = esp_https_ota_get_image_len_read(_ota_data->https_ota_handle);
	if (len < 0) {
		ESP_LOGE(TAG, "esp_https_ota_get_image_len_read failed");
		return -1;
	}

	// 避免使用减法
	if (len > _ota_data->last_len + 50 * 1024) {
		calc_speed(len);
		_ota_data->last_len = len;
	}

	_ota_data->md5_ctx.update((uint8_t*)args->data_in, args->data_in_len);

	args->data_out = (char*)malloc(args->data_in_len);

	if (args->data_out == nullptr)
		return ESP_ERR_NO_MEM;

	memcpy(args->data_out, args->data_in, args->data_in_len);

	args->data_out_len = args->data_in_len;

	return ESP_OK;
}

static int begin() {
	// http_client_config
	esp_http_client_config_t http_config = {
		.url = _ota_data->url.c_str(),
		.timeout_ms = 10000,
		.buffer_size = 2048,
		.skip_cert_common_name_check = true,
		.crt_bundle_attach = esp_crt_bundle_attach,
		.keep_alive_enable = true,
	};

	// https_ota_config
	esp_https_ota_config_t ota_config = {
		.http_config = &http_config,
		.decrypt_cb = decrypt_cb,
	};
	// ota_begin
	if (esp_err_t err = esp_https_ota_begin(&ota_config, &_ota_data->https_ota_handle); err != ESP_OK) {
		ESP_LOGE(TAG, "ESP HTTPS OTA Begin failed, ERR:%s", esp_err_to_name(err));
		return -1;
	} 

	return 0;
}

static int processing() {
	esp_err_t err = ESP_ERR_HTTPS_OTA_IN_PROGRESS;
	// ota开始执行
	#ifdef PROJECT_SMBNA //电表版夹子低功耗处理。
	int count = 0;
	#endif
	while ((err = esp_https_ota_perform(_ota_data->https_ota_handle)) == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
		#ifdef PROJECT_SMBNA //电表版夹子低功耗处理。
		count++;
		if(count >=100){
			os::delay(_delay_ms);
			count=0;
		}
		#endif
		if (os::task::sem::take(0) >= 0) {
			ESP_LOGW(TAG, "user abort");
			return -1;
		}
	}

	ESP_LOGW(TAG, "processing ret %s", esp_err_to_name(err));

	return err != ESP_OK ? -1 : 0;
}

static int done() {
	// 380d41587e9fca69dfdacb326bc62df9
	if (esp_https_ota_is_complete_data_received(_ota_data->https_ota_handle) != true) {
		// the OTA image was not completely received and user can customise the response to this situation.
		ESP_LOGE(TAG, "Complete data was not received.");
		return -1;
	}
	
	//如果保存的md5值为空，就不进行md5校验
	if (_ota_data->md5 != MD5{} && _ota_data->md5 != _ota_data->md5_ctx.finish()) {
		ESP_LOGE(TAG, "md5 check error");
		return -1;
	}

	if (esp_err_t err = esp_https_ota_finish(_ota_data->https_ota_handle); err != ESP_OK) {
		// esp_https_ota_finish 已经释放了句柄
		_ota_data->https_ota_handle = nullptr;
		ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed %s", esp_err_to_name(err));
		return -1;
	}

	ESP_LOGI(TAG, "Image download successful.");
	return 0;
}

static void success() {
	_successful_cb(); // ota成功回调
	reset();
}

static void failure() {
	if (_ota_data->https_ota_handle) {
		esp_https_ota_abort(_ota_data->https_ota_handle);
	}
	_failed_cb(); // ota失败回调
	reset();
}

static void ota_task(void *arg) {
	if (begin() < 0)
		goto err;
	_ota_data->status = Status::PROCESSING;
	if (processing() < 0)
		goto err;
	_ota_data->status = Status::DONE; // ota结束
	if (done() < 0)
		goto err;
	_ota_data->status = Status::SUCCESS; // ota成功
	success();
	err:
	_ota_data->status = Status::FAILURE; // ota失败
	failure();
}

void start(string_view url, const MD5& md5) {
	if (is_updating()) {
		ESP_LOGW(TAG, "already start");
		return;
	}
	std::string https_url = {url.data(), url.size()};
	if (url[4] == ':') { // https
		https_url = "https" + std::string(url.substr(4, url.size()));
	}
	ESP_LOGD(TAG, "https url: %s", https_url.c_str());
	//申请内存，并初始化
	_ota_data = std::make_unique<OTAData>(https_url, md5);

	if(!_ota_data) {
		ESP_LOGE(TAG, "memory allocation failed");
		return;
	}
	if(_start_cb)
		_start_cb();
	_task_handle.create(_task_cfg);
}

bool is_updating() {
	return _ota_data != nullptr;
}

int abort() {
	if (!_task_handle.is_inited()) {
		ESP_LOGW(TAG, "task not init");
		return -1;
	}
	os::task::sem::give(_task_handle);
	return 0;
}

uint8_t get_percentage() {
	if(!_ota_data || _ota_data->total_len == 0)
		return 100;
	return (_ota_data->last_len * 100) / _ota_data->total_len;
}

int get_ota_size() {
	if(!_ota_data)
		return 0;
	return _ota_data->last_len;
}

std::pair<int, double> download_size_speed() {
	if(!_ota_data)
		return {0, 0};
	return {esp_https_ota_get_image_len_read(_ota_data->https_ota_handle), _ota_data->speed};
}

Status status() {
	if(!_ota_data)
		return Status::IDLE;
	return _ota_data->status;
}

void init(const os::task::Cfg& task_cfg, Callback successful_cb, Callback failed_cb) {
	_task_cfg = task_cfg;
	_task_cfg.func = ota_task;
	if(successful_cb)
		_successful_cb = successful_cb;
	if(failed_cb)
		_failed_cb = failed_cb;
}

}
