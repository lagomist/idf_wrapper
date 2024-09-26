#ifndef MQTT_WRAPPER_H
#define MQTT_WRAPPER_H

#include "mqtt_client.h"


typedef struct {
    const char *broker_uri;
    const char *username;
    const char *password;
    esp_mqtt_protocol_ver_t protocol_ver;
} mqtt_wrapper_client_cfg_t;

typedef struct {
    char *topic;
    int topic_len;
    char *data;
    int data_len;

} mqtt_wrapper_recv_t;

typedef void (*mqtt_wrapper_recv_cb_t)(mqtt_wrapper_recv_t);


typedef enum {
    MQTT_WRAPPER_CONNECTED,
    MQTT_WRAPPER_PUBLISHED,
    MQTT_WRAPPER_SUBSCRIBED,
    MQTT_WRAPPER_UNSUBSCRIBED,
} mqtt_wrapper_status_t;



/**
 * @brief MQTT初始化，创建客户端
 * 
 * @param config 客户端配置
 * @return esp_mqtt_client_handle_t 客户端句柄
 */
esp_mqtt_client_handle_t mqtt_wrapper_init(mqtt_wrapper_client_cfg_t *config);

/**
 * @brief 等待MQTT状态置位
 * 
 * @param status 等待的状态位
 * @param wait_time 等待时间
 * @return true 成功
 * @return false 失败
 */
bool mqtt_wrapper_wait_status(mqtt_wrapper_status_t status, uint32_t wait_time);

/**
 * @brief 注册MQTT数据接收回调
 * 
 * @param callback_func 回调函数
 */
void mqtt_wrapper_register_receive_callback(mqtt_wrapper_recv_cb_t callback_func);

/**
 * @brief 创建MQTT断开重连服务
 * 
 */
void mqtt_wrapper_crate_recover_service();


void mqtt_wrapper_add_recover_subscribe(const char *topic);

#endif

