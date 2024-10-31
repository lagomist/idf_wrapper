#pragma once

#include <string>
// #include <unordered_map>
// #include <vector>
#include <cJSON.h>

namespace Wrapper {

class JsonObject {
public:
    JsonObject();
    JsonObject(const std::string& jsonString);
    ~JsonObject();

    // Parse JSON string into a dictionary
    bool parse(const std::string& jsonString);

    // Serialize dictionary into a JSON string
    std::string serialize() const;

    // Accessors
    bool isObject() const;
    bool isArray() const;
    bool isString() const;
    bool isNumber() const;
    bool isBool() const;
    bool isNull() const;

    // Setting the object
    void setArray();

    // Getters for object
    JsonObject getObject(const std::string& key) const;
    std::string getString(const std::string& key) const;
    float getNumber(const std::string& key) const;
    bool getBool(const std::string& key) const;
    int getArraySize(const std::string& key) const;
    int getArraySize() const;
    std::string getString() const;

    void addObject(const std::string& key, JsonObject& obj);
    void addArray(JsonObject& arr);
    void add(const std::string& key, const std::string& value);
    void add(const std::string& key, int value);
    void add(const std::string& key, float value);

    // Utility functions
    JsonObject& operator[](const std::string& key);
    JsonObject& operator[](int index);

private:
    cJSON* _root;
    JsonObject* _value = nullptr;
    bool _is_child = false;

    JsonObject(cJSON* json);
    void clear();
};

}
