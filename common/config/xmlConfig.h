#ifndef XMLCONFIG_H
#define XMLCONFIG_H

#include <tinyxml2.h>

#include <mutex>
#include <string>
#include <memory>
#include <map>
#include <functional>
using namespace tinyxml2;

struct ConfigData {
};

class xmlConfig
{
public:
    ~xmlConfig();

    static xmlConfig &getInstance();

    void loadConfig(const std::string &filename);

    const ConfigData *getConfig();

private:
    xmlConfig();
    xmlConfig(const xmlConfig &) = delete;
    xmlConfig &operator=(const xmlConfig &) = delete;

    std::unique_ptr<ConfigData> data = nullptr;
    std::mutex mutex_;
    std::map<std::string, std::function<void(ConfigData &, const XMLElement *)>> parseCallbacksMap_;
};

#endif // CONFIG_H