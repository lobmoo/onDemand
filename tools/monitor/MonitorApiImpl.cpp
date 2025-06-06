
/**
 * @file MonitorApiImpl.cpp
 * @brief 
 * @author wwk (1162431386@qq.com)
 * @version 1.0
 * @date 2025-06-05
 * 
 * @copyright Copyright (c) 2025  by  宝信
 * 
 * @par 修改日志:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2025-06-05     <td>1.0     <td>wwk   <td>修改?
 * </table>
 */
#include "MonitorApi.h"
#include "MonitorDataBaseManager.h"
#include "log/logger.h"

namespace Monitor {

class MonitorAPI::Impl {
public:
    bool init(uint32_t domain, uint16_t n_bins, uint32_t t_interval) {
        return MonitorDataBaseManager::getInstance().init(domain, n_bins, t_interval);
    }

    bool stop() {
        return MonitorDataBaseManager::getInstance().stop();
    }

    void run() {
        return MonitorDataBaseManager::getInstance().run();
    }

    topicInfo_t getTopicInfo(const std::string& topicName) const {
        topicInfo_t topicInfo;
        auto internal = MonitorDataBaseManager::getInstance().getTopicInfo(topicName);
        topicInfo.data_type = internal.data_type;
        topicInfo.domainId = internal.domainId;
        topicInfo.datareaders = internal.datareaders;
        topicInfo.datawriters = internal.datawriters;
        return topicInfo;
    }
};

MonitorAPI::MonitorAPI() : impl_(std::make_unique<Impl>()) {}
MonitorAPI::~MonitorAPI() = default;

bool MonitorAPI::init(uint32_t domain, uint16_t n_bins, uint32_t t_interval) {
    return impl_->init(domain, n_bins, t_interval);
}
bool MonitorAPI::stop() {
    return impl_->stop();
}

void MonitorAPI::run() {
    return impl_->run();
}

topicInfo_t MonitorAPI::getTopicInfo(const std::string& topicName) const {
    return impl_->getTopicInfo(topicName);
}

} // namespace Monitor