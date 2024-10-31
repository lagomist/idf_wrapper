#pragma once

#include <cstdint>

namespace Wrapper {

// UART内部自己用了信号量和锁，确保线程安全。
class UART {
public:
	// parity 0表示无，2表示偶校验位，3表示奇校验位
	UART(int uart_num, int tx_io_num = -1, int rx_io_num = -1, int baud_rate = 115200, uint8_t parity = 0);

	//esp ring buffer requires rx_buf_len must > 128
	void init(int rx_buf_len = 129);
	void deinit();

	// C风格接口
	int write(const uint8_t buf[], uint32_t size);
	//max_len must <= RX_BUF_LEN
	int read(uint8_t buf[], uint32_t size, int timeout = -1);

	int flush();
private:
	int _uart_num;
	int _rx_buf_len = 129;
};

}
