#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <chrono>
#include <thread>
#include <vector>
#include <string>

#include "MonitorUi.h"

using namespace ftxui;

namespace Monitor
{

MonitorUI::MonitorUI() : screen_(ScreenInteractive::TerminalOutput())
{
    component_ = CatchEvent(Renderer([&] { return renderCurrentTopic(); }), [&](Event event) {
        if (event == Event::ArrowRight || event == Event::Character('l')) {
            nextTopic();
            return true;
        } else if (event == Event::ArrowLeft || event == Event::Character('h')) {
            prevTopic();
            return true;
        }
        return false;
    });
}

void MonitorUI::run()
{
    std::thread([this] {
        while (running_) {
            updateTopics();
            screen_.PostEvent(Event::Custom); // 触发重绘
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }).detach();

    screen_.Loop(component_);
}

void MonitorUI::updateTopics()
{
    auto &db = MonitorDataBase::getInstance();
    auto &manger = MonitorDataBaseManager::getInstance();
  
    topic_names_ = db.getAllTopicsName();

    for (const auto &name : topic_names_) {
        if (manger.parseTopic(name)) {
            topic_info_map_[name] = manger.getTopicInfo(name);
        }
    }

    if (current_index_ >= topic_names_.size()) {
        current_index_ = 0;
    }
}

void MonitorUI::nextTopic()
{
    if (!topic_names_.empty())
        current_index_ = (current_index_ + 1) % topic_names_.size();
}

void MonitorUI::prevTopic()
{
    if (!topic_names_.empty())
        current_index_ = (current_index_ + topic_names_.size() - 1) % topic_names_.size();
}

Element MonitorUI::renderCurrentTopic()
{
    if (topic_names_.empty()) {
        return text("没有 Topic 可用") | center;
    }

    const std::string &name = topic_names_[current_index_];
    const topicInfo_t &info = topic_info_map_[name];

    std::vector<Element> content;
    content.push_back(text("📌 Topic: " + name));
    content.push_back(text("📦 Type: " + info.data_type));
    content.push_back(text("🌍 Domain ID: " + info.domainId));
    content.push_back(separator());

    auto addList = [&](const std::string &title, const std::vector<std::string> &list) {
        content.push_back(text(title));
        for (const auto &item : list) {
            content.push_back(text("  - " + item));
        }
    };

    auto addObjList = [&](const std::string &title, const std::vector<info_t> &objs) {
        content.push_back(text(title));
        for (const auto &obj : objs) {
            content.push_back(
                text("  - " + obj.name + " (" + (obj.type == R_TYPE ? "Reader" : "Writer") + ")"));
        }
    };

    addList("📖 Readers:", info.datareaders);
    addList("✍️ Writers:", info.datawriters);
    addObjList("👤 Participants:", info.participants);
    addObjList("🧠 Processes:", info.processes);
    addObjList("🙋 Users:", info.users);
    addObjList("🖥 Hosts:", info.hosts);

    content.push_back(separator());
    content.push_back(text("←/→ or h/l 翻页 topic"));

    return vbox(std::move(content)) | border | center;
}

} // namespace Monitor