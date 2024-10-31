#pragma once

#include <string_view>

namespace Wrapper {

namespace Time {

uint64_t microsecond();
uint64_t millisecond();
uint32_t second();

std::string_view date();

// 手动授时，返回-1说明s这个时间戳错误
int serve(uint32_t s, bool force = false);
// 初始化 sntp
void init();

} /* namespace Time */

} /* namespace Wrapper */
