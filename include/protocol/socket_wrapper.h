#pragma once

#include "bufdef.h"
#include <stdint.h>
#include <string>

/**
 * @brief 套接字驱动，包含TCP、UDP下的服务器与客户端模式，固定堵塞接收、发送
 * 
 */
namespace SocketWrapper {

enum class Protocol : uint8_t {
	TCP,
	UDP
};

int socketSend(int sock, const void* data, int len);

class Socket {
public:
	Socket(Protocol proto);
	Socket(int fd) : _sockfd(fd) {
		_protocol = Protocol::TCP;
	}
	~Socket();
	
	int recv(void *rx_buf, int buf_len);
	int send(const void *tx_buf, int buf_len);
	int recvfrom(OBuf rx_buf, std::string& ip, uint16_t& port);
	int sendto(std::string_view ip, uint16_t port, IBuf data);
	int bind(uint16_t port);

	int fd() {
		return _sockfd;
	}
	Protocol protocol() {
		return _protocol;
	}
private:
	int _sockfd;
	Protocol _protocol;
};

class Server {
public:
	Server(Protocol proto) : _socket(proto) {}
	~Server() {}
	int init(uint16_t port);
	int accept();
	void stop();
	int fd() {
		return _socket.fd();
	}
	Socket socket() {
		return _socket;
	}
private:
	Socket _socket;
};


class Client {
public:
	Client(Protocol proto) : _socket(proto) {}
	~Client() {}
	int init(uint16_t port);
	int connect(std::string_view ip, uint16_t port);
	void shutdown();
	int fd() {
		return _socket.fd();
	}
	Socket socket() {
		return _socket;
	}
private:
	Socket _socket;
};

}
