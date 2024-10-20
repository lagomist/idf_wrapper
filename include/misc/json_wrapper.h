#pragma once

#include <string>  
#include <unordered_map>  
#include <vector>
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
  
    // Getters for object  
    JsonWrapper getObject(const std::string& key) const;
    std::string getString() const;
    float getNumber() const;
    bool getBool() const;
    int getArraySize();
  
    // Setters for object
    void setObject();
    void setArray();
    void setString(const std::string& str);
    void setNumber(float num);
    void setBool(bool boolVal);
    void setNull();

    void addObject(const std::string& key, JsonWrapper& obj);
    void addArray(JsonWrapper& arr);
  
    // Utility functions  
    // JsonWrapper& operator[](const std::string& key);
    const JsonWrapper& operator[](const std::string& key) const;
    const JsonWrapper& operator[](int index) const;
  
private:  
    cJSON* _root;
    bool _is_child = false;

    JsonWrapper(cJSON* json);
    void clear();
};
