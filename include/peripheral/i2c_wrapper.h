#pragma once

#include <stdint.h>
#include <cstddef>

namespace Wrapper {

namespace I2C {

void init(uint8_t host, int sda, int scl, bool inter_pullup = true);
bool isInited(uint8_t host);
void *getBusHandle(uint8_t host);
void deinit(uint8_t host);


class Device {
public:
	Device(uint8_t host) : _host(host) {}
	~Device();
	int init(uint16_t addr, uint32_t freq_hz = 400000);
	int write(uint8_t data);
	int write(const uint8_t data[], uint32_t len);
	int trans_recv(const uint8_t tx_buf[], size_t tx_len, uint8_t rx_buf[], size_t rx_len);
	int read(uint8_t data[], uint32_t len);
protected:
	uint8_t _host;
	void* _device = nullptr;
};

}

} 
