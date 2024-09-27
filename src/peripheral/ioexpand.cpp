#include "ioexpand.h"
#include "c8tx.h"
#include "esp_log.h"

namespace ioexpand {

struct Config {
	C8Tx::GPIO gpio;
	uint8_t pin;
	C8Tx::Mode mode;
	C8Tx::Pull pull;
};

static const char TAG[] = "ioexpand";

static C8Tx* _dev = nullptr;

static std::vector<Config> _cfg;

GPI::GPI(uint8_t pin, int8_t pullmode) :
GPIBase(pin) {
	_cfg.push_back({C8Tx::GPIO(_pin >> 4), uint8_t(_pin & 0xF), C8Tx::GPIO_MODE_INPUT, C8Tx::Pull((pullmode < 0) ? 0 : (pullmode == 0) ? 2 : 1)});
}

int GPI::get() {
	spibus::mutex.lock();
	int ret = _dev->get(C8Tx::GPIO(_pin >> 4), _pin & 0xF);
	spibus::mutex.unlock();
	return ret;
}

GPO::GPO(uint8_t pin, int8_t pullmode) :
GPOBase(pin) {
	_cfg.push_back({C8Tx::GPIO(_pin >> 4), uint8_t(_pin & 0xF), C8Tx::GPIO_MODE_OUTPUT_PP, C8Tx::Pull((pullmode < 0) ? 0 : (pullmode == 0) ? 2 : 1)});
}

void GPO::set(uint8_t level) {
	spibus::mutex.lock();
	_dev->set(C8Tx::GPIO(_pin >> 4), _pin & 0xF, level);
	spibus::mutex.unlock();
}

int GPO::get() {
	spibus::mutex.lock();
	int ret = _dev->get(C8Tx::GPIO(_pin >> 4), _pin & 0xF);
	spibus::mutex.unlock();
	return ret;
}

CS::CS(uint8_t pin):
GPOBase(pin) {
	//CS脚应该设为上拉
	int8_t pullmode = 1;
	_cfg.push_back({C8Tx::GPIO(_pin >> 4), uint8_t(_pin & 0xF), C8Tx::GPIO_MODE_OUTPUT_PP, C8Tx::Pull((pullmode < 0) ? 0 : (pullmode == 0) ? 2 : 1)});
}

void CS::set(uint8_t level) {
	//高电平是去选中
	if(level)
		_dev->reset_cs();
	//低电平是选中
	else
		_dev->set(C8Tx::GPIO(_pin >> 4), _pin & 0xF, 0, true);
}

int init(C8Tx& c8tx) {
	_dev = &c8tx;
	spibus::mutex.lock();
	int ret = 0;
	if ((ret = _dev->init(1)) != 0) {
		ESP_LOGE(TAG, "ioexpand init failed");
		goto exit;
	}
	for (auto e : _cfg) {
		_dev->config(e.gpio, e.pin, e.mode, e.pull);
		// stm32 用 0 表示浮空, 用 1 表示上拉, 用 2 表示下拉
		// 如果是输出且非浮空则设置初始电平
		if (e.mode && e.pull)
			_dev->set(e.gpio, e.pin, !(e.pull - 1));
		ESP_LOGI(TAG, "set gpio [%d] pin [%d] to [%s] mode, pull [%d]", e.gpio, e.pin, e.mode ? "output" : "input", e.pull);
	}
	ESP_LOGI(TAG, "ioexpand init success");
	exit:
	spibus::mutex.unlock();
	return ret;
}

void deinit() {
	spibus::mutex.lock();
	_dev->deinit();
	spibus::mutex.unlock();
}

int init_check() {
	spibus::mutex.lock();
	int ret = _dev->init_check();
	spibus::mutex.unlock();
	return ret;
}

}
