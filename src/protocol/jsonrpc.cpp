#include "jsonrpc.h"
#include "dev_desc.h"
#include "os_api.h"
#include <esp_log.h>

namespace jsonrpc {

using Map = std::unordered_map<std::string_view, Method>;

class MethodRegistry : public Map {
public:
    MethodRegistry(std::initializer_list<Pair> list);
    void register_method(std::initializer_list<Pair> list);
    void unregister_method(std::initializer_list<std::string_view> list);
    bool check(std::string_view name);
    void list_print();
};

static const char TAG[] = "jsonrpc";
static constexpr char REQ_CURR_PUB_TOPIC[] = "$aws/rules/jrpc_rule";

static os::task::Task _task_handle;
static PubFunc _pub_func = +[](std::string_view, std::string_view){};
static os::RingBuffer _rb(4048);
static os::QueueT<Topic> _queue(20);
static os::QueueT<uint8_t> _ack(1);
static std::unique_ptr<MethodRegistry> _server_registry = nullptr;
static os::Mutex _mutex;
static os::Mutex _req_mutex; // 为了rpc上报支持多线程，多线程不等于并发
static size_t _msg_id = 0;
static std::string _res_buf;

// 初始化时注册不需要加锁
MethodRegistry::MethodRegistry(std::initializer_list<Pair> list) {
    register_method(list);
}

bool MethodRegistry::check(std::string_view name) {
    os::LockGuard lg(_mutex);
    return find(name) != end();
}

void MethodRegistry::register_method(std::initializer_list<Pair> list) {
    os::LockGuard lg(_mutex);
    for (auto it = list.begin(); it != list.end(); ++it) {
        operator[](it->first) = it->second;
        ESP_LOGI(TAG, "registered method %s", it->first.data());
    }
}

void MethodRegistry::unregister_method(std::initializer_list<std::string_view> list) {
    os::LockGuard lg(_mutex);
    for (auto it = list.begin(); it != list.end(); ++it) {
        this->erase(*it);
        ESP_LOGI(TAG, "registered method %s", it->data());
    }
}

void MethodRegistry::list_print() {
    ESP_LOGI(TAG, "========Registry List Print========");
    ESP_LOGI(TAG, " List Size: %d", size());
    _mutex.lock();
    for (auto it = (*_server_registry).begin(); it != (*_server_registry).end(); ++it) {
        ESP_LOGI(TAG, "-->   %s", it->first.data());
    }
    _mutex.unlock();
    ESP_LOGI(TAG, "===================================");
}

static DynamicJsonDocument make_response() {
    auto response = DynamicJsonDocument(1024);
    response["msg_id"] = 0;
    response["status"] = 0;
    response["ts"] = os::time_ms();
    response["replier_id"] = dev_desc::get_sn().data();
    response["result"]["code"] = 0;
    response["result"]["body"] = "";

    return response;
}

// 接收服务端的请求
static void request_server(std::string_view buf) {
    ESP_LOGW(TAG, "request_server:%.*s", buf.size(), buf.data());
    // function返回值
    auto ret_json = DynamicJsonDocument(1024);
    auto request = DynamicJsonDocument(1024);

    auto response = make_response();
    DeserializationError error = deserializeJson(request, buf);
    if (error) { // json反序列化失败
        ESP_LOGE(TAG, "deserializeJson error: %d", (int)error.code()); 
        return;
    }
    response["msg_id"] = request["msg_id"];
    std::string method = request["method"];
    if (!(*_server_registry).check(method)) {
        response["result"]["code"] = -0xff; // 没找到这个method，code=-255
    }
    else {
        _mutex.lock();
        auto& func = (*_server_registry)[method];
        _mutex.unlock();
        JsonObject param = request["params"]; // 参数
        auto [code, ret] = func(param);
        response["result"]["code"] = code;
        if (ret.isNull())
            response["result"]["body"] = ret.to<JsonObject>();
        else
            response["result"]["body"] = ret;
    }

    std::string res_topic = request["res_topic"];
    if (!res_topic.size()) {
        return;
    }

    std::string response_buf;
    serializeJson(response, response_buf);
    ESP_LOGI(TAG, "response_buf\n%.*s", response_buf.size(), response_buf.data());
    std::string topic = request["res_topic"]; 
    _pub_func(topic, response_buf);
}

static void response_server(std::string_view buf) {
    ESP_LOGW(TAG, "\response_server:\n%.*s\n", buf.size(), buf.data());
    auto response = DynamicJsonDocument(1024);
    DeserializationError error = deserializeJson(response, buf);
    if (error) { // json反序列化失败
        ESP_LOGE(TAG, "deserializeJson error: %d", (int)error.code()); 
        return;
    }
    uint8_t msg_id = response["msg_id"];
    ESP_LOGW(TAG, "MSG_ID: %d", msg_id);
    _res_buf = buf;
    _ack.send(msg_id);
}

static void request_client(std::string_view buf) {
    _pub_func(REQ_CURR_PUB_TOPIC, buf);
}

// 本地发起请求，对外接口
std::string request(ReqCfg &cfg) {
    os::LockGuard lg(_req_mutex);
    _msg_id++;
    auto request = DynamicJsonDocument(1024);
    request["ver"] = JSONRPC_VERSION;
    request["method"] = cfg.func_name.data();
    request["msg_id"] = _msg_id;
    request["res_topic"] = cfg.res_topic.data();
    request["ts"] = os::time_ms();
    request["exp"] = 0;
    request["caller_id"] = dev_desc::get_sn().data();
    request["params"] = cfg.param; 
    static std::string buf = {};
    buf = {};
    serializeJson(request, buf);
    ESP_LOGW(TAG, "request:%.*s\n", buf.size(), buf.data());

    int  ret = _rb.send({(uint8_t*)buf.data(), buf.size()}, 10);
    if (ret == -1) {
        return {};
    }
    _queue.send(Topic::REQUEST_CLIENT);
    
    // 清空队列
	_ack.receive(0);
    if (!cfg.res_topic.size()) { // 不需要回复，则直接返回
        return {};
    }
    auto msg_id = _ack.receive(cfg.timeout_ms);
    if (msg_id == std::nullopt) {
        ESP_LOGW(TAG, "response timeout");
		return {};
    }
    else if(*msg_id != _msg_id){
		ESP_LOGW(TAG, "response sequence error exp [%d] but [%d]", _msg_id, *msg_id);
		return {};
    }
    return _res_buf;
}

void request_cb(std::string_view data) {
    int ret = _rb.send({(uint8_t*)data.data(), data.size()}, 10);
    if (ret == -1) {
        return;
    }
    _queue.send(Topic::REQUEST_SERVER);
}

void response_cb(std::string_view data) {
    int ret = _rb.send({(uint8_t*)data.data(), data.size()}, 10);
    if (ret == -1) {
        return;
    }
    _queue.send(Topic::RESPONSE_SERVER);
}

static void task(void* arg) {
    while (1) {
        Topic topic = Topic::NONE;
        _queue.receive(topic);
        // ESP_LOGI(TAG, "topic:%d", (int)topic);
        auto buf = _rb.recv();
        // ESP_LOGI(TAG, "buf size:%d", buf.size());
        if (!buf.size())
            continue;
        switch (topic) {
            case Topic::REQUEST_SERVER:
                request_server({(char*)buf.data(), buf.size()});
            break;
            case Topic::RESPONSE_SERVER:
                response_server({(char*)buf.data(), buf.size()});
            break;
            case Topic::REQUEST_CLIENT:
                request_client({(char*)buf.data(), buf.size()});
            break;
            default : 
            ESP_LOGI(TAG, "default");
            break;
        }
        // printf("--------------%s-%s-%d---------------\n",__FILE__, __FUNCTION__, __LINE__);
        _rb.return_item(buf); // 把空间还回去
    }
}

int register_method(std::initializer_list<Pair> list) {
    if (!_server_registry) {
        return -1;
    }
    _server_registry->register_method(list);
    return 0;
}

int unregister_method(std::initializer_list<std::string_view> list) {
    if (!_server_registry) {
        return -1;
    }
    _server_registry->unregister_method(list);
    return 0;
}

void init(const os::task::Cfg& cfg, std::initializer_list<Pair> list, PubFunc pub_func) {
    if (pub_func) {
        _pub_func = pub_func;
    }
    _server_registry.reset(new MethodRegistry(list));
    if (cfg.name) {
        auto real_cfg = cfg;
        real_cfg.func = task;
        _task_handle.create(real_cfg);
    }
    ESP_LOGI(TAG, "task init");
}

void deinit() {
    if (!_task_handle.is_inited()) {
		ESP_LOGW(TAG, "task not init");
		return;
	}
    _task_handle.suspend();
	_task_handle.del();
    _server_registry.reset();
    ESP_LOGI(TAG, "task deinit");
}

} // namespace jsonrpc
