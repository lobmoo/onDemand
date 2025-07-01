#ifndef MONITORUI_H
#define MONITORUI_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include "MonitorApi.h"
#include "MonitorDataBase.h"
#include "MonitorDataBaseManager.h"

namespace Monitor
{

class MonitorUI
{
public:
    MonitorUI();
    void run();

private:
    ftxui::ScreenInteractive screen_;
    ftxui::Component component_;
    int current_page_ = 0; // 0 表示菜单页，1 表示详情页
    std::string current_topic_;
    
    std::vector<std::string> topic_names_;
    std::map<std::string, topicInfo_t> topic_info_map_;
    int current_index_ = 0;
    bool running_ = true;

    void updateTopics();
    void nextTopic();
    void prevTopic();
    ftxui::Element renderCurrentTopic();
};

} // namespace Monitor
#endif // MONITORUI_H