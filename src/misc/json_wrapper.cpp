#include "json_wrapper.h"
#include "esp_log.h"

constexpr static char TAG[] = "json_wrapper";

JsonWrapper::JsonWrapper() : _root(cJSON_CreateObject()) {}
  
JsonWrapper::JsonWrapper(const std::string& jsonString) : _root(nullptr) {
    parse(jsonString);
}

JsonWrapper::JsonWrapper(cJSON* json) : _root(json) {
    // do not free json memory
    _is_child = true;
}

JsonWrapper::~JsonWrapper() {
    clear();
}
  
bool JsonWrapper::parse(const std::string& jsonString) {
    clear();
    _root = cJSON_Parse(jsonString.c_str());
    if (!_root) {
        const char* error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != nullptr) {
            ESP_LOGE(TAG, "Error before: %s" , error_ptr);
        }
        return false;
    }
    return true;
}
  
std::string JsonWrapper::serialize() const {
    char* jsonString = cJSON_PrintUnformatted(_root);
    std::string result(jsonString);
    cJSON_free(jsonString);
    return result;
}
  
bool JsonWrapper::isObject() const {
    return cJSON_IsObject(_root);
}
  
bool JsonWrapper::isArray() const {
    return cJSON_IsArray(_root);
}
  
bool JsonWrapper::isString() const {
    return cJSON_IsString(_root);
}
  
bool JsonWrapper::isNumber() const {
    return cJSON_IsNumber(_root);
}

bool JsonWrapper::isBool() const {
    return cJSON_IsBool(_root);
}
  
bool JsonWrapper::isNull() const {
    return cJSON_IsNull(_root);
}

void JsonWrapper::setArray() {
    clear();
    _root = cJSON_CreateArray();
}

int JsonWrapper::getArraySize(const std::string& key) const {
    const cJSON* item = cJSON_GetObjectItem(_root, key.c_str());
    return cJSON_GetArraySize(item);
}

int JsonWrapper::getArraySize() const {
    return cJSON_GetArraySize(_root);
}
  
JsonWrapper JsonWrapper::getObject(const std::string& key) const {
    return JsonWrapper(cJSON_GetObjectItem(_root, key.c_str()));
}
  
std::string JsonWrapper::getString(const std::string& key) const {
    const cJSON* item = cJSON_GetObjectItem(_root, key.c_str());
    if (!cJSON_IsString(item)) {
        return {};
    }
    return item->valuestring;
}

std::string JsonWrapper::getString() const {
    if (!cJSON_IsString(_root)) {
        return {};
    }
    return _root->valuestring;
}

float JsonWrapper::getNumber(const std::string& key) const{
    const cJSON* item = cJSON_GetObjectItem(_root, key.c_str());
    if (!cJSON_IsNumber(item)) {
        return {};
    }
    return (float )item->valuedouble;
}

bool JsonWrapper::getBool(const std::string& key) const {
    const cJSON* item = cJSON_GetObjectItem(_root, key.c_str());
    return cJSON_IsTrue(item);
}

void JsonWrapper::addObject(const std::string& key, JsonWrapper& obj) {
    cJSON_AddItemToObject(_root, key.c_str(), obj._root);
}

void JsonWrapper::addArray(JsonWrapper& arr) {
    cJSON_AddItemToArray(_root, arr._root);
}

void JsonWrapper::add(const std::string& key, const std::string& value) {
    cJSON_AddStringToObject(_root, key.c_str(), value.c_str());
}

void JsonWrapper::add(const std::string& key, int value) {
    cJSON_AddNumberToObject(_root, key.c_str(), value);
} 

void JsonWrapper::add(const std::string& key, float value) {
    cJSON_AddNumberToObject(_root, key.c_str(), value);
}

JsonWrapper& JsonWrapper::operator[](const std::string& key) {
    if (_value) {
        delete _value;
    }
    _value = new JsonWrapper(cJSON_GetObjectItem(_root, key.c_str()));
    return *_value;
}

JsonWrapper& JsonWrapper::operator[](int index) {
    if (_value) {
        delete _value;
    }
    if (!isArray()) {
        _value = new JsonWrapper();
    } else {
        _value = new JsonWrapper(cJSON_GetArrayItem(_root, index));
    }
    return *_value;
}
  
void JsonWrapper::clear() {
    if (_root && !_is_child) {
        cJSON_Delete(_root);
        _root = nullptr;
    }
    if (_value) {
        delete _value;
        _value = nullptr;
    }
    _is_child = false;
}
