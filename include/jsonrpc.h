#pragma once
#include "ArduinoJson.h"
#include "utility.h"
#include "os_api.h"
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>

namespace jsonrpc {

constexpr const int JSONRPC_VERSION = 1;

enum class Topic : uint8_t {
    NONE,
    REQUEST_SERVER,
    RESPONSE_SERVER,

    REQUEST_CLIENT
};

using PubFunc = void(*)(std::string_view topic, std::string_view buf);
using Ret = std::pair<int, DynamicJsonDocument>;
using Method = std::function<Ret(const JsonObject& param)>;
using Pair = std::pair<std::string_view, Method>;

struct ReqCfg {
    std::string func_name;
    // std::string pub_topic;
    std::string res_topic;
    DynamicJsonDocument param;
    uint32_t timeout_ms;
};

void request_cb(std::string_view data);
void response_cb(std::string_view data);
std::string request(ReqCfg &cfg);
int register_method(std::initializer_list<Pair> list);
int unregister_method(std::initializer_list<std::string_view> list);
void init(const os::task::Cfg& cfg, std::initializer_list<Pair> list, PubFunc pub_func);
void deinit();
} // namespace jsonrpc
