#pragma once

#include <string>
#include <unordered_map>

class ConfigMgr{
public:
    // 从文件加载配置，返回是否加载成功
    bool loadFromFile(const std::string& filepath); 

    // 获取值，找不到则返回 defaultValue
    template <typename T>
    T get(const std::string& key, const T& defaultValue) const{
        auto it = _kv.find(key);
        if(it == _kv.end())
            return defaultValue;

        return fromString<T>(it->second);
    }

    // 检查是否存在某个 key
    bool hasKey(const std::string& key) const {
        return _kv.count(key) > 0;
    }

    static ConfigMgr& Inst(){
        static ConfigMgr mgr;
        return mgr;
    }

private:
    ConfigMgr() = default;
    ConfigMgr(const ConfigMgr &) = delete;
    ConfigMgr &operator=(const ConfigMgr &) = delete;

    std::string _section;// 当前段名
    std::unordered_map<std::string, std::string> _kv;// 存储扁平化后的 key->value


    // 声明一个可在 .cpp 中特化的模板
    template <typename T>
    static T fromString(const std::string &s);
};