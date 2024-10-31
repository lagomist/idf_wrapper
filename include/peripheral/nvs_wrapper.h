#pragma once

#include "type_traits2.h"
#include <stdint.h>
#include <functional>


namespace Wrapper {

// 最大NVS名字15字节，不包含\0
namespace NVS {

int write(const char name[], const void* buf, uint32_t len);

int read(const char name[], void* buf, uint32_t len);

int erase(const char name[]);
//size in bytes
int getSize(const char name[]);

void traversal(std::string_view part_name, std::function<void(std::string_view)> pred);

int init(std::string_view part_name);

int deinit();

template <typename T>
int write(const char name[], const T& value);

//return -1 or actual number of items
template <typename T>
int read(const char name[], T& value);

} 

}
