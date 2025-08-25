#pragma once
#include <unordered_map>
#include <string>

class Config {
public:
    static Config& Instance() {
        static Config instance;
        return instance;
    }
    
    template<typename T>
    T get(const std::string& key, const T& defaultValue = T{}) {
        // 实现配置读取逻辑
        return defaultValue;
    }
    
    template<typename T>
    void set(const std::string& key, const T& value) 
    {
        // 实现配置设置逻辑
    }

private:
    std::unordered_map<std::string, std::string> values_;
};