#pragma once

#include "bufdef.h"
#include "type_traits2.h"
#include <mbedtls/md5.h>
#include <stdarg.h>
#include <string_view>
#include <functional>

namespace Wrapper {

namespace Utils {

template <typename T = uint16_t, T POLY = 0x8005, T INIT = 0x0000, T XOROUT = 0x0000, bool REFIN = true, bool REFOUT = true>
class CRC {
public:
    static T encode(const void* buf, size_t len);
private:
    template <typename R>
    static R ref(R data);
};

template <typename T, T POLY, T INIT, T XOROUT, bool REFIN, bool REFOUT>
template <typename R>
R CRC<T, POLY, INIT, XOROUT, REFIN, REFOUT>::ref(R data) {
  R temp = 0;
  for(uint8_t i = 0; i < sizeof(R) * 8; i++)
    temp |= ((data >> i) & 1) << (sizeof(R) * 8 - 1 - i);
  return temp;
}

template <typename T, T POLY, T INIT, T XOROUT, bool REFIN, bool REFOUT>
T CRC<T, POLY, INIT, XOROUT, REFIN, REFOUT>::encode(const void* buf, size_t len){
  uint8_t data;
  T crc = INIT;
  for (unsigned j = 0; j < len; j++) {
    data = ((uint8_t*)buf)[j];
    if(REFIN)
      data = ref(data);
    crc = crc ^ (data << (sizeof(T) - 1)*8);
    for (int i = 0; i < 8; i++){
      if (crc & (1U << (sizeof(T) * 8 - 1)))
        crc = (crc << 1) ^ POLY;
      else
        crc <<= 1;
    }
  }
  if(REFOUT)
    crc = ref(crc);
  crc = crc ^ XOROUT;
  return crc;
}

class MD5_Context {
public:
	using MD5 = std::array<uint8_t, 16>;
	MD5_Context() {
		mbedtls_md5_init(&_ctx);
	}
	void update(const uint8_t* data, size_t len) {
		mbedtls_md5_update(&_ctx, data, len);
	}
	MD5 finish() {
		MD5 md5 = {};
		mbedtls_md5_finish(&_ctx, md5.data());
		return md5;
	}
private:
	mbedtls_md5_context _ctx;
};

constexpr uint32_t BKDR_hash(const char str[], uint32_t len) {
	uint32_t hash = 0;
	for(const char* p = str;p < str+len;p++)
		hash = hash*131 + *p;
	return hash;
}

constexpr uint32_t BKDR_hash(const char str[]) {
	uint32_t hash = 0;
	for(;*str;str++)
		hash = hash*131 + *str;
	return hash;
}

constexpr uint32_t BKDR_hash(std::string_view str) {
	uint32_t hash = 0;
	for(auto e : str)
		hash = hash*131 + e;
	return hash;
}


long strntol(const char nptr[], size_t size, char *endptr[], int base);
double strntod(const char nptr[], size_t size, char *endptr[]);

size_t next_power2(size_t n);

uint32_t digest32(uint8_t *input, size_t input_len);

std::string time_tostring(uint64_t target_time);

std::string hex_tostring(std::string_view buf);
std::string string_tohex(std::string_view buf);

void rand(void* buf, size_t size);
long rand(long from = 0, long to = 999999l);

OBuf vsprint(const char format[], va_list args)  __attribute__((__format__ (__printf__, 1, 0)));
OBuf snprint(const char* format, ...) __attribute__((__format__ (__printf__, 1, 2)));

template <typename T>
constexpr static OBuf format(const T& val);

template <typename T, size_t ... seq>
constexpr static OBuf format(const T& val, std::index_sequence<seq ...>) {
	OBuf fmt = ((format(*(val.begin() + seq)) + Utils::snprint(",")) + ...);
	fmt.pop_back();
	return fmt;
}

template <typename T, size_t ... seq>
constexpr static OBuf format_tuple(const T& val, std::index_sequence<seq ...>) {
	OBuf fmt = ((std::get<seq>(val) + Utils::snprint(",")) + ...);
	fmt.pop_back();
	return fmt;
}

template <typename T>
constexpr static OBuf format(const T& val) {
	if constexpr (std::is_same_v<T, float>)
		return Utils::snprint("%.3f", val);
	else if constexpr (std::is_same_v<T, int8_t> || std::is_same_v<T, int>)
		return Utils::snprint("%d", val);
	else if constexpr (std::is_same_v<T, uint8_t>)
		return Utils::snprint("%u", val);
	else if constexpr (std::is_same_v<T, int32_t>)
		return Utils::snprint("%ld", val);
	else if constexpr (std::is_same_v<T, uint32_t>)
		return Utils::snprint("%lu", val);
	else if constexpr (type_traits2::is_array<T>::value)
		return format(val, std::make_index_sequence<type_traits2::is_array<T>::Nm> {});
	else if constexpr (type_traits2::is_tuple<T>::value)
		return format_tuple(val, std::make_index_sequence<std::tuple_size<T>::value> {});
	else {
		static_assert(!std::is_same_v<T, T>);
	}
}
  
} // namespace Utils 

}

// out of namespace
constexpr uint32_t operator "" _hash(const char* str, size_t n) {
	return Wrapper::Utils::BKDR_hash(str);
}
