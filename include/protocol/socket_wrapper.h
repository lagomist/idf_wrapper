#pragma once

#include <stdint.h>

class TCPSocket {
public:
	int server(uint16_t port);
	int client(const char* ip, uint16_t port);
	int accept(int server_fd);
	int recv(void *rx_buf, int buf_len, int timeval_usec = 1000000);
	int send(const void *tx_buf, int buf_len);
	int close();
	int fd();
private:
	int _sockfd = 0;
};

class UDPSocket {
public:
	int server(const char* ip, uint16_t port);
	int client(const char* ip, uint16_t port);
	int accept(int server_fd);
	int recv(void *rx_buf, int buf_len, int timeval_usec = 1000000);
	int send(const void *tx_buf, int buf_len);
	int close();
	int fd();
private:
	int _sockfd = 0;
};
