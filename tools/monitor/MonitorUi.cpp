#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <chrono>
#include <thread>
#include <vector>
#include <string>

#include "MonitorDataBase.h"
#include "MonitorDataBaseManager.h"


using namespace ftxui;

namespace Monitor
{

void launchTopicMonitorUI()
{
    auto screen = ScreenInteractive::TerminalOutput();

    int scroll_offset = 0;
    const int max_display = 5;

    auto &DataBase = MonitorDataBase::getInstance();
    auto &manager = MonitorDataBaseManager::getInstance();
    auto renderer = Renderer([&] {
        std::vector<std::string> topicNames = DataBase.getAllTopicsName();
        Elements topic_blocks;

        int total = topicNames.size();
        int start = std::min(scroll_offset, std::max(0, total - max_display));
        int end = std::min(start + max_display, total);

        for (int i = start; i < end; ++i) {
            const auto &name = topicNames[i];
            const auto info = manager.getTopicInfo(name); // ← 你自己的数据结构

            Elements block;
            block.push_back(text("Topic: " + name) | bold | color(Color::Yellow));
            block.push_back(text("  Type: " + info.data_type));
            block.push_back(text("  Domain ID: " + info.domainId));

            block.push_back(text("  Data Readers:"));
            for (const auto &r : info.datareaders)
                block.push_back(text("    - " + r));

            block.push_back(text("  Data Writers:"));
            for (const auto &w : info.datawriters)
                block.push_back(text("    - " + w));

            // 可选：participants、processes、users、hosts...
            topic_blocks.push_back(vbox(std::move(block)) | border | flex);
        }

        return vbox({hbox(text(" Topic Monitor ") | bold | color(Color::Green)), separator(),
                     vbox(topic_blocks) | frame | size(HEIGHT, LESS_THAN, 40), separator(),
                     hbox(text(" ↑↓: 翻页 | q: 退出 ") | dim)});
    });

    // 控制上下翻页
    auto component = CatchEvent(renderer, [&](Event event) {
        if (event == Event::ArrowDown) {
            scroll_offset++;
            return true;
        }
        if (event == Event::ArrowUp && scroll_offset > 0) {
            scroll_offset--;
            return true;
        }
        if (event == Event::Character('q')) {
            screen.Exit();
            return true;
        }
        return false;
    });

    // 启动 UI，定时刷新
    std::thread([&] {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            screen.PostEvent(Event::Custom); // 触发重绘
        }
    }).detach();

    screen.Loop(component);
}
} // namespace Monitor