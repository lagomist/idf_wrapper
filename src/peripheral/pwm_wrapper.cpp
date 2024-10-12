#include "pwm_wrapper.h"
#include <driver/ledc.h>
#include <algorithm>
#include <array>

static const char TAG[] = "pwm_wrapper";
// 10bit resolution, can be 10~13
static constexpr uint8_t PWM_RESOLUTION = 10;

static std::array<uint32_t, LEDC_TIMER_MAX> _tim_freq = {0};

static std::array<bool, LEDC_CHANNEL_MAX> _used_channel = {0};

PwmWrapper::~PwmWrapper() {
	if (_channel < LEDC_CHANNEL_MAX) {
		_used_channel[_channel] = false;
	}
	stop();
}

int PwmWrapper::init(int pin, uint32_t freq_hz, float dc) {
	_channel = LEDC_CHANNEL_MAX;
	_duty_cycle = dc;
	// 查找可用定时器。频率相等或等于0（未使用），说明timer可用，退出查找
	auto pfreq = std::find_if(_tim_freq.begin(), _tim_freq.end(), [freq_hz](auto value) {
		return value == freq_hz || value == 0;
	});
	// 没有可用定时器, 退出
	if (pfreq == _tim_freq.end()) return -1;

	// 查找可用通道
	for (uint8_t index = 0; index < _used_channel.size(); index ++) {
		if (_used_channel[index] == false) {
			_used_channel[index] = true;
			_channel = index;
			break;
		}
	}
	if (_channel == LEDC_CHANNEL_MAX) return -2;

	ledc_timer_t timer_num = ledc_timer_t(pfreq - _tim_freq.begin());

	// 频率为0说明定时器未初始化，先初始化定时器
	if (*pfreq == 0) {
		ledc_timer_config_t ledc_timer = {
			.speed_mode = LEDC_LOW_SPEED_MODE,
			.duty_resolution = LEDC_TIMER_10_BIT,
			.timer_num = timer_num,
			.freq_hz = freq_hz,
			.clk_cfg = LEDC_AUTO_CLK
		};
		// Set configuration of timer0 for high speed channels
		assert(ledc_timer_config(&ledc_timer) == ESP_OK);
		*pfreq = freq_hz;
	}
	ledc_channel_config_t ledc_channel = {
		.gpio_num	= pin,
		.speed_mode = LEDC_LOW_SPEED_MODE,
		.channel	= (ledc_channel_t)_channel,
		.intr_type	= LEDC_INTR_DISABLE,
		.timer_sel	= timer_num,
		.duty		= uint32_t((1<<PWM_RESOLUTION) * dc),
		.flags		= {0},
	};
	// Set LED Controller with previously prepared configuration
	return ledc_channel_config(&ledc_channel);
}

void PwmWrapper::setDutyCycle(float dc) {
	ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)_channel, (1 << PWM_RESOLUTION) * dc);
	ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)_channel);
	_duty_cycle = dc;
}

float PwmWrapper::getDutyCycle() {
	return _duty_cycle;
}

void PwmWrapper::stop() {
	ledc_stop(LEDC_LOW_SPEED_MODE, (ledc_channel_t)_channel, 1);
}

void PwmWrapper::fade(float dc, uint32_t time_ms) {
	ledc_set_fade_time_and_start(LEDC_LOW_SPEED_MODE,
								(ledc_channel_t)_channel,
								(1 << PWM_RESOLUTION) * dc,
								time_ms,
								LEDC_FADE_WAIT_DONE);
}
