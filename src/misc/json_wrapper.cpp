#include "json_wrapper.h"
#include "esp_log.h"

namespace Wrapper {

constexpr static char TAG[] = "Wrapper::JsonObject";

JsonObject::JsonObject() : _root(cJSON_CreateObject()) {}
  
JsonObject::JsonObject(const std::string& jsonString) : _root(nullptr) {
    parse(jsonString);
}

JsonObject::JsonObject(cJSON* json) : _root(json) {
    // do not free json memory
    _is_child = true;
}

JsonObject::~JsonObject() {
    clear();
}
  
bool JsonObject::parse(const std::string& jsonString) {
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
  
std::string JsonObject::serialize() const {
    char* jsonString = cJSON_PrintUnformatted(_root);
    std::string result(jsonString);
    cJSON_free(jsonString);
    return result;
}
  
bool JsonObject::isObject() const {
    return cJSON_IsObject(_root);
}
  
bool JsonObject::isArray() const {
    return cJSON_IsArray(_root);
}
  
bool JsonObject::isString() const {
    return cJSON_IsString(_root);
}
  
bool JsonObject::isNumber() const {
    return cJSON_IsNumber(_root);
}

bool JsonObject::isBool() const {
    return cJSON_IsBool(_root);
}
  
bool JsonObject::isNull() const {
    return cJSON_IsNull(_root);
}

void JsonObject::setArray() {
    clear();
    _root = cJSON_CreateArray();
}

int JsonObject::getArraySize(const std::string& key) const {
    const cJSON* item = cJSON_GetObjectItem(_root, key.c_str());
    return cJSON_GetArraySize(item);
}

int JsonObject::getArraySize() const {
    return cJSON_GetArraySize(_root);
}
  
JsonObject JsonObject::getObject(const std::string& key) const {
    return JsonObject(cJSON_GetObjectItem(_root, key.c_str()));
}
  
std::string JsonObject::getString(const std::string& key) const {
    const cJSON* item = cJSON_GetObjectItem(_root, key.c_str());
    if (!cJSON_IsString(item)) {
        return {};
    }
    return item->valuestring;
}

std::string JsonObject::getString() const {
    if (!cJSON_IsString(_root)) {
        return {};
    }
    return _root->valuestring;
}

float JsonObject::getNumber(const std::string& key) const{
    const cJSON* item = cJSON_GetObjectItem(_root, key.c_str());
    if (!cJSON_IsNumber(item)) {
        return {};
    }
    return (float )item->valuedouble;
}

bool JsonObject::getBool(const std::string& key) const {
    const cJSON* item = cJSON_GetObjectItem(_root, key.c_str());
    return cJSON_IsTrue(item);
}

void JsonObject::addObject(const std::string& key, JsonObject& obj) {
    cJSON_AddItemToObject(_root, key.c_str(), obj._root);
}

void JsonObject::addArray(JsonObject& arr) {
    cJSON_AddItemToArray(_root, arr._root);
}

void JsonObject::add(const std::string& key, const std::string& value) {
    cJSON_AddStringToObject(_root, key.c_str(), value.c_str());
}

void JsonObject::add(const std::string& key, int value) {
    cJSON_AddNumberToObject(_root, key.c_str(), value);
} 

void JsonObject::add(const std::string& key, float value) {
    cJSON_AddNumberToObject(_root, key.c_str(), value);
}

JsonObject& JsonObject::operator[](const std::string& key) {
    if (_value) {
        delete _value;
    }
    _value = new JsonObject(cJSON_GetObjectItem(_root, key.c_str()));
    return *_value;
}

JsonObject& JsonObject::operator[](int index) {
    if (_value) {
        delete _value;
    }
    if (!isArray()) {
        _value = new JsonObject();
    } else {
        _value = new JsonObject(cJSON_GetArrayItem(_root, index));
    }
    return *_value;
}
  
void JsonObject::clear() {
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

}
