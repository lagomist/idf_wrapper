#include <string_view>
#include <functional>
#include <utility>

class Ping {
	public:
	using Callback = std::function<void(Ping&)>;
	int init(void *netif, std::string_view domain);
	int deinit();
	bool inited();
	int start();
	int stop();
	void add_success_cb(Callback cb) { _success_cb = cb; };
	void add_timeout_cb(Callback cb) { _timeout_cb = cb; };
	void add_end_cb(Callback cb) { _end_cb = cb; };
	// 返回成功次数, 超时次数
	std::pair<int, int> block_ping(void *netif, std::string_view domain, int retry = 3);

	private:
	void *_handle = nullptr;
	Callback _success_cb = nullptr;
	Callback _timeout_cb = nullptr;
	Callback _end_cb = nullptr;
	static void success_adapter(void *handle, void *args);
	static void timeout_adapter(void *handle, void *args);
	static void end_adapter(void *handle, void *args);
};
