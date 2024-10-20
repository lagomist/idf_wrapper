#include "socket_wrapper.h"
#include "wrapper_config.h"

#include "esp_log.h"
#include <lwip/sockets.h>
#include <arpa/inet.h>    // for inet_addr
#include <unistd.h>       // for close

namespace SocketWrapper {

constexpr static const char* TAG = "socket_wrapper" ;

Socket::Socket(Protocol proto) {
	_protocol = proto;
	switch (_protocol) {
	case Protocol::TCP :
		this->_sockfd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		break;
	case Protocol::UDP :
		this->_sockfd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		break;
	default:
		break;
	}
}

Socket::~Socket() {
	::close(this->_sockfd);
}

int Socket::bind(uint16_t port) {
	struct sockaddr_in sock_addr;
	memset(&sock_addr, 0, sizeof(sockaddr_in));
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_port = htons(port);
	sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	return ::bind(this->_sockfd, (struct sockaddr *)&sock_addr, sizeof(struct sockaddr));
}

int Socket::recv(void *rx_buf, int buf_len) {
	if (this->_sockfd < 0) return this->_sockfd;
	if (this->_protocol == Protocol::UDP) return -1;
	// Keep receiving until we have a reply
	int len = ::recv(this->_sockfd, rx_buf, buf_len, 0);
	
	return len;
}

int Socket::recvfrom(OBuf rx_buf, std::string& ip, uint16_t& port) {
	if (this->_sockfd < 0) return this->_sockfd;
	struct sockaddr_in source_addr; 
	socklen_t socklen = sizeof(source_addr);
	int len = ::recvfrom(this->_sockfd, rx_buf.data(), rx_buf.size(), 0, (struct sockaddr *)&source_addr, (socklen_t *)&socklen);
	if (len > 0) {
		ip = inet_ntoa(source_addr.sin_addr);
		port = source_addr.sin_port;
	}
	
	return len;
}

int Socket::send(const void *tx_buf, int buf_len) {
	if (this->_sockfd < 0) return this->_sockfd;
	if (this->_protocol == Protocol::UDP) return -1;
    int to_write = buf_len;
    while (to_write > 0) {
		int written = ::send(this->_sockfd, (uint8_t *)tx_buf + (buf_len - to_write), to_write, 0);
        if (written < 0) {
            return -2;
        }
        to_write -= written;
    }
    return buf_len;
}

int Socket::sendto(std::string_view ip, uint16_t port, IBuf data) {
	if (this->_sockfd < 0) return this->_sockfd;
    int to_write = data.size();
	/* 目标地址设置 */
    struct sockaddr_in dest_addr;
	memset(&dest_addr, 0, sizeof(sockaddr_in));
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(port);
	dest_addr.sin_addr.s_addr = inet_addr(ip.data());
	
    while (to_write > 0) {
		int written = ::sendto(this->_sockfd, data.data() + (data.size() - to_write), to_write, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (written < 0) {
            return -1;
        }
        to_write -= written;
    }
    return data.size();
}



/* ---------------- Server ---------------- */

int Server::init(uint16_t port) {
	if (this->_socket.protocol() == Protocol::TCP) {
		int optval = 1;
		/* 解除端口占用 */
		if (setsockopt(this->_socket.fd(), SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
			ESP_LOGE(TAG, "Setting reuseaddr error.");
			return -2;
		}
	} else {
		// 设置socket为广播
		int broadcast_enable = 1;
		if (setsockopt(this->_socket.fd(), SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
			ESP_LOGE(TAG, "Setting broadcast error.");
			return -2;
		}
	}

	if (this->_socket.bind(port) < 0) {
		ESP_LOGE(TAG, "Server bind error.");
		return -3;
	}

	if (this->_socket.protocol() == Protocol::TCP) {
		if (::listen(this->_socket.fd(), 3) < 0) {
			ESP_LOGE(TAG, "Server listen error.");
			return -4;
		}
	}

	return this->_socket.fd();
}

int Server::accept() {
	if (this->_socket.protocol() != Protocol::TCP) {
		return -1;
	}
	struct sockaddr_in client_addr;
	socklen_t addrlen = sizeof(struct sockaddr);
	int fd = ::accept(this->_socket.fd(), (struct sockaddr *)&client_addr, &addrlen);
	if(fd < 0) {
		return -2;
	}
	// Set tcp keepalive option
	int keepAlive = 1;          // 开启keepalive属性
	int keepIdle = 5;           // 如该连接在5秒内没有任何数据往来,则进行探测
	int keepInterval = 5;       // 探测时发包的时间间隔为5秒
	int keepCount = 3;          // 探测尝试的次数. 如果第1次探测包就收到响应了,则后几次探测包不再发送.
	setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepAlive, sizeof(keepAlive));
	setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, (void *)&keepIdle, sizeof(keepIdle));
	setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, (void *)&keepInterval, sizeof(keepInterval));
	setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, (void *)&keepCount, sizeof(keepCount));

	return fd;
}

void Server::stop() {
	::shutdown(this->_socket.fd(), SHUT_RD);
}




/* ---------------- Client ---------------- */

int Client::init(uint16_t port) {
	if (this->_socket.bind(port) < 0) {
		ESP_LOGE(TAG, "Client socket bind error.");
		return -1;
	}
	if (this->_socket.protocol() == Protocol::UDP) {
		// 设置套接字选项以启用地址重用
		int reuseEnable = 1;
		setsockopt(this->_socket.fd(), SOL_SOCKET, SO_REUSEADDR, &reuseEnable, sizeof(reuseEnable));

		// Enable broadcasting
		int broadcast_enable = 1;
		setsockopt(this->_socket.fd(), SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable));
	}

	return this->_socket.fd();
}

int Client::connect(std::string_view ip, uint16_t port) {
	if (this->_socket.protocol() != Protocol::TCP){
		return -1;
	}

	int fd = this->_socket.fd();
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = inet_addr(ip.data());

	if (::connect(fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) < 0) {
		return -2;
	}

	return fd;
}

void Client::shutdown() {
	::shutdown(this->_socket.fd(), SHUT_RDWR);
}


int socketSend(int sock, const void* data, int len) {
    int to_write = len;
    while (to_write > 0) {
		int written = ::send(sock, (uint8_t *)data + (len - to_write), to_write, 0);
        if (written < 0) {
            return -1;
        }
        to_write -= written;
    }
    return len;
}

} /* namespace SocketWrapper */
