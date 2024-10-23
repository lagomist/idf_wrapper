#pragma once

#include <string>
// #include <unordered_map>
// #include <vector>
#include <cJSON.h>

class JsonWrapper {
public:
    JsonWrapper();
    JsonWrapper(const std::string& jsonString);
    ~JsonWrapper();

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
    JsonWrapper getObject(const std::string& key) const;
    std::string getString(const std::string& key) const;
    float getNumber(const std::string& key) const;
    bool getBool(const std::string& key) const;
    int getArraySize(const std::string& key) const;
    int getArraySize() const;
    std::string getString() const;

    void addObject(const std::string& key, JsonWrapper& obj);
    void addArray(JsonWrapper& arr);
    void add(const std::string& key, const std::string& value);
    void add(const std::string& key, int value);
    void add(const std::string& key, float value);

    // Utility functions
    JsonWrapper& operator[](const std::string& key);
    JsonWrapper& operator[](int index);

private:
    cJSON* _root;
    JsonWrapper* _value = nullptr;
    bool _is_child = false;

    JsonWrapper(cJSON* json);
    void clear();
};
