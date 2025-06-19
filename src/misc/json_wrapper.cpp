#include "json_wrapper.h"
#include "esp_log.h"

namespace Wrapper {

constexpr static char TAG[] = "Wrapper::JsonObject";

JsonObject::JsonObject() : _root(cJSON_CreateObject()) {}
  
JsonObject::JsonObject(std::string_view jsonString) : _root(nullptr) {
    parse(jsonString);
}

JsonObject::JsonObject(cJSON* json, std::string_view key) : _root(json), _key(key) {
    // do not free json memory
    _is_child = true;
}

JsonObject::~JsonObject() {
    clear();
}

void JsonObject::clear() {
    if (_root && !_is_child) {
        cJSON_Delete(_root);
        _root = nullptr;
    }
    _is_child = false;
}
  
bool JsonObject::parse(std::string_view jsonString) {
    clear();
    _root = cJSON_Parse(jsonString.data());
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

bool JsonObject::isValid() const {
    return _root != nullptr;
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

void JsonObject::setToArray() {
    clear();
    _root = cJSON_CreateArray();
}

int JsonObject::getArraySize(std::string_view key) const {
    const cJSON* item = cJSON_GetObjectItem(_root, key.data());
    return cJSON_GetArraySize(item);
}

int JsonObject::getArraySize() const {
    return cJSON_GetArraySize(_root);
}

bool JsonObject::contains(std::string_view key) const {
    return cJSON_HasObjectItem(_root, key.data());
}

JsonObject JsonObject::getObject(std::string_view key) const {
    return JsonObject(cJSON_GetObjectItem(_root, key.data()));
}
  
std::string JsonObject::getString(std::string_view key) const {
    const cJSON* item = cJSON_GetObjectItem(_root, key.data());
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

float JsonObject::getNumber(std::string_view key) const{
    const cJSON* item = cJSON_GetObjectItem(_root, key.data());
    if (!cJSON_IsNumber(item)) {
        return {};
    }
    return (float )item->valuedouble;
}

bool JsonObject::getBool(std::string_view key) const {
    const cJSON* item = cJSON_GetObjectItem(_root, key.data());
    return cJSON_IsTrue(item);
}

void JsonObject::addToArray(JsonObject& item) {
    cJSON_AddItemToArray(_root, item._root);
}

void JsonObject::addToArray(std::string_view value) {
    cJSON* item = cJSON_CreateString(value.data());
    cJSON_AddItemToArray(_root, item);
}

void JsonObject::addToArray(int value) {
    cJSON* item = cJSON_CreateNumber(value);
    cJSON_AddItemToArray(_root, item);
}

void JsonObject::add(std::string_view key, JsonObject& obj) {
    cJSON_AddItemToObject(_root, key.data(), obj._root);
}

void JsonObject::add(std::string_view key, std::string_view value) {
    cJSON_AddStringToObject(_root, key.data(), value.data());
}

void JsonObject::add(std::string_view key, int value) {
    cJSON_AddNumberToObject(_root, key.data(), value);
} 

void JsonObject::add(std::string_view key, float value) {
    cJSON_AddNumberToObject(_root, key.data(), value);
}

JsonObject JsonObject::operator[](std::string_view key) {
    cJSON *mval = cJSON_GetObjectItem(_root, key.data());
    if (mval == nullptr) {
        return JsonObject(_root, key);
    }
    return JsonObject(mval, key);
}

JsonObject JsonObject::operator[](int index) {
    if (!isArray()) {
        return JsonObject();
    }
    return JsonObject(cJSON_GetArrayItem(_root, index));
}

JsonObject& JsonObject::operator=(JsonObject&& other) noexcept {
    if (this != &other) {
        clear();
        _root = other._root;
        other._root = nullptr;
    }
    return *this;
}

JsonObject& JsonObject::operator=(const JsonObject& other) {
    if (this != &other) {
        clear();
        _root = cJSON_Duplicate(other._root, true);
    }
    return *this;
}

}
