#include "gpio2.h"

bool GPI::_isr_service_installed = false;

GPI::GPI(uint8_t pin, int8_t pullmode, gpio_int_type_t int_type, Callback cb) :
GPIBase(pin), _cb(cb) {
	gpio_config_t io_conf = {
		.pin_bit_mask = 1ULL << pin,
		.mode         = GPIO_MODE_INPUT,
		.pull_up_en   = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type    = int_type,
	};
	if (pullmode == 1)
		io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
	else if (pullmode == 0)
		io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
	gpio_config(&io_conf);
}

int GPI::get() {
	return gpio_get_level((gpio_num_t)_pin);
}

void GPI::isr_enable() {
	isr_service_install();
	if (!_isr_handler_added) {
		gpio_isr_handler_add((gpio_num_t)_pin, isr_handler_adapter, this);
		_isr_handler_added = true;
	}
	gpio_intr_enable((gpio_num_t)_pin);
}

void GPI::isr_disable() {
	gpio_intr_disable((gpio_num_t)_pin);
}

void GPI::isr_service_install() {
	//install once for all gpio objects. if already installed, this func ignores
	if (!_isr_service_installed) {
		gpio_install_isr_service(0);
		_isr_service_installed = true;
	}
}

void GPI::isr_handler_adapter(void* arg) {
	reinterpret_cast<GPI*>(arg)->_cb();
}

GPO::GPO(uint8_t pin, int8_t pullmode) :
GPOBase(pin) {
	gpio_config_t io_conf = {
		.pin_bit_mask     = 1ULL << pin,
		.mode             = GPIO_MODE_OUTPUT,
		.pull_up_en       = GPIO_PULLUP_DISABLE,
		.pull_down_en     = GPIO_PULLDOWN_DISABLE,
		.intr_type        = GPIO_INTR_DISABLE,
	};
	if(pullmode == 1) {
		io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
	}
	else if(pullmode == 0) {
		io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
	}
	gpio_config(&io_conf);
	if(pullmode >= 0)
		gpio_set_level((gpio_num_t)_pin, (uint32_t)pullmode);
}
void GPO::set(uint8_t level) {
	gpio_set_level((gpio_num_t)_pin, level);
}

int GPO::get() {
	return gpio_get_level((gpio_num_t)_pin);
}
