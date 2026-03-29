#include "spi_wrapper.h"
#include "driver/spi_master.h"
#include <cstring>

#define _hd ((spi_device_handle_t)_device)
#define _phd ((spi_device_handle_t *)&_device)

namespace Wrapper {

namespace SPI {

void init(uint8_t host, int mosi, int miso, int clk, bool enable_dma, int max_trans_sz) {
	spi_bus_config_t buscfg = {
		.mosi_io_num = mosi,
		.miso_io_num = miso,
		.sclk_io_num = clk,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		// if 0, default, 4096, but it won't allocate 4096 bytes from heap in the beginning.
		// I've tested. the heap useage only inrease 20bytes from setting to 128 to 4096.
		.max_transfer_sz = max_trans_sz,
	};
	// Initialize the SPI bus
	ESP_ERROR_CHECK(spi_bus_initialize((spi_host_device_t)host, &buscfg, enable_dma ? SPI_DMA_CH_AUTO : SPI_DMA_DISABLED));
}

void deinit(uint8_t host) {
	spi_bus_free((spi_host_device_t)host);
}


int Device::init(uint32_t freq_mhz, int cs_io_num, uint8_t mode, uint8_t addr_bit) {
	spi_device_interface_config_t devcfg = {
		.address_bits	= addr_bit,
		.mode			= mode,
		.clock_speed_hz	= int(freq_mhz * 1000000),
		.spics_io_num	= cs_io_num,
		.queue_size		= 7,
	};
	return spi_bus_add_device((spi_host_device_t)_host, &devcfg, _phd);
}

Device::~Device() {
	spi_bus_remove_device(_hd);
}

int Device::write(uint8_t addr, uint8_t data) {
	spi_transaction_t t = {
		.flags	= SPI_TRANS_USE_TXDATA,
		.addr	= addr,
		.length = 8,
	};
	t.tx_data[0] = data;
	if(spi_device_polling_transmit(_hd, &t) != ESP_OK)
		return -1;
	return 1;
}

int Device::write(uint8_t addr, const uint8_t data[], uint8_t len) {
	spi_transaction_t t = {
		.addr		= addr,
		.length		= uint32_t(8 * len),
		.tx_buffer	= data
	};
	if (spi_device_polling_transmit(_hd, &t) != ESP_OK)
		return -1;
	return len;
}

int Device::read(uint8_t addr, uint8_t& data_out) {
	spi_transaction_t t = {
		.flags	= SPI_TRANS_USE_RXDATA,
		.addr	= addr,
		.length = 8,
		.rxlength = 8,
		.tx_buffer = "\x00\x00\x00\x00"
	};
	if (spi_device_polling_transmit(_hd, &t) != ESP_OK)
		return -1;
	data_out = t.rx_data[0];
	return 1;
}

int Device::read(uint8_t addr, uint8_t data_out[], uint8_t len) {
	spi_transaction_t t;
    std::memset(&t, 0, sizeof(t));
	t.addr = addr;
    t.length = size_t(len) * 8;
	t.rxlength = size_t(len) * 8;
    t.tx_buffer = "\x00\x00\x00\x00";
	t.rx_buffer = data_out;

	if(spi_device_polling_transmit(_hd, &t) != ESP_OK)
		return -1;
	return len;
}

int Device::trans(const uint8_t tx_buf[], size_t tx_len, uint8_t rx_buf[], size_t rx_len) {
	spi_transaction_t t;
    std::memset(&t, 0, sizeof(t));
    t.length = tx_len * 8;
    t.tx_buffer = tx_buf;
	t.rxlength = rx_len * 8;
	t.rx_buffer = rx_buf;
	if(spi_device_polling_transmit(_hd, &t) != ESP_OK)
		return -1;
	return tx_len;
}


}

}
