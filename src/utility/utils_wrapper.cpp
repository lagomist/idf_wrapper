#include "utils_wrapper.h"
#include <sha/sha_dma.h>
#include <esp_random.h>
#include <string.h>	//memcpy
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

namespace Wrapper {

namespace Utils {

static inline int chtonum(int ch) {
  return isdigit(ch) ? (ch - '0') : isalpha(ch) ? (toupper(ch) - 'A' + 10) : -1;
}

long strntol(const char nptr[], size_t size, char *endptr[], int base) {
  char buf[size + 1];
  if (size < sizeof(buf))
    memcpy(buf, nptr, size);
  buf[size] = '\0';
  long res = strtol(buf, endptr, base);
  if (endptr)
    *endptr = (char *)nptr + (*endptr - buf);
  return res;
}

double strntod(const char nptr[], size_t size, char *endptr[]) {
  char buf[size + 1];
  if (size < sizeof(buf))
    memcpy(buf, nptr, size);
  buf[size] = '\0';
  double res = strtod(buf, endptr);
  if (endptr)
    *endptr = (char *)nptr + (*endptr - buf);
  return res;
}
    
uint32_t digest32(uint8_t *input, size_t input_len) {
  uint8_t hash[32];
  esp_sha(SHA2_256, input, input_len, hash);
  uint32_t prefix = 0;
  memcpy(&prefix, hash, 4);
  return prefix;
}

std::string time_tostring(uint64_t target_time) {
  char time_str[20];
	time_t time = target_time / 1000;
	struct tm *t1 = localtime(&time); // 转换为当地时间
	strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t1); // 转换为字符串格式：年-月-日 时:分:秒
  return time_str;
}

std::string hex_tostring(std::string_view buf) {
  std::string out;
  for (int i = 0; i < buf.size(); i++) {
    out += "0123456789ABCDEF"[buf[i] >> 4];
    out += "0123456789ABCDEF"[buf[i] & 0x0F];
  }
  return out;
}

std::string string_tohex(std::string_view buf) {
  std::string out;
  for (int i = 0; i < buf.size(); i += 2) {
    int h = chtonum(buf[i]), l = chtonum(buf[i + 1]);
    if (h < 0 || l < 0)
      return {};
    out += (h << 4) | l;
  }
  return out;
}

size_t next_power2(size_t n) {
  --n;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
  return ++n;
}

void rand(void* buf, size_t size) {
	esp_fill_random(buf, size);
}

long rand(long from, long to) {
	uint32_t random;
  rand(&random, sizeof(random));
	return from + (random % (to - from));
}


OBuf vsprint(const char format[], va_list args) {
  OBuf out;
  // 前两个参数传空值可以拿到序列化后的长度, 这样就可以根据这个长度申请动态内存作为序列化缓冲区
  va_list args_copy;
  va_copy(args_copy, args);
  int len = vsnprintf(nullptr, 0, format, args_copy);
  if (len < 0)
    return out;
  out.reserve(len + 1);
	out.resize(len);
  vsnprintf((char*)out.data(), out.capacity(), format, args);
  return out;
}

OBuf snprint(const char format[], ...) {
	va_list args;
	va_start(args, format);
	auto out = vsprint(format, args);
	va_end(args);
	return out;
}

} // namespace utility

}
