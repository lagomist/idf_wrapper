#pragma once

#include "type_traits2.h"
#include <stdint.h>
#include <functional>


namespace nvs_pred {

using WritePred = int(*)(const char name[], const void* buf, uint32_t len);
using ReadPred = int(*)(const char name[], void* buf, uint32_t len);

template <typename T>
int write(WritePred pred, const char name[], const T& value) {
    int res = 0;
	// 匹配各种 stl 容器
	if constexpr (type_traits2::is_vector<T>::value || type_traits2::is_string_view<T>::value || type_traits2::is_string<T>::value)
		res = pred(name, (void*)value.data(), sizeof(typename T::value_type) * value.size());
	// 匹配非指针类型的 pod 数据
	else if constexpr (std::is_standard_layout_v<T> && !std::is_pointer_v<T>)
		res = pred(name, (void*)&value, sizeof(T));
	else
		static_assert(!std::is_same_v<T, T>, "unknown type");
	return res;
}

template <typename T>
int read(ReadPred pred, const char name[], T& value, uint32_t size) {
	int res = 0;	
	// 匹配各种 stl 容器
	if constexpr (type_traits2::is_vector<T>::value || type_traits2::is_string_view<T>::value || type_traits2::is_string<T>::value) {
		if (size % sizeof(typename T::value_type) != 0)
			return -2;
		value.resize(size / sizeof(typename T::value_type));
		res = pred(name, (void*)value.data(), (size_t)size);
	}
	// 匹配非指针类型的 pod 数据
	else if constexpr (std::is_standard_layout_v<T> && !std::is_pointer_v<T>) {
		res = pred(name, (void*)&value, (size_t)size);
	}
	else
		static_assert(!std::is_same_v<T, T>, "unknown type");
	return res;
}

} /* namespace nvs_pred */

//最大NVS名字15字节，不包含\0
namespace NvsWrapper {

int write(const char name[], const void* buf, uint32_t len);

int read(const char name[], void* buf, uint32_t len);

int erase(const char name[]);
//size in bytes
int getSize(const char name[]);

void traversal(std::string_view part_name, std::function<void(std::string_view)> pred);

int init(std::string_view part_name);

int deinit();

template <typename T>
int write(const char name[], const T& value) {
	return nvs_pred::write(write, name, value);
}

//return -1 or actual number of items
template <typename T>
int read(const char name[], T& value) {
	int size = getSize(name);
	if(size <= 0)
		return -1;
	return nvs_pred::read(read, name, value, size);
}

} 
