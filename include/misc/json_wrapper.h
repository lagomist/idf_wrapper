#pragma once

#include <cJSON.h>
#include <string>
#include <string_view>

namespace Wrapper {

class JsonBase {
public:
    JsonBase();
    JsonBase(cJSON *json);
    JsonBase(std::string_view jsonString);
    ~JsonBase();

    // Parse JSON string into a dictionary
    bool parse(std::string_view jsonString);

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
    bool empty() const;

    // Setting the object
    void setToArray();

    bool contains(std::string_view key) const;

    // Getters for object
    JsonBase getObject(std::string_view key) const;
    std::string getString(std::string_view key) const;
    float getFloat(std::string_view key) const;
    uint32_t getNumber(std::string_view key) const;
    bool getBool(std::string_view key) const;
    int getArraySize(std::string_view key) const;
    int getArraySize() const;
    std::string getString() const;
    float getFloat() const;
    int getNumber() const;

    void addToArray(JsonBase& item);
    void addToArray(std::string_view value);
    void addToArray(int value);
    void add(std::string_view key, JsonBase& obj);
    void add(std::string_view key, std::string_view value);
    void add(std::string_view key, int value);
    void add(std::string_view key, float value);

protected:
    cJSON* _root;
    bool _is_child = false;

    void clear();
    cJSON* get_handle() const;
};


class JsonObject : public JsonBase {
public:
    using JsonBase::JsonBase;

    class Proxy : public JsonBase {
	public:
		Proxy(cJSON* base, std::string_view key = {}) : JsonBase(base), _key(key) {}

        explicit operator std::string() const;
        explicit operator char*() const;
        explicit operator uint8_t() const;
        explicit operator uint32_t() const;
        explicit operator int() const;
        explicit operator float() const;
        explicit operator bool() const;

		template <typename T>
        Proxy& operator=(const T &val) {
            if (_key.empty() || _root == nullptr) {
                // No key set, do nothing
                return *this;
            }
            cJSON *mval = pack(val);
            cJSON_AddItemToObject(_root, _key.data(), mval);
            return *this;
        }
		
        Proxy operator[](std::string_view key);
        Proxy operator[](int index);
	private:
		std::string_view _key;

        template <typename T>
        cJSON *pack(const T &val) {
            cJSON *mval = nullptr;
            if constexpr (std::is_base_of_v<JsonBase, T>) {
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
            return mval;
        }
	};

    // Utility functions
    Proxy operator[](std::string_view key);
    Proxy operator[](int index);

    JsonObject& operator=(JsonObject&& other) noexcept;
	JsonObject& operator=(const JsonObject& other);

private:

};

}
