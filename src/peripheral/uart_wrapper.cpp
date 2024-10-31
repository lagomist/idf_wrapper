
#include "uart_wrapper.h"
#include <memory>
#include <cstring>
#include <driver/uart.h>

namespace Wrapper {

UART::UART(int uart_num, int tx_io_num, int rx_io_num, int baud_rate, uint8_t parity):
_uart_num(uart_num) {
	const uart_config_t uart_config = {
		.baud_rate	= baud_rate,
		.data_bits	= UART_DATA_8_BITS,
		.parity		= (uart_parity_t)parity,
		.stop_bits	= UART_STOP_BITS_1,
		.flow_ctrl	= UART_HW_FLOWCTRL_DISABLE,
		.rx_flow_ctrl_thresh = 0,
		.source_clk = UART_SCLK_DEFAULT,
		.flags 		= 0,
	};
	// We won't use a buffer for sending data.
	ESP_ERROR_CHECK(uart_param_config((uart_port_t)_uart_num, &uart_config));
	ESP_ERROR_CHECK(uart_set_pin((uart_port_t)_uart_num, tx_io_num, rx_io_num, -1, -1));
}

void UART::init(int rx_buf_len) {
	//driver must be installed when system is running
	_rx_buf_len = rx_buf_len;
	ESP_ERROR_CHECK(uart_driver_install((uart_port_t)_uart_num, _rx_buf_len, 0, 0, NULL, 0));
}

void UART::deinit() {
	if (uart_is_driver_installed((uart_port_t)_uart_num)) {
		ESP_ERROR_CHECK(uart_driver_delete((uart_port_t)_uart_num));
	}
}

int UART::write(const uint8_t buf[], uint32_t size) {
	//because i didn't allocate a tx buffer, this function will return only after push all data into tx fifo.
	//thus no need for wait_tx_done.
	return uart_write_bytes((uart_port_t)_uart_num, buf, size);
}

int UART::read(uint8_t buf[], uint32_t size, int timeout) {
	uint32_t to = timeout < 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeout);
	return uart_read_bytes((uart_port_t)_uart_num, buf, size, to);
}


int UART::flush() {
	return uart_flush((uart_port_t)_uart_num);
}

} // namespace Wrapper
