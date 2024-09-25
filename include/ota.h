#pragma once
#include <string>
#include <tuple>
#include "os_api.h"

namespace ota {

// ota状态机状态
enum class Status : uint8_t {
    IDLE,
    BEGIN,
	PROCESSING,
	DONE,
	SUCCESS,
	FAILURE
};

using std::string, std::string_view;
using Callback = void(*)();
using MD5 = std::array<uint8_t, 16>;

//命令
void start(string_view url, const MD5& md5 = {});
int abort();

//状态
Status status();
bool is_updating();
int get_ota_size();
uint8_t get_percentage();
//用于产测app
std::pair<int, double> download_size_speed();

//cfg.func会被覆盖，因为用的是本模块自己的任务函数
void init(const os::task::Cfg& task_cfg, Callback successful_cb = nullptr, Callback failed_cb = nullptr);
void set_ota_start_cb(Callback start_cb);
}
