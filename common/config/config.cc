#include "config.h"
#include <yaml-cpp/yaml.h>

Config::Config(){}

Config::~Config(){}


Config& Config::getInstance() {
    static Config instance;  
    return instance;
}


void Config::loadConfig(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);  // 避免多线程访问时数据冲突
    YAML::Node config = YAML::LoadFile(filename); 

    if (!data) {
      data = std::make_unique<ConfigData>(); 
    }

    if (config["server1"]["ip"]) 
        data->server_ip = config["server1"]["ip"].as<std::string>();

    if (config["server1"]["timeout"]) 
        data->timeout = config["server1"]["timeout"].as<int>();
}


const ConfigData* Config::getConfig() {
  return data.get();  
}
