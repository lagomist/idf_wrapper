#include "i2c_wrapper.h"

#include "driver/i2c_master.h"

#define _hd ((i2c_master_dev_handle_t)_device)
#define _phd ((i2c_master_dev_handle_t *)&_device)

namespace Wrapper {

namespace I2C {

static i2c_master_bus_handle_t _handle[5];

void init(uint8_t host, int sda, int scl, bool inter_pullup) {
	if (host >= 4) return;
	i2c_master_bus_config_t buscfg = {
		.i2c_port = (i2c_port_num_t )host,
		.sda_io_num = (gpio_num_t )sda,
		.scl_io_num = (gpio_num_t )scl,
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.glitch_ignore_cnt = 0,
		.intr_priority = 0,
		.trans_queue_depth = 0,
		.flags = {.enable_internal_pullup = inter_pullup,},
	};
	ESP_ERROR_CHECK(i2c_new_master_bus(&buscfg, &_handle[host]));
}

bool isInited(uint8_t host) {
	if (host >= 4) return false;
	return _handle[host] != NULL;
}

void *getBusHandle(uint8_t host) {
	if (host >= 4) return nullptr;
	return _handle[host];
}

void deinit(uint8_t host) {
	i2c_del_master_bus(_handle[host]);
}

int Device::init(uint16_t addr, uint32_t freq_hz) {
	i2c_device_config_t devcfg = {
		.dev_addr_length	= I2C_ADDR_BIT_LEN_7,
		.device_address 	= addr,
		.scl_speed_hz		= freq_hz,
		.scl_wait_us		= 0,
		.flags = {.disable_ack_check = 0},
	};
	return i2c_master_bus_add_device(_handle[_host], &devcfg, _phd);
}

Device::~Device() {
	i2c_master_bus_rm_device(_hd);
}

int Device::write(uint8_t data) {
	return i2c_master_transmit(_hd, &data, 1, 100);
}

int Device::write(const uint8_t data[], uint32_t len) {
	return i2c_master_transmit(_hd, data, len, 100);
}

int Device::trans_recv(const uint8_t tx_buf[], size_t tx_len, uint8_t rx_buf[], size_t rx_len) {
	return i2c_master_transmit_receive(_hd, tx_buf, tx_len, rx_buf, rx_len, 200);
}

int Device::read(uint8_t data[], uint32_t len) {
	return i2c_master_receive(_hd, data, len, 200);
}

}

}
