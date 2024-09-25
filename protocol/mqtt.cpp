#include "mqtt.h"
#include "os_api.h"
#include "utility.h"
#include <lwip/sockets.h>
#include <mqtt_client.h>
#include <esp_netif.h>
#include <esp_log.h>
#include <list>
#include <unordered_map>
#include <atomic>

namespace mqtt {

struct CliConfig {
	ifreq if_name{};
	esp_mqtt_client_config_t config;
};

constexpr static const char  TAG[] = "mqtt";

static esp_mqtt_client_handle_t _client = nullptr;
static CliConfig _client_config = {
	.config = {
		.network = {
			.if_name = &_client_config.if_name,
		}
	},
};

static std::atomic_bool _is_start = false;
static std::atomic_bool _connected = false;

static std::list<SubscribedCallback> _subscribed_cbs;
static std::list<std::pair<ConnCallback, void*>> _connected_cbs;
static std::list<std::pair<ConnCallback, void*>> _disconnected_cbs;

// 用 topic hash 索引回调的表
static std::unordered_map<uint32_t, RecvCallback> _received_cbs;

static os::RecursiveMutex _mutex;

static void connected_handler(esp_mqtt_event_t* event) {
	ESP_LOGI(TAG, "connected");
	_connected = true;
	for (auto& [cb, arg] : _connected_cbs)
		cb(arg);
}

//if auto reconnect is enabled, this will be called every 5 second
//if not, this will be called once
static void disconnected_handler(esp_mqtt_event_t* event) {
	//use bool to make sure this is called once
	if (!_connected) return;
	ESP_LOGI(TAG, "disconnected");
	_connected = false;
	for (auto& [cb, arg] : _disconnected_cbs)
		cb(arg);
}

static void before_connect_handler(esp_mqtt_event_t* event) {

}

static void received_handler(esp_mqtt_event_t* event) {
	ESP_LOGD(TAG, "received, msg_id=%d, topic=%.*s, data_len=%d", event->msg_id, event->topic_len, event->topic, event->data_len);
	string_view topic = string_view{event->topic, (size_t)event->topic_len};
	// 用topic生成键
	uint32_t hash = utility::BKDR_hash(topic);
	auto it = _received_cbs.find(hash);
	//topic and data are consequential in the same buffer
	if (it == _received_cbs.end()) {
		ESP_LOGE(TAG, "received,unsubscribe,%.*s,", topic.size(), topic.data());
		return;
	}
	auto recv_cb = it->second;
	recv_cb(topic, {(uint8_t*)event->data, (size_t)event->data_len}, event->qos);
}

static void subscribed_handler(esp_mqtt_event_t* event) {
	ESP_LOGI(TAG, "subscribed %d", event->msg_id);
	for (auto& cb : _subscribed_cbs)
		cb(event->msg_id);
}

static void published_handler(esp_mqtt_event_t* event) {
	ESP_LOGD(TAG, "published,%d,", event->msg_id);
}

static void event_handler_adapter(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
	// ESP_LOGI(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
	esp_mqtt_event_t* event = reinterpret_cast<esp_mqtt_event_t*>(event_data);
	switch(event->event_id) {
		case MQTT_EVENT_CONNECTED:
			connected_handler(event);
			break;
		case MQTT_EVENT_DISCONNECTED:
			disconnected_handler(event);
			break;
		case MQTT_EVENT_BEFORE_CONNECT:
			before_connect_handler(event);
			break;
		case MQTT_EVENT_DATA:
			received_handler(event);
			break;
		case MQTT_EVENT_SUBSCRIBED:
			subscribed_handler(event);
			break;
		case MQTT_EVENT_PUBLISHED:
			published_handler(event);
			break;
		default:
			break;
	}
}

// esp_mqtt_set_config 不能被锁住, 否则会导致死锁
static CliConfig update_config(bool auto_reconnect, int keepalive_s = 0, esp_netif_t* netif = nullptr) {
	std::lock_guard lg(_mutex);
	// 怎么着都要更新 disable_auto_reconnect
	_client_config.config.network.disable_auto_reconnect = !auto_reconnect;
	// 有值才更新 keepalive 和 netif
	if (keepalive_s)
		_client_config.config.session.keepalive = keepalive_s;

	if (netif)
		esp_netif_get_netif_impl_name(netif, _client_config.if_name.ifr_name);
	
	ESP_LOGW(TAG, "auto_reconnect,%d,keepalive,%d,netif,%s,",
		auto_reconnect,
		_client_config.config.session.keepalive, 
		_client_config.if_name.ifr_name
	);
	// 拷贝一份
	return _client_config;
}

Route route() {
	std::lock_guard lg(_mutex);
	if (!connected())
		return Route::NONE;
	switch (utility::BKDR_hash(_client_config.if_name.ifr_name, 2)) {
		case "st"_hash: return Route::WIFI;
		case "pp"_hash: return Route::MODEM;
		case "en"_hash: return Route::ETH;
		default: return Route::NONE;
	}
}

void connect(void* netif, int keepalive_s) {
	auto cfg = update_config(true, keepalive_s, (esp_netif_t *)netif);
	esp_mqtt_set_config(_client, &cfg.config);
	if (_is_start)
		esp_mqtt_client_reconnect(_client);
	else {
		esp_mqtt_client_start(_client);
		_is_start = true;
	}
	ESP_LOGW(TAG, "request connect");
}

void disconnect() {
	auto cfg = update_config(false);
	esp_mqtt_set_config(_client, &cfg.config);
	esp_mqtt_client_disconnect(_client);
	ESP_LOGW(TAG, "request disconnect");
}

//cannot use sem to wait for conformation, because this will be called in connected_cb
//which will be called in event handler, resulting in lock.
int subscribe(string_view topic, RecvCallback recv_cb, int qos) {
	int msg_id = esp_mqtt_client_subscribe_single(_client, topic.data(), qos);
	ESP_LOGI(TAG, "subscribing,%s,msg_id,%d,", topic.data(), msg_id);
	if (msg_id < 0)
		return msg_id;
	uint32_t hash = utility::BKDR_hash(topic);
	_received_cbs[hash] = recv_cb;
	return msg_id;
}

int unsubscribe(string_view topic) {
	int msg_id = esp_mqtt_client_unsubscribe(_client, topic.data());
	ESP_LOGI(TAG, "unsubscribing,%s,msg_id,%d,", topic.data(), msg_id);
	if (msg_id < 0)
		return msg_id;
	uint32_t hash = utility::BKDR_hash(topic);
	_received_cbs.erase(hash);
	return msg_id;
}

int publish_block(string_view topic, IBuf data, int qos) {
	//qos==1时是阻塞式发送，收到ACK时才会返回
	//qos==0又未联网时，返回-1
	//在调用线程中阻塞式发送
	return esp_mqtt_client_publish(_client, topic.data(), (const char*)data.data(), data.size(), qos, 0);
}

void publish(string_view topic, IBuf data, int qos, bool skip_if_discon) {
	if (!_connected && skip_if_discon)
		return;
	if(data.size() == 0)
		return;
	//写入缓冲区，在后台任务中发送
	int msg_id = esp_mqtt_client_enqueue(_client, topic.data(), (const char*)data.data(), data.size(), qos, 0, true);
	(void)msg_id;
	// ESP_LOGI(TAG, "Publishing topic: '%s', msg_id = %d", topic, msg_id);
	// ESP_LOG_BUFFER_HEX("pub", data, len);
}

bool connected() {
	return _connected;
}

int get_outbox_size() {
	return esp_mqtt_client_get_outbox_size(_client);
}

void set_connected_cb(ConnCallback cb, void* arg) {
	for (auto& it : _connected_cbs) {
		if (it.first == cb)
			return;
	}
	_connected_cbs.push_back({cb, arg});
}

void reset_connected_cb(ConnCallback cb) {
	_connected_cbs.remove_if([cb](decltype(_connected_cbs)::value_type& it) {
		return it.first == cb;
	});
}

void set_disconnected_cb(ConnCallback cb, void* arg) {
	for (auto& it : _disconnected_cbs) {
		if (it.first == cb)
			return;
	}
	_disconnected_cbs.push_back({cb, arg});
}

void reset_disconnected_cb(ConnCallback cb) {
	_disconnected_cbs.remove_if([cb](decltype(_disconnected_cbs)::value_type& it) {
		return it.first == cb;
	});
}

void set_subscribed_cb(SubscribedCallback cb) {
	for (auto& it : _subscribed_cbs) {
		if (it == cb)
			return;
	}
	_subscribed_cbs.push_back(cb);
}

void reset_subscribed_cb(SubscribedCallback cb) {
	_subscribed_cbs.remove(cb);
}

bool inited() {
	return _client != nullptr;
}

void init(const Cfg& cfg) {
	if (inited())
		return;
	_client_config.config.broker.address.uri = cfg.broker_url.data();
	_client_config.config.broker.address.port = cfg.port;
	_client_config.config.credentials.client_id = cfg.client_id.data();
	_client_config.config.session.last_will.topic = cfg.lwt_topic.data();
	_client_config.config.session.last_will.msg = cfg.lwt_msg.data();
	_client_config.config.session.last_will.msg_len = cfg.lwt_msg.size();
	_client_config.config.session.last_will.qos = 1;
	_client_config.config.session.disable_clean_session = true;
	_client_config.config.network.disable_auto_reconnect = true;
	_client_config.config.session.keepalive = 30;
	_client_config.config.buffer.size = cfg.buffer_size;
	_client_config.config.broker.verification.certificate = cfg.ca.data();
	_client_config.config.broker.verification.certificate_len = cfg.ca.size() + 1;
	_client_config.config.credentials.authentication.certificate = cfg.client_cert.data();
	_client_config.config.credentials.authentication.certificate_len = cfg.client_cert.size() + 1;
	_client_config.config.credentials.authentication.key = cfg.client_key.data();
	_client_config.config.credentials.authentication.key_len = cfg.client_key.size() + 1;

	_client = esp_mqtt_client_init(&_client_config.config);
	esp_mqtt_client_register_event(_client, MQTT_EVENT_ANY, event_handler_adapter, nullptr);
}

void deinit() {
	if (!inited())
		return;
	esp_mqtt_client_stop(_client);
	esp_mqtt_client_destroy(_client);
	_client = nullptr;
	_is_start = false;
}

}
