#pragma once

#include <driver/gpio.h>

namespace Wrapper {

class GPIBase {
public:
	GPIBase(uint8_t pin) : _pin(pin) {}
	virtual int get() = 0;
protected:
	const uint8_t _pin;
};

class GPI : public GPIBase {
public:
	using Callback = void (*)();
	// pullmode: 0-down, 1-up, -1-none
	GPI(uint8_t pin, int8_t pullmode = 0, gpio_int_type_t int_type = GPIO_INTR_DISABLE, Callback cb = nullptr);
	int get();
	void isr_enable();
	void isr_disable();
	static void isr_service_install();
private:
	static void isr_handler_adapter(void* arg);
	static bool _isr_service_installed;
	Callback _cb = nullptr;
	bool _isr_handler_added = false;
};

class GPOBase {
public:
	// pullmode: 0-down, 1-up, -1-none
	GPOBase(uint8_t pin) : _pin(pin) {}
	virtual void set(uint8_t level = 1) = 0;
	virtual int get() = 0;
	void reset() {set(0);}
protected:
	uint8_t _pin;
};

class GPO : public GPOBase {
public:
	// pullmode: 0-down, 1-up, -1-none
	GPO(uint8_t pin, int8_t pullmode = 0);
	void set(uint8_t level = 1);
	int get();
	void toggle() {set(!get());}
};

}
