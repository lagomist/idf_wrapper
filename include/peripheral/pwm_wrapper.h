#pragma once

#include <stdint.h>

class PwmWrapper {
public:
	PwmWrapper() {}
	~PwmWrapper();

	int init(int pin, uint32_t freq_hz, float dc = 0);

	// 设置占空比 dc = 0.0 ～ 1.0
	void setDutyCycle(float dc);
	float getDutyCycle();
	// 渐灭
	void fade(float dc = 0.5f, uint32_t time_ms = 5000);
	void stop();
private:
	uint8_t _channel = 0;
	float _duty_cycle = 0;
};
