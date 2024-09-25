#include "ping.h"
#include "os_api.h"
#include <ping/ping_sock.h>
#include <lwip/netdb.h>
#include <lwip/dns.h>
#include <esp_netif.h>
#include <esp_log.h>

static const char TAG[] = "ping";

static std::pair<int, ip_addr_t> parse_ip_addr(esp_netif_t *netif, std::string_view domain) {
	ip_addr_t ip_addr{};
	const struct addrinfo hints = {
			.ai_family = AF_INET,
			.ai_socktype = SOCK_STREAM,
	};
	addrinfo *res = nullptr;
	/* convert URL to IP, 0 on success, non-zero on failure */
	int err = getaddrinfo(domain.data(), NULL, &hints, &res);
	if (err == 0)
		inet_addr_to_ip4addr(ip_2_ip4(&ip_addr), &((sockaddr_in *)(res->ai_addr))->sin_addr);
	freeaddrinfo(res);
	ESP_LOGI(TAG, "%s,dns," IPSTR ",target," IPSTR ",err,%d,", 
		esp_netif_get_desc(netif),
		IP2STR((ip4_addr_t*)dns_getserver(0)),
		IP2STR((ip4_addr_t*)&ip_addr),
		err
	);
	return {err, ip_addr};
}

int Ping::init(void *netif, std::string_view domain) {
	if (inited())
		return 1;
	auto [err, target_addr] = parse_ip_addr((esp_netif_t *)netif, domain);
	if (err != 0)
		return -2;
	esp_ping_config_t config = {
		.count = 1,
		.interval_ms = 1000,
		.timeout_ms = 1000,
		.data_size = 64,
		.tos = 0,
		.ttl = IP_DEFAULT_TTL,
		.target_addr = target_addr,
		.task_stack_size = 3 * 1024,
		.task_prio = 2,
		.interface = (uint32_t)esp_netif_get_netif_impl_index((esp_netif_t *)netif),
	};
	/* set callback functions */
	esp_ping_callbacks_t cbs = {
		.cb_args = this,
		.on_ping_success = success_adapter,
		.on_ping_timeout = timeout_adapter,
		.on_ping_end = end_adapter,
	};
	return esp_ping_new_session(&config, &cbs, &_handle) == ESP_OK ? 0 : -1;
}

int Ping::deinit() {
	if (!inited())
		return 1;
	if (stop() != ESP_OK)
		return -1;
	if (esp_ping_delete_session(_handle) == ESP_OK)
		_handle = nullptr;
	return 0;
}

bool Ping::inited() {
	return _handle;
}

int Ping::start() {
	return esp_ping_start(_handle) == ESP_OK ? 0 : -1;
}

int Ping::stop() {
	return esp_ping_stop(_handle) == ESP_OK ? 0 : -1;
}

void Ping::success_adapter(void *handle, void *args) {
	// ESP_LOGI(TAG, "success");
	if (((Ping *)args)->_success_cb)
		((Ping *)args)->_success_cb(*(Ping *)args);
}

void Ping::timeout_adapter(void *handle, void *args) {
	// ESP_LOGI(TAG, "timeout");
	if (((Ping *)args)->_timeout_cb)
		((Ping *)args)->_timeout_cb(*(Ping *)args);
}

void Ping::end_adapter(void *handle, void *args) {
	// ESP_LOGI(TAG, "end");
	if (((Ping *)args)->_end_cb)
		((Ping *)args)->_end_cb(*(Ping *)args);
}

std::pair<int, int> Ping::block_ping(void *netif, std::string_view domain, int retry) {
	int success = 0, timeout = 0;
	os::Semaphore sem;
	add_success_cb([&retry, &success](Ping& self) {
		--retry;
		++success;
	});
	add_timeout_cb([&retry, &timeout](Ping& self) {
		--retry;
		++timeout;
	});
	add_end_cb([&retry, &success, &sem](Ping& self) {
		// 如果还有机会就继续尝试
		if (retry)
			self.start();
		// 否则意味着机会都用完了, 就通知
		else
			sem.give();
	});
	if (init(netif, domain) < 0)
		goto exit;
	if (start() < 0)
		goto exit;
	sem.take(retry * 3000);
	ESP_LOGI(TAG, "%s,success,%d,timeout,%d", esp_netif_get_desc((esp_netif_t *)netif), success, timeout);
	// 不管怎样都释放会话
	exit:
	deinit();
	return {success, timeout};
}
