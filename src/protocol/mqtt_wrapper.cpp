#include "mqtt_wrapper.h"
#include "wifi_wrapper.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include <atomic>


namespace MqttWrapper {

struct topic_subscribe_inst {
    char topic[128];
    uint8_t topic_len;
    uint8_t qos; 
    struct topic_subscribe_inst *next;
};


static const char *TAG = "mqtt_wrapper";

/* MQTT clinet handle */
static esp_mqtt_client_handle_t _mqtt_client = nullptr;
/* MQTT data receive callback */
static RecvCallback _recv_cb = nullptr;
/* MQTT event group handle */
static EventGroupHandle_t _mqtt_event_group = NULL;

/* recoverable subscribe topic list head */
static topic_subscribe_inst* _topic_sub_list_head = nullptr;


#define MQTT_CONNECTED_BIT      BIT0
#define MQTT_PUBLISHED_BIT      BIT1
#define MQTT_SUBSCRIBED_BIT     BIT2
#define MQTT_UNSUBSCRIBED_BIT   BIT3

static void add_recover_topic_instance(std::string_view topic, int qos) {
    uint8_t topic_len = topic.size();
    if (topic_len > 128 ) return;

    topic_subscribe_inst* new_topic = (topic_subscribe_inst* )malloc(sizeof(topic_subscribe_inst));  
    if (new_topic == nullptr) return;
    if (_topic_sub_list_head == nullptr) {
        _topic_sub_list_head = new_topic;
    } else {
        topic_subscribe_inst *current;
        current = _topic_sub_list_head;
        while (current->next != nullptr) {
            current = current->next;
        }
        current->next = new_topic;
    }

    memcpy(new_topic->topic, topic.data(), topic_len);
    new_topic->topic[topic_len] = '\0';
    new_topic->topic_len = topic_len;
    new_topic->qos = qos;
    new_topic->next = nullptr;
}


static void delete_recover_topic_instance(std::string_view topic) {
    topic_subscribe_inst* prev = nullptr;
    topic_subscribe_inst* current = nullptr;
    for (current = _topic_sub_list_head; current; current = current->next) {
        if (strncmp(current->topic, topic.data(), topic.size()) == 0) {
            if (prev != nullptr) {
                prev->next = current->next;
            }
            free(current);
            break;
        }
        prev = current;
    }
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t )event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected.");
        xEventGroupSetBits(_mqtt_event_group, MQTT_CONNECTED_BIT);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT disconnected.");
        xEventGroupClearBits(_mqtt_event_group, MQTT_CONNECTED_BIT);
        break;
    case MQTT_EVENT_SUBSCRIBED:
        xEventGroupSetBits(_mqtt_event_group, MQTT_SUBSCRIBED_BIT);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        xEventGroupSetBits(_mqtt_event_group, MQTT_UNSUBSCRIBED_BIT);
        break;
    case MQTT_EVENT_PUBLISHED:
        xEventGroupSetBits(_mqtt_event_group, MQTT_PUBLISHED_BIT);
        break;
    case MQTT_EVENT_DATA:
        if (_recv_cb != nullptr) {
            _recv_cb(event->topic, {(uint8_t*)event->data, (size_t)event->data_len}, event->qos);
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        break;
    }
}


bool waitStatus(Status status, int time_ms) {
    EventBits_t bits = 0;
    uint32_t wait_tick = time_ms < 0 ? portMAX_DELAY : pdMS_TO_TICKS(time_ms);
    switch (status) {
    case Status::CONNECTED :
        bits = xEventGroupWaitBits(_mqtt_event_group, MQTT_CONNECTED_BIT, pdFALSE, pdFALSE, wait_tick);
        if (bits & MQTT_CONNECTED_BIT) return true;
        break;
    case Status::PUBLISHED:
        bits = xEventGroupWaitBits(_mqtt_event_group, MQTT_PUBLISHED_BIT, pdTRUE, pdFALSE, wait_tick);
        if (bits & MQTT_PUBLISHED_BIT) return true;
        break;
    case Status::SUBSCRIBED:
        bits = xEventGroupWaitBits(_mqtt_event_group, MQTT_SUBSCRIBED_BIT, pdTRUE, pdFALSE, wait_tick);
        if (bits & MQTT_SUBSCRIBED_BIT) return true;
        break;
    case Status::UNSUBSCRIBED:
        bits = xEventGroupWaitBits(_mqtt_event_group, MQTT_UNSUBSCRIBED_BIT, pdTRUE, pdFALSE, wait_tick);
        if (bits & MQTT_UNSUBSCRIBED_BIT) return true;
        break;
    default:
        break;
    }

    return false;
}


void connect() {
    esp_mqtt_client_start(_mqtt_client);
}

void disconnect() {
    esp_mqtt_client_disconnect(_mqtt_client);
}

void registerRecvCallback(RecvCallback callback_func) {
    _recv_cb = callback_func;
}


int subscribe(std::string_view topic, int qos) {
    add_recover_topic_instance(topic, qos);
	int msg_id = esp_mqtt_client_subscribe(_mqtt_client, topic.data(), qos);
    return msg_id;
}

int unsubscribe(std::string_view topic) {
    delete_recover_topic_instance(topic);
	int msg_id = esp_mqtt_client_unsubscribe(_mqtt_client, topic.data());
    return msg_id;
}

int publish(std::string_view topic, IBuf data, int qos) {
	if (!waitStatus(Status::CONNECTED, 0)) return -1;
	int msg_id = esp_mqtt_client_publish(_mqtt_client, topic.data(), (const char*)data.data(), data.size(), qos, 0);;
    return msg_id;
}


void subscribeRecoveryHandle() {
    if (waitStatus(Status::CONNECTED, 0) == true) return;
    /* startup MQTT reconnect*/
    esp_mqtt_client_reconnect(_mqtt_client);

    if (waitStatus(Status::CONNECTED, 5000)) {
        /* subscribe topic list */
        topic_subscribe_inst *list;
        for (list = _topic_sub_list_head; list; list = list->next) {
            esp_mqtt_client_subscribe(_mqtt_client, list->topic, list->qos);
        }
    } else {
        disconnect();
        ESP_LOGE(TAG, "MQTT reconnect failed.");
    }
}

bool inited() {
	return _mqtt_client != nullptr;
}

void init(const Cfg& cfg) {
    if (inited()) return;
#if (!CONFIG_MQTT_PROTOCOL_5)
    if (cfg.protocol == Version::PROTOCOL_V_5) {
        ESP_LOGE(TAG, "protocol v5 unenable. please config menuconfig");
        return;
    }
#endif
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = cfg.broker_uri.data(),
    mqtt_cfg.session.protocol_ver = (esp_mqtt_protocol_ver_t )cfg.protocol,
    mqtt_cfg.network.disable_auto_reconnect = true,
    mqtt_cfg.credentials.username = cfg.username.data(),
    mqtt_cfg.credentials.authentication.password = cfg.password.data(),
    mqtt_cfg.session.last_will.topic = "/topic/last_will",
    mqtt_cfg.session.last_will.msg = "i will leave",
    mqtt_cfg.session.last_will.msg_len = 12,
    mqtt_cfg.session.last_will.qos = 1,
    mqtt_cfg.session.last_will.retain = true,
    
    _mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
#if CONFIG_MQTT_PROTOCOL_5
    if (cfg.protocol == Version::PROTOCOL_V_5) {
        esp_mqtt5_connection_property_config_t connect_property = {
            .session_expiry_interval = 10,
            .maximum_packet_size = 1024,
            .receive_maximum = 65535,
            .topic_alias_maximum = 2,
            .request_resp_info = true,
            .request_problem_info = true,
            .will_delay_interval = 10,
            .payload_format_indicator = true,
            .message_expiry_interval = 10,
            .response_topic = NULL,
            .correlation_data = NULL,
            .correlation_data_len = 0,
        };
        esp_mqtt5_client_set_connect_property(_mqtt_client, &connect_property);
    }
#endif

    /* The last argument may be used to pass data to the event handler, in this mqtt_event_handler */
    esp_mqtt_client_register_event(_mqtt_client, MQTT_EVENT_ANY, mqtt_event_handler, NULL);
    esp_mqtt_client_start(_mqtt_client);

    _mqtt_event_group = xEventGroupCreate();
}

void deinit() {
	if (!inited()) return;
	esp_mqtt_client_stop(_mqtt_client);
	esp_mqtt_client_destroy(_mqtt_client);
    vEventGroupDelete(_mqtt_event_group);
    _mqtt_event_group = nullptr;
	_mqtt_client = nullptr;
}

}
