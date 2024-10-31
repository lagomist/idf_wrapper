#include "time_wrapper.h"
#include <esp_sntp.h>
#include <esp_log.h>
#include <time.h>
#include <atomic>

namespace Wrapper {

namespace Time {

constexpr static const char TAG[] = "Wrapper::Time";

uint64_t microsecond() {
	timeval tv;
	gettimeofday(&tv, nullptr);
	return (uint64_t)(tv.tv_sec) * 1000000u + (uint64_t)(tv.tv_usec);
}
uint64_t millisecond() {
	timeval tv;
	gettimeofday(&tv, nullptr);
	return (uint64_t)(tv.tv_sec) * 1000u + (uint64_t)(tv.tv_usec) / 1000;
}
uint32_t second() {
	timeval tv;
	gettimeofday(&tv, nullptr);
	return tv.tv_sec;
}

std::string_view date() {
	time_t timep;
    time(&timep);
	return ctime(&timep);
}


int serve(uint32_t s, bool force) {
	// 授时不可逆转时间
	if (s <= second()) {
		ESP_LOGW(TAG, "manual timing is lagging, now(%lu) new(%lu)", second(), s);
		if (!force) {
			return -1;
		}
	}
	sntp_set_system_time(s, 0);
	return 0;
}


void init() {
	esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
	esp_sntp_setservername(0, "pool.ntp.org");
	sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
	esp_sntp_init();
	ESP_LOGI(TAG, "SNTP inited");
}

} /* namespace Time */

} /* namespace Wrapper */
