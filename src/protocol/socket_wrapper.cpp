#include "socket_wrapper.h"
#include "esp_log.h"
#include <lwip/sockets.h>
#include <stdio.h>        // for perror
#include <arpa/inet.h>    // for inet_addr
#include <unistd.h>       // for close

#define MAX_CONNECT_NUM 10

constexpr static const char* TAG = "socket_wrapper" ;

int TCPSocket::server(uint16_t port) {
	int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if (fd < 0) {
		perror("socket");
		return -1;
	}

	int optval = 1;
	/* 解除端口占用 */
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
		perror("setsockopt\n");
		return -1;
	}

	struct sockaddr_in server_addr = {
		.sin_family = AF_INET,
		.sin_port = htons(port),
		.sin_addr = {
		.s_addr = htonl(INADDR_ANY),
		},
	};

	if (bind(fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) < 0) {
		perror("bind");
		::close(fd);
		return -1;
	}

	if(listen(fd, MAX_CONNECT_NUM) < 0) {
		perror("listen");
		::close(fd);
		return -1;
	}

	_sockfd = fd;

	return _sockfd;
}

int TCPSocket::client(const char *ip, uint16_t port) {
	int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if (fd < 0){
		perror("socket");
		return -1;
	}

	struct sockaddr_in server_addr {
		.sin_family = AF_INET,
		.sin_port = htons(port),
		.sin_addr = {
		.s_addr = inet_addr(ip),
		},
	};

	if (::connect(fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) < 0) {
		perror("connect");
		::close(fd);
		return -1;
	}

	_sockfd = fd;

	return _sockfd;
}

int TCPSocket::accept(int server_fd) {
	struct sockaddr_in client_addr = {0};
	socklen_t addrlen = sizeof(struct sockaddr);
	int fd = ::accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
	if(fd < 0) {
		perror("accept");
		::close(fd);
		return -1;
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
	char addr[32];
	if (client_addr.sin_family == PF_INET) {
		inet_ntoa_r(client_addr.sin_addr, addr, sizeof(addr) - 1);
		ESP_LOGI(TAG, "socket accepted ip address: %s\n", addr);
	}

	_sockfd = fd;

	return _sockfd;
}

int TCPSocket::recv(void *rx_buf, int buf_len, int timeval_usec) {
	int fp0 = 0;
	int maxfd = (_sockfd > fp0) ? (_sockfd + 1) : (fp0 + 1);
	struct timeval timeout = {
		.tv_sec = timeval_usec / 1000000,
		.tv_usec = timeval_usec % 1000000,
	};
	fd_set readset;
	FD_ZERO(&readset);
	FD_SET(_sockfd, &readset);
	if (select(maxfd, &readset, NULL, NULL, (timeval_usec < 0) ? NULL : &timeout) < 0) {
		perror("select");
		return -1;
	}
	if (FD_ISSET(_sockfd, &readset)) {
		int recv_bytes = ::recv(_sockfd, rx_buf, buf_len, MSG_DONTWAIT);
		if (recv_bytes < 0){
			perror("recv");
			return -1;
		}
		return recv_bytes;
	}

	return 0;
}

int TCPSocket::send(const void *tx_buf, int buf_len) {
	for (int to_send = buf_len; to_send > 0;){
		int sent_bytes = ::send(_sockfd, (uint8_t *)tx_buf + (buf_len - to_send), to_send, MSG_DONTWAIT);
		if (sent_bytes < 0) {
			perror("send");
			return -1;
		}
		to_send -= sent_bytes;
	}
	return buf_len;
}

int TCPSocket::close() {
	::shutdown(_sockfd, SHUT_RDWR);
	::close(_sockfd);
	_sockfd = 0;
	return 0;
}

int TCPSocket::fd() {
	return _sockfd;
}
