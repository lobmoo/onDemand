#include "xmlConfig.h"
#include "log/logger.h"

xmlConfig::xmlConfig()
{
}

xmlConfig::~xmlConfig()
{
}

parseCallbacksMap_["server"] = [](ConfigData &data, const XMLElement *element) {

};

xmlConfig &xmlConfig::getInstance()
{
    static xmlConfig instance;
    return instance;
}

void xmlConfig::loadConfig(const std::string &filename)
{
    std::lock_guard<std::mutex> lock(mutex_);
    XMLDocument doc;
    XMLError result = doc.LoadFile(filename.c_str());
    if (result != XML_SUCCESS) {
        LOG(error) << "Failed to load file: " << filename;
        return;
    }
    if (!data) {
        data = std::make_unique<ConfigData>();
    }
    const XMLElement *root = doc.RootElement();
    for (const auto &it : parseCallbacksMap_) {
        LOG(info) << "Parsing " << it.first << "config";
        it.second(*data, root);
    }
}

const ConfigData *xmlConfig::getConfig()
{
    return data.get();
}
