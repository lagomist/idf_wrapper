#pragma once

#include "driver/gptimer.h"
#include <stdint.h>

namespace Wrapper {

class Timer {
public:
	using Callback = void(*)();
	Timer(Callback cb, uint32_t period_ms, bool reload = true);
	~Timer();
	void start();
	void restart(uint32_t period_ms);
	void stop();
private:
	static bool StaticOnAlarmCallback(gptimer_handle_t timer, const gptimer_alarm_event_data_t* edata, void* user_data) {
		// Cast user_data back to Timer*
		Timer* self = static_cast<Timer*>(user_data);
		self->_callback();
		return true;
	}
	gptimer_handle_t _handle;
	Callback _callback;
};


}
