#pragma once

#include <stdint.h>

namespace Wrapper {

namespace SPI {

void init(uint8_t host, uint8_t mosi, uint8_t miso, uint8_t clk, bool enable_dma = true);
void deinit(uint8_t host);


class Device {
public:
	Device(uint8_t host) : _host(host) {}
	~Device();
	int init(uint32_t freq_mhz, int cs_io_num, uint8_t mode = 0);
	int write(uint8_t addr, uint8_t data);
	int write(uint8_t addr, const uint8_t data[], uint8_t len);
	int read(uint8_t addr, uint8_t& data_out);
	int read(uint8_t addr, uint8_t data[], uint8_t len);
protected:
	uint8_t _host;
	void* _device = nullptr;
};

}

} 
