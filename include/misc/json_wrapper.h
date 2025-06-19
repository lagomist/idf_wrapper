#pragma once

#include <cJSON.h>
#include <string>
#include <string_view>

namespace Wrapper {

class JsonObject {
public:
    JsonObject();
    JsonObject(std::string_view jsonString);
    ~JsonObject();

    // Parse JSON string into a dictionary
    bool parse(std::string_view jsonString);

    void clear();

    // Serialize dictionary into a JSON string
    std::string serialize() const;

    // Accessors
    bool isValid() const;
    bool isObject() const;
    bool isArray() const;
    bool isString() const;
    bool isNumber() const;
    bool isBool() const;
    bool isNull() const;

    // Setting the object
    void setToArray();

    bool contains(std::string_view key) const;

    // Getters for object
    JsonObject getObject(std::string_view key) const;
    std::string getString(std::string_view key) const;
    float getNumber(std::string_view key) const;
    bool getBool(std::string_view key) const;
    int getArraySize(std::string_view key) const;
    int getArraySize() const;
    std::string getString() const;

    void addToArray(JsonObject& item);
    void addToArray(std::string_view value);
    void addToArray(int value);
    void add(std::string_view key, JsonObject& obj);
    void add(std::string_view key, std::string_view value);
    void add(std::string_view key, int value);
    void add(std::string_view key, float value);

    // Utility functions
    explicit operator std::string() const {
        if (!isString()) {
            return {};
        }
        return std::string(_root->valuestring);
    }

    explicit operator char *() const {
        if (!isString()) {
            return {};
        }
        return _root->valuestring;
    }

    explicit operator uint8_t() const {
        if (!isNumber()) {
            return {};
        }
        return (uint8_t )_root->valuedouble;
    }

    explicit operator int() const {
        if (!isNumber()) {
            return {};
        }
        return (int )_root->valuedouble;
    }

    explicit operator uint32_t() const {
        if (!isNumber()) {
            return {};
        }
        return (uint32_t )_root->valuedouble;
    }

    explicit operator float() const {
        if (!isNumber()) {
            return {};
        }
        return (float )_root->valuedouble;
    }

    explicit operator bool() const {
        if (!isBool()) {
            return {};
        }
        return (bool )_root->valuedouble;
    }

    JsonObject operator[](std::string_view key);
    JsonObject operator[](int index);

    template <typename T>
    JsonObject& operator=(const T &val) {
        cJSON *mval = nullptr;
        if (_key.empty()) {
            return *this;   
        }
        if constexpr (std::is_base_of_v<JsonObject, T>) {
            mval = cJSON_Duplicate(val.get_handle(), true);
        }
        else if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view>) {
            mval = cJSON_CreateString(val.data());
        }
        else if constexpr (std::is_same_v<T, const char *> || std::is_array_v<T>) {
            mval = cJSON_CreateString(val);
        }
        // floating-point will match signed, so match floating-point first.
        else if constexpr (std::is_floating_point_v<T> || std::is_signed_v<T> || std::is_unsigned_v<T>) {
            mval = cJSON_CreateNumber(val);
        }
        else if constexpr (std::is_same_v<T, bool>) {
            mval = cJSON_CreateBool(val);
        }
        else if constexpr (std::is_enum_v<T>) {
            mval = cJSON_CreateNumber((int )val);
        }
        else {
            static_assert(!std::is_same_v<T, T>, "unsupported type");
        }

        cJSON_AddItemToObject(_root, _key.data(), mval);
        return *this;
    }

    JsonObject& operator=(JsonObject&& other) noexcept;

	JsonObject& operator=(const JsonObject& other);

private:
    cJSON* _root;
    std::string _key = {};
    bool _is_child = false;

    JsonObject(cJSON* json, std::string_view key = {});
};


}
