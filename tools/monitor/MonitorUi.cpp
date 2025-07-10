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
    component_ =
        CatchEvent(Renderer([&] {
                       if (current_page_ == 0) {
                           // 首页：Topic 列表菜单页
                           std::vector<Element> entries;
                           std::lock_guard<std::mutex> lock(data_mutex_);
                           for (size_t i = 0; i < topic_names_.size(); ++i) {
                               bool selected = (i == current_index_);
                               entries.push_back(text((selected ? "> " : "  ") + topic_names_[i]));
                           }

                           return vbox({text("📄 所有 Topics：") | bold | center, separator(),
                                        vbox(std::move(entries)) | frame, separator(),
                                        text("↑/↓ 选择，Enter 查看详情，q 退出") | dim})
                                  | border | center;
                       } else {
                           // 详情页
                           return renderCurrentTopic();
                       }
                   }),
                   [&](Event event) {
                       if (current_page_ == 0) {
                           // 在首页
                           if (event == Event::ArrowDown) {
                               current_index_ = (current_index_ + 1) % topic_names_.size();
                               return true;
                           } else if (event == Event::ArrowUp) {
                               current_index_ =
                                   (current_index_ + topic_names_.size() - 1) % topic_names_.size();
                               return true;
                           } else if (event == Event::Return && !topic_names_.empty()) {
                               current_topic_ = topic_names_[current_index_];
                               current_page_ = 1;
                               return true;
                           } else if (event == Event::Character('q')) {
                               running_ = false;
                               if (update_thread_.joinable()) {
                                   update_thread_.join(); // 🧷 等它安全退出
                               }
                               MonitorDataBaseManager::getInstance().stop();
                               screen_.Exit();
                               return true;
                           }
                       } else {
                           // 在详情页
                           if (event == Event::ArrowLeft || event == Event::Character('q')) {
                               current_page_ = 0;
                               return true;
                           }
                       }
                       return false;
                   });
}

void MonitorUI::run()
{
    update_thread_ = std::thread([this] {
        using clock = std::chrono::steady_clock;
        auto last_update = clock::now();

        while (running_) {
            updateTopics();
            auto now = clock::now();
            if (now - last_update > std::chrono::milliseconds(500)) {
                screen_.PostEvent(Event::Custom); // 每 500ms 刷新一次 UI
                last_update = now;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200)); // 快速轮询但少刷屏
        }
    });

    screen_.Loop(component_);
}

void MonitorUI::updateTopics()
{
    auto &db = MonitorDataBase::getInstance();
    auto &manger = MonitorDataBaseManager::getInstance();
    std::lock_guard<std::mutex> lock(data_mutex_);
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
    std::lock_guard<std::mutex> lock(data_mutex_);
    if (topic_names_.empty()) {
        return text("没有 Topic 可用") | center;
    }

    if (current_topic_.empty() || topic_info_map_.find(current_topic_) == topic_info_map_.end()) {
        return text("无有效 Topic") | center;
    }

    const std::string &name = current_topic_;
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