#pragma once

#include <cstdint>
#include <cstddef>

namespace Wrapper {

namespace SPI {

void init(uint8_t host, int mosi, int miso, int clk, bool enable_dma = true, int max_trans_sz = 4096);
void deinit(uint8_t host);


class Device {
public:
	Device(uint8_t host) : _host(host) {}
	~Device();
	int init(uint32_t freq_mhz, int cs_io_num, uint8_t mode = 0, uint8_t addr_bit = 8);
	int write(uint8_t addr, uint8_t data);
	int write(uint8_t addr, const uint8_t data[], uint8_t len);
	int read(uint8_t addr, uint8_t& data_out);
	int read(uint8_t addr, uint8_t data[], uint8_t len);
	int trans(const uint8_t tx_buf[], size_t tx_len, uint8_t rx_buf[], size_t rx_len);
protected:
	uint8_t _host;
	void* _device = nullptr;
};

}

} 
