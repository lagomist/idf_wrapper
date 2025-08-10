#include "json_wrapper.h"
#include "esp_log.h"

namespace Wrapper {

JsonBase::JsonBase() : _root(cJSON_CreateObject()) {}
JsonBase::JsonBase(cJSON *json) : _root(json), _is_child(true) {}
JsonBase::JsonBase(std::string_view jsonString) : _root(nullptr) {
    parse(jsonString);
}

JsonBase::~JsonBase() {
    clear();
}

void JsonBase::clear() {
    if (_root && !_is_child) {
        cJSON_Delete(_root);
        _root = nullptr;
    }
}

cJSON* JsonBase::get_handle() const {
    return _root;
}

bool JsonBase::parse(std::string_view jsonString) {
    clear();
    _root = cJSON_Parse(jsonString.data());
    if (!_root) {
        return false;
    }
    return true;
}
  
std::string JsonBase::serialize() const {
    char* jsonString = cJSON_PrintUnformatted(_root);
    std::string result(jsonString);
    cJSON_free(jsonString);
    return result;
}

bool JsonBase::isValid() const {
    return _root != nullptr;
}
  
bool JsonBase::isObject() const {
    return cJSON_IsObject(_root);
}

bool JsonBase::isArray() const {
    return cJSON_IsArray(_root);
}

bool JsonBase::isString() const {
    return cJSON_IsString(_root);
}

bool JsonBase::isNumber() const {
    return cJSON_IsNumber(_root);
}

bool JsonBase::isBool() const {
    return cJSON_IsBool(_root);
}
  
bool JsonBase::isNull() const {
    return cJSON_IsNull(_root);
}

bool JsonBase::empty() const {
    if (_root) {
        return _root->child == nullptr;
    }
    return true;
}


void JsonBase::setToArray() {
    clear();
    _root = cJSON_CreateArray();
}

int JsonBase::getArraySize(std::string_view key) const {
    const cJSON* item = cJSON_GetObjectItem(_root, key.data());
    return cJSON_GetArraySize(item);
}

int JsonBase::getArraySize() const {
    return cJSON_GetArraySize(_root);
}

bool JsonBase::contains(std::string_view key) const {
    return cJSON_HasObjectItem(_root, key.data());
}
  
JsonBase JsonBase::getObject(std::string_view key) const {
    return JsonBase(cJSON_GetObjectItem(_root, key.data()));
}
  
std::string JsonBase::getString(std::string_view key) const {
    const cJSON* item = cJSON_GetObjectItem(_root, key.data());
    if (!cJSON_IsString(item)) {
        return {};
    }
    return item->valuestring;
}

std::string JsonBase::getString() const {
    if (!isString()) {
        return {};
    }
    return _root->valuestring;
}

float JsonBase::getFloat() const {
    if (!isNumber()) {
        return {};
    }
    return (float )_root->valuedouble;
}

int JsonBase::getNumber() const {
    if (!isNumber()) {
        return {};
    }
    return (int )_root->valuedouble;
}

float JsonBase::getFloat(std::string_view key) const {
    const cJSON* item = cJSON_GetObjectItem(_root, key.data());
    if (!cJSON_IsNumber(item)) {
        return {};
    }
    return (float )item->valuedouble;
}

uint32_t JsonBase::getNumber(std::string_view key) const {
    const cJSON* item = cJSON_GetObjectItem(_root, key.data());
    if (!cJSON_IsNumber(item)) {
        return {};
    }
    return (uint32_t )item->valuedouble;
}

bool JsonBase::getBool(std::string_view key) const {
    const cJSON* item = cJSON_GetObjectItem(_root, key.data());
    return cJSON_IsTrue(item);
}

void JsonBase::addToArray(JsonBase& item) {
    cJSON_AddItemToArray(_root, item._root);
}

void JsonBase::addToArray(std::string_view value) {
    cJSON* item = cJSON_CreateString(value.data());
    cJSON_AddItemToArray(_root, item);
}

void JsonBase::addToArray(int value) {
    cJSON* item = cJSON_CreateNumber(value);
    cJSON_AddItemToArray(_root, item);
}

void JsonBase::add(std::string_view key, JsonBase& obj) {
    cJSON_AddItemToObject(_root, key.data(), obj._root);
}

void JsonBase::add(std::string_view key, std::string_view value) {
    cJSON_AddStringToObject(_root, key.data(), value.data());
}

void JsonBase::add(std::string_view key, int value) {
    cJSON_AddNumberToObject(_root, key.data(), value);
} 

void JsonBase::add(std::string_view key, float value) {
    cJSON_AddNumberToObject(_root, key.data(), value);
}




JsonObject::Proxy::operator std::string() const {
    if (!cJSON_IsString(_root)) {
        return {};
    }
    return std::string(_root->valuestring);
}

JsonObject::Proxy::operator char *() const {
    if (!cJSON_IsString(_root)) {
        return {};
    }
    return _root->valuestring;
}

JsonObject::Proxy::operator uint8_t() const {
    if (!cJSON_IsNumber(_root)) {
        return {};
    }
    return (uint8_t )_root->valuedouble;
}

JsonObject::Proxy::operator uint32_t() const {
    if (!cJSON_IsNumber(_root)) {
        return {};
    }
    return (uint32_t )_root->valuedouble;
}

JsonObject::Proxy::operator int() const {
    if (!cJSON_IsNumber(_root)) {
        return {};
    }
    return (int )_root->valuedouble;
}

JsonObject::Proxy::operator float() const {
    if (!cJSON_IsNumber(_root)) {
        return {};
    }
    return (float )_root->valuedouble;
}

JsonObject::Proxy::operator bool() const {
    if (!cJSON_IsBool(_root)) {
        return {};
    }
    return (bool )_root->valuedouble;
}

JsonObject::Proxy JsonObject::Proxy::operator[](std::string_view key) {
    cJSON *mval = cJSON_GetObjectItem(_root, key.data());
    if (mval == nullptr) {
        return Proxy(_root, key);
    }
    return Proxy(mval, key);
}

JsonObject::Proxy JsonObject::Proxy::operator[](int index) {
    if (!isArray()) {
        return Proxy(nullptr);
    }
    return Proxy(cJSON_GetArrayItem(_root, index));
}


JsonObject::Proxy JsonObject::operator[](std::string_view key) {
    cJSON *mval = cJSON_GetObjectItem(_root, key.data());
    if (mval == nullptr) {
        return Proxy(_root, key);
    }
    return Proxy(mval, key);
}

JsonObject::Proxy JsonObject::operator[](int index) {
    if (!isArray()) {
        return Proxy(nullptr);
    }
    return Proxy(cJSON_GetArrayItem(_root, index));
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
