#include "spi.h"

#include "esp_log.h"
#include "driver/spi_master.h"

#define _hd ((spi_device_handle_t)_device)
#define _phd ((spi_device_handle_t *)&_device)

namespace spibus {

os::Mutex mutex;

void init(uint8_t host, uint8_t mosi, uint8_t miso, uint8_t clk, bool enable_dma) {
	spi_bus_config_t buscfg = {
		.mosi_io_num = mosi,
		.miso_io_num = miso,
		.sclk_io_num = clk,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		//if 0, default, 4096, but it won't allocate 4096 bytes from heap in the beginning.
		//I've tested. the heap useage only inrease 20bytes from setting to 128 to 4096.
		// .max_transfer_sz = 0,
	};
	//Initialize the SPI bus
	ESP_ERROR_CHECK(spi_bus_initialize((spi_host_device_t)host, &buscfg, enable_dma ? SPI_DMA_CH_AUTO : SPI_DMA_DISABLED));
}

void deinit(uint8_t host) {
	spi_bus_free((spi_host_device_t)host);
}
} // namespace spibus


int SPIDevice1::init(uint32_t freq_MHz, int cs_io_num, uint8_t mode) {
	spi_device_interface_config_t devcfg = {
		.address_bits	= 8,
		.mode			= mode,
		.clock_speed_hz	= int(freq_MHz * 1000000),
		.spics_io_num	= cs_io_num,
		.queue_size		= 7,
	};
	return spi_bus_add_device((spi_host_device_t)_host, &devcfg, _phd);
}

SPIDevice1::~SPIDevice1() {
	spi_bus_remove_device(_hd);
}

int SPIDevice1::write(uint8_t addr, uint8_t data) {
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

int SPIDevice1::write(uint8_t addr, const uint8_t data[], uint8_t len) {
	spi_transaction_t t = {
		.addr		= addr,
		.length		= uint32_t(8 * len),
		.tx_buffer	= data
	};
	if (spi_device_polling_transmit(_hd, &t) != ESP_OK)
		return -1;
	return len;
}

int SPIDevice1::read(uint8_t addr, uint8_t& data_out) {
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

uint8_t SPIDevice1::read(uint8_t addr) {
	uint8_t data = 0;
	read(addr, data);
	return data;
}

int SPIDevice1::read(uint8_t addr, uint8_t data_out[], uint8_t len) {
	spi_transaction_t t = {
		.addr		= addr,
		.length		= size_t(len) * 8,
		.rxlength	= size_t(len) * 8,
		.tx_buffer = "\x00\x00\x00\x00",
		.rx_buffer	= data_out
	};
	if(spi_device_polling_transmit(_hd, &t) != ESP_OK)
		return -1;
	return len;
}

int SPIDevice2::init(uint32_t freq_MHz, int cs_io_num, uint8_t mode) {
	spi_device_interface_config_t devcfg = {
		.command_bits	= 8,
		.address_bits	= 8,
		.mode			= mode,
		.clock_speed_hz = int(freq_MHz * 1000000),
		.spics_io_num	= cs_io_num,
		.queue_size		= 7
	};
	return spi_bus_add_device((spi_host_device_t)_host, &devcfg, _phd);
}

SPIDevice2::~SPIDevice2() {
	spi_bus_remove_device(_hd);
}

int SPIDevice2::write(uint8_t addr, uint8_t cmd, uint8_t data) {
	spi_transaction_t t = {
		.flags	= SPI_TRANS_USE_TXDATA,
		.cmd	= addr,
		.addr	= cmd,
		.length = 8,
	};
	t.tx_data[0] = data;
	if (spi_device_polling_transmit(_hd, &t) != ESP_OK)
		return -1;
	return 1;
}

int SPIDevice2::write(uint8_t addr, uint8_t cmd, const uint8_t data[], uint8_t len) {
	spi_transaction_t t = {
		.cmd		= addr,
		.addr		= cmd,
		.length		= uint32_t(8*len),
		.tx_buffer	= data
	};
	if (spi_device_polling_transmit(_hd, &t) != ESP_OK)
		return -1;
	return len;
}

int SPIDevice2::read(uint8_t addr, uint8_t cmd, uint8_t& data_out) {

	spi_transaction_t t = {
		.flags = SPI_TRANS_USE_RXDATA,
		.cmd		= addr,
		.addr		= cmd,
		.length		= 8,
		.rxlength	= 8,
		.tx_buffer = "\x00\x00\x00\x00"
	};
	if (spi_device_polling_transmit(_hd, &t) != ESP_OK)
		return -1;
	data_out = t.rx_data[0];
	return 1;
}

uint8_t SPIDevice2::read(uint8_t addr, uint8_t cmd) {
	uint8_t data = 0;
	read(addr, cmd, data);
	return data;
}

int SPIDevice2::read(uint8_t addr, uint8_t cmd, uint8_t data_out[], uint8_t len) {
	spi_transaction_t t = {
		.cmd		= addr,
		.addr		= cmd,
		.length		= size_t(len) * 8,
		.rxlength	= size_t(len) * 8,
		.tx_buffer = "\x00\x00\x00\x00",
		.rx_buffer	= data_out
	};
	if (spi_device_polling_transmit(_hd, &t) != ESP_OK)
		return -1;
	return len;
}

int SPIDevice3::init(uint32_t freq_MHz, int cs_io_num, uint8_t mode) {
	spi_device_interface_config_t devcfg = {
		.command_bits	= 0,
		.address_bits	= 0,
		.mode			= mode,
		.clock_speed_hz = int(freq_MHz * 1000000),
		.spics_io_num	= cs_io_num,
		.queue_size		= 7
	};
	return spi_bus_add_device((spi_host_device_t)_host, &devcfg, _phd);
}

SPIDevice3::~SPIDevice3() {
	spi_bus_remove_device(_hd);
}

int SPIDevice3::write(const uint8_t data[], int len) {
	spi_transaction_t t = {
		.length    = (size_t)len * 8,
		.tx_buffer = data,
	};
	if (spi_device_polling_transmit(_hd, &t) != ESP_OK)
		return -1;
	return len;
}

int SPIDevice3::read(uint8_t data[], int len) {
	spi_transaction_t t = {
		.length    = (size_t)len * 8,
		.rxlength  = (size_t)len * 8,
		.tx_buffer = "\x00\x00\x00\x00",
		.rx_buffer = data
	};
	if (spi_device_polling_transmit(_hd, &t) != ESP_OK)
		return -1;
	return len;
}


// BL0910
int SPIDevice4::init(uint32_t freq_MHz, int cs_io_num, uint8_t mode) {
	spi_device_interface_config_t devcfg = {
		.command_bits	= 8,
		.address_bits	= 8,
		.mode			= mode,
		.clock_speed_hz = int(freq_MHz * 1000000),
		.spics_io_num	= cs_io_num,
		.queue_size		= 7
	};
	return spi_bus_add_device((spi_host_device_t)_host, &devcfg, _phd);
}

SPIDevice4::~SPIDevice4() {
	spi_bus_remove_device(_hd);
}

int SPIDevice4::write(uint8_t cmd, uint8_t addr, const uint8_t data[], uint8_t len) {
	spi_transaction_t t = {
		.cmd		= cmd,
		.addr		= addr,
		.length		= uint32_t(8*len),
		.tx_buffer	= data
	};
	if (spi_device_polling_transmit(_hd, &t) != ESP_OK)
		return -1;
	return len;
}

int SPIDevice4::read(uint8_t cmd, uint8_t addr, uint8_t data_out[], uint8_t len) {
	spi_transaction_t t = {
		.cmd		= cmd,
		.addr		= addr,
		.length		= size_t(len) * 8,
		.rxlength	= size_t(len) * 8,
		.tx_buffer = "\x00\x00\x00\x00",
		.rx_buffer	= data_out
	};
	if (spi_device_polling_transmit(_hd, &t) != ESP_OK)
		return -1;
	return len;
}


