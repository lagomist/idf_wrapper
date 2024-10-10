#pragma once

#include "bufdef.h"
#include <string_view>

namespace MqttWrapper {

enum Version : uint8_t {
    UNDEFINED = 0,
    PROTOCOL_V_3_1,
    PROTOCOL_V_3_1_1,
    PROTOCOL_V_5,
};

enum Status : uint8_t {
    CONNECTED = 0,
    PUBLISHED,
    SUBSCRIBED,
    UNSUBSCRIBED,
};


struct Cfg {
    std::string_view broker_uri;
    std::string_view username;
    std::string_view password;
    Version protocol;
};

using RecvCallback = void(*)(std::string_view topic, IBuf data, int qos);


void init(const Cfg& cfg);
bool inited();
void deinit();

void connect();
void disconnect();

int subscribe(std::string_view topic, int qos = 1);
int unsubscribe(std::string_view topic);
int publish(std::string_view topic, IBuf data, int qos = 1);
void registerRecvCallback(RecvCallback callback_func);

/**
 * @brief 等待MQTT状态置位
 * 
 * @param status 等待的状态位
 * @param time_ms 等待时间ms( time < 0 则无限等待)
 * @return true 成功
 * @return false 失败
 */
bool waitStatus(Status status, int time_ms);

/**
 * @brief 服务重连主题订阅恢复处理
 * 
 */
void subscribeRecoveryHandle();

}
