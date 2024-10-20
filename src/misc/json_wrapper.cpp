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

int JsonWrapper::getArraySize() {
    return cJSON_GetArraySize(_root);
}
  
JsonWrapper JsonWrapper::getObject(const std::string& key) const {
    return JsonWrapper(cJSON_GetObjectItem(_root, key.c_str()));
}
  
std::string JsonWrapper::getString() const {
    if (!isString()) {
        return {};
    }
    return _root->valuestring;
}
  
float JsonWrapper::getNumber() const {
    if (!isNumber()) {
        return 0;
    }
    return _root->valuedouble;
}
  
bool JsonWrapper::getBool() const {
    if (!isBool()) {
        return false;
    }
    return _root->valueint != 0;
}
  
void JsonWrapper::setObject() {
    clear();
    _root = cJSON_CreateObject();
    _is_child = false;
}

void JsonWrapper::setArray() {
    clear();
    _root = cJSON_CreateArray();
}

void JsonWrapper::setString(const std::string& str) {
    clear();
    _root = cJSON_CreateString(str.c_str());
}

void JsonWrapper::setNumber(float num) {
    clear();
    _root = cJSON_CreateNumber(num);
}
  
void JsonWrapper::setBool(bool boolVal) {
    clear();
    _root = cJSON_CreateBool(boolVal);
}
  
void JsonWrapper::setNull() {
    clear();
    _root = cJSON_CreateNull();
}

void JsonWrapper::addObject(const std::string& key, JsonWrapper& obj) {
    cJSON_AddItemToObject(_root, key.c_str(), obj._root);
}

void JsonWrapper::addArray(JsonWrapper& arr) {
    cJSON_AddItemToArray(_root, arr._root);
}

// JsonWrapper& JsonWrapper::operator[](const std::string& key) {
//     return getObject(key);
// }

const JsonWrapper& JsonWrapper::operator[](const std::string& key) const {
    return getObject(key);
}

const JsonWrapper& JsonWrapper::operator[](int index) const {
    if (!isArray()) return {};
    return JsonWrapper(cJSON_GetArrayItem(_root, index));
}
  
void JsonWrapper::clear() {
    if (_root && !_is_child) {
        cJSON_Delete(_root);
        _root = nullptr;
    }
    _is_child = false;
}
