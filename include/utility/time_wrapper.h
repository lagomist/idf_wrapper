#pragma once

#include <string>

namespace Wrapper {

namespace Time {

uint64_t microsecond();
uint64_t millisecond();
uint32_t second();

std::string date();
std::string get_current_time();

// 手动授时，返回-1说明s这个时间戳错误
int serve(uint32_t s, bool force = false);
// 初始化 sntp
void init();

} /* namespace Time */

} /* namespace Wrapper */
