#include "timer_wrapper.h"


namespace Wrapper {

Timer::Timer(Callback cb, uint32_t period_ms, bool reload): _callback(cb) {
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, // 1MHz, 1 tick=1us
		.intr_priority = 0,
		.flags = {
			.intr_shared = 0,
			.backup_before_sleep = 0,
		}
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &_handle));
	gptimer_event_callbacks_t cbs = {
        .on_alarm = StaticOnAlarmCallback,
    };
	gptimer_register_event_callbacks(_handle, &cbs, this);

    gptimer_enable(_handle);
	gptimer_alarm_config_t alarm_config = {
        .alarm_count = period_ms * 1000,
        .reload_count = 0,
        .flags = {
			.auto_reload_on_alarm = reload
		},
    };
    gptimer_set_alarm_action(_handle, &alarm_config);
}

Timer::~Timer() {
    gptimer_stop(_handle);
	gptimer_disable(_handle);
    gptimer_del_timer(_handle);
}

void Timer::start() {
    gptimer_start(_handle);
}

void Timer::restart(uint32_t period_ms) {
	gptimer_alarm_config_t alarm_config = {
        .alarm_count = period_ms * 1000,
        .reload_count = 0,
        .flags = {
			.auto_reload_on_alarm = true
		},
    };
    gptimer_set_alarm_action(_handle, &alarm_config);
    gptimer_start(_handle);
}

void Timer::stop() {
    gptimer_stop(_handle);
}

}
