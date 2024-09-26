#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

#include "mqtt_wrapper.h"
#include "wifi_wrapper.h"



typedef struct topic_subscribe_node {
    char topic[128];
    uint8_t topic_len;
    struct topic_subscribe_node *next;
} topic_subscribe_list_t;


static const char *TAG = "mqtt_wrapper";

/* MQTT clinet handle */
static esp_mqtt_client_handle_t mqtt_client = NULL;
/* MQTT data receive callback */
static mqtt_wrapper_recv_cb_t mqtt_receive_callback = NULL;
/* MQTT event group handle */
static EventGroupHandle_t mqtt_event_group = NULL;

/* automatic subscribe topic list head */
static topic_subscribe_list_t *topic_sub_list_head = NULL;

#define MQTT_CONNECTED_BIT      BIT0
#define MQTT_PUBLISHED_BIT      BIT1
#define MQTT_SUBSCRIBED_BIT     BIT2
#define MQTT_UNSUBSCRIBED_BIT   BIT3




void mqtt_wrapper_register_receive_callback(mqtt_wrapper_recv_cb_t callback_func)
{
    mqtt_receive_callback = callback_func;
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
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected.");
        xEventGroupSetBits(mqtt_event_group, MQTT_CONNECTED_BIT);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT disconnected.");
        xEventGroupClearBits(mqtt_event_group, MQTT_CONNECTED_BIT);
        break;
    case MQTT_EVENT_SUBSCRIBED:
        xEventGroupSetBits(mqtt_event_group, MQTT_SUBSCRIBED_BIT);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        xEventGroupSetBits(mqtt_event_group, MQTT_UNSUBSCRIBED_BIT);
        break;
    case MQTT_EVENT_PUBLISHED:
        xEventGroupSetBits(mqtt_event_group, MQTT_PUBLISHED_BIT);
        break;
    case MQTT_EVENT_DATA:
        if (mqtt_receive_callback != NULL) {
            mqtt_wrapper_recv_t recv = {
                .data = event->data,
                .data_len = event->data_len,
                .topic = event->topic,
                .topic_len = event->topic_len,
            };
            mqtt_receive_callback(recv);
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        break;
    }
}


bool mqtt_wrapper_wait_status(mqtt_wrapper_status_t status, uint32_t wait_time)
{
    EventBits_t bits = 0;
    switch (status) {
    case MQTT_WRAPPER_CONNECTED:
        bits = xEventGroupWaitBits(mqtt_event_group, MQTT_CONNECTED_BIT, pdFALSE, pdFALSE, wait_time);
        if (bits & MQTT_CONNECTED_BIT) return true;
        break;
    case MQTT_WRAPPER_PUBLISHED:
        bits = xEventGroupWaitBits(mqtt_event_group, MQTT_PUBLISHED_BIT, pdTRUE, pdFALSE, wait_time);
        if (bits & MQTT_PUBLISHED_BIT) return true;
        break;
    case MQTT_WRAPPER_SUBSCRIBED:
        bits = xEventGroupWaitBits(mqtt_event_group, MQTT_SUBSCRIBED_BIT, pdTRUE, pdFALSE, wait_time);
        if (bits & MQTT_SUBSCRIBED_BIT) return true;
        break;
    case MQTT_WRAPPER_UNSUBSCRIBED:
        bits = xEventGroupWaitBits(mqtt_event_group, MQTT_UNSUBSCRIBED_BIT, pdTRUE, pdFALSE, wait_time);
        if (bits & MQTT_UNSUBSCRIBED_BIT) return true;
        break;
    default:
        break;
    }

    return false;
}


esp_mqtt_client_handle_t mqtt_wrapper_init(mqtt_wrapper_client_cfg_t *config)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = config->broker_uri,
        .session.protocol_ver = config->protocol_ver,
        .network.disable_auto_reconnect = true,
        .credentials.username = config->username,
        .credentials.authentication.password = config->password,
        .session.last_will.topic = "/topic/will",
        .session.last_will.msg = "i will leave",
        .session.last_will.msg_len = 12,
        .session.last_will.qos = 1,
        .session.last_will.retain = true,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);

    if (config->protocol_ver == MQTT_PROTOCOL_V_5) {
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
        esp_mqtt5_client_set_connect_property(client, &connect_property);
    }

    /* The last argument may be used to pass data to the event handler, in this mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    mqtt_event_group = xEventGroupCreate();
    mqtt_client = client;

    return client;
}



static void mqtt_recover_task(void * pvParameters)
{
    esp_mqtt_client_handle_t client = mqtt_client;

    /* ensure wifi and MQTT connected */
    wifi_wrapper_wait_connected(portMAX_DELAY);
    mqtt_wrapper_wait_status(MQTT_WRAPPER_CONNECTED, portMAX_DELAY);
    while (1) {
        if (mqtt_wrapper_wait_status(MQTT_WRAPPER_CONNECTED, 0) == false) {
            wifi_wrapper_wait_connected(portMAX_DELAY);
            /* if MQTT disconnected startup reconnect*/
            if (esp_mqtt_client_reconnect(client) != ESP_OK) {
                ESP_LOGE(TAG, "MQTT reconnect failed.");
            }
            if (mqtt_wrapper_wait_status(MQTT_WRAPPER_CONNECTED, 5000)) {
                /* subscribe topic list */
                topic_subscribe_list_t *list;
                for (list = topic_sub_list_head; list; list = list->next) {
                    esp_mqtt_client_subscribe(mqtt_client, list->topic, 0);
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void mqtt_wrapper_crate_recover_service()
{
    xTaskCreate(mqtt_recover_task, "mqtt_recover_task", 3 * 1024, NULL, 14, NULL);
}

void mqtt_wrapper_add_recover_subscribe(const char *topic)
{
    uint8_t topic_len = strlen(topic);
    if (topic_len > 128 ) return;

    topic_subscribe_list_t *new_topic = (topic_subscribe_list_t*)malloc(sizeof(topic_subscribe_list_t));  
    if (new_topic == NULL) return;
    if (topic_sub_list_head == NULL) {
        topic_sub_list_head = new_topic;
    } else {
        topic_subscribe_list_t *current;
        current = topic_sub_list_head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_topic;
    }

    memcpy(new_topic->topic, topic, topic_len);
    new_topic->topic[topic_len] = '\0';
    new_topic->topic_len = topic_len;
    new_topic->next = NULL;
}