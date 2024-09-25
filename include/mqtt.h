#pragma once

#include "bufdef.h"
#include <esp_event_base.h>
#include <string_view>

/*need to be pasted into mqtt_client.c
bool _mqtt_running(esp_mqtt_client_handle_t client) {return client->run;}
void _mqtt_set_auto_reconnect(esp_mqtt_client_handle_t client, bool value) {client->config->auto_reconnect = value;}
*/

namespace mqtt {

using std::string_view;

using ConnCallback = void(*)(void* arg);
using RecvCallback = void(*)(string_view topic, IBuf data, int qos);
using SubscribedCallback = void(*)(int msg_id);

struct Cfg {
	std::string_view broker_url;
	uint16_t port;
	std::string_view client_id;
	int buffer_size;
	std::string_view ca;
	std::string_view client_cert;
	std::string_view client_key;
	std::string_view lwt_topic;
	std::string_view lwt_msg;
};

struct Subscribe {
	const char* topic;
	int qos = 1;
};

enum class Route : uint8_t { NONE, WIFI, MODEM, ETH, };

inline const char* route_str(Route route) {
	switch (route) {
		case Route::NONE: return "none";
		case Route::WIFI: return "wifi";
		case Route::MODEM: return "modem";
		case Route::ETH: return "eth";
		default: return "unknown";
	}
}

void stop();

//调用conncect会设置auto_connect为true
Route route();
void connect(void* netif, int keepalive_s = 60);
void disconnect();
bool connected();
int get_outbox_size();

// topic must end with \0
int subscribe(string_view topic, RecvCallback recv_cb, int qos = 1);
int unsubscribe(string_view topic);

//在调用线程中阻塞式发送。qos==1时等到ack才会返回。是对publish的封装
int publish_block(string_view topic, IBuf data, int qos = 1);
//非阻塞发送，在后台线程中进行，数据写入缓冲区就返回。是对enqueue的封装
void publish(string_view topic, IBuf data, int qos = 0, bool skip_if_discon = true);

void set_connected_cb(ConnCallback cb, void* arg = nullptr);
void reset_connected_cb(ConnCallback cb);

void set_disconnected_cb(ConnCallback cb, void* arg = nullptr);
void reset_disconnected_cb(ConnCallback cb);

void set_subscribed_cb(SubscribedCallback cb);
void reset_subscribed_cb(SubscribedCallback cb);

bool inited();

void init(const Cfg& cfg);
void deinit();

}
