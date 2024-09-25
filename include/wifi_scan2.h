#pragma once

#include "wifi2.h"
#include "bufdef.h"

//本模块依赖于wifi2的connect和disconnect函数
namespace wifi_scan {

OBuf get_wifi_scan_list(uint8_t max_record_num, size_t size = UINT32_MAX);
OBuf get_wifi_scan_list_new(uint8_t max_record_num, size_t size = UINT32_MAX);
	
} // namespace wifi_scan
