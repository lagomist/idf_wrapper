#ifndef TUYA_WRAPPER_H
#define TUYA_WRAPPER_H

#include "tuyalink_core.h"

typedef struct {
    const char *id;
    const char *secret;
} tuya_wrapper_device_t;

typedef void (*tuya_connect_callback_t)(tuya_mqtt_context_t* context, void* user_data);
typedef void (*tuya_messages_callback_t)(tuya_mqtt_context_t* context, void* user_data, const tuyalink_message_t* msg);


/**
 * @brief 创建Tuya MQTT标准协议 生态接入客户端
 * 
 * @param device 设备标识
 * @param messages_callback 消息接收回调
 * @param connect_callback 连接成功回调
 * @return int 0：成功，其他：失败
 */
int tuya_wrapper_mqtt_init(tuya_wrapper_device_t device, 
                            tuya_messages_callback_t messages_callback,
                            tuya_connect_callback_t connect_callback);

#endif
