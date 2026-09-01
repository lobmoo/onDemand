#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <atomic>

#include "metrics_engine.h"
#include "pcap_worker.h"
#include "rtps_parser.h"

namespace ondemand_monitor {

class MonitorUi {
public:
    MonitorUi(PcapWorker& worker, MetricsEngine& engine);
    ~MonitorUi() = default;

    // Non-copyable
    MonitorUi(const MonitorUi&) = delete;
    MonitorUi& operator=(const MonitorUi&) = delete;

    // Main entry point
    void Run();

private:
    // Two-level navigation: home page lists nodes (↑↓ to select, Enter to
    // enter); detail page shows that node's business topics (ESC to go back).
    enum class ViewMode {
        LIST_VIEW,    // Level 1: node (participant) selection list
        DETAIL_VIEW   // Level 2: selected node's topic tables
    };

    // UI components
    ftxui::Component BuildListView();
    ftxui::Component BuildDetailView();
    ftxui::Component BuildStatusBar();

    // Data refresh
    void RefreshData();
    // Re-fetch the selected node's topic cache. Called by RefreshData each
    // tick AND synchronously after selection/tab changes so a keypress shows
    // fresh data on the very next frame instead of waiting a full tick.
    void UpdateTopicCache();

    // Format helpers
    ftxui::Element FormatHeader(const std::string& title);
    ftxui::Element FormatRow(const std::vector<std::string>& cells, bool selected = false);
    ftxui::Element FormatTable(const std::vector<std::string>& headers,
                                const std::vector<std::vector<std::string>>& rows,
                                int selected_row = -1);
    std::string FormatGuid(const GUID_t& guid);
    std::string FormatSize(uint64_t bytes);
    std::string FormatNumber(uint64_t num);

    // Components
    PcapWorker& worker_;
    MetricsEngine& engine_;
    ftxui::ScreenInteractive screen_;

    // UI state - two-level navigation
    ViewMode view_mode_ = ViewMode::LIST_VIEW;
    int selected_index_ = 0;   // Selected node in the home-page list
    int topic_tab_index_ = 0;  // Topic category tab inside detail view

    // Data snapshots
    MetricsEngine::Summary summary_;
    std::vector<ParticipantInfo> participants_;
    std::vector<TransferStats> transfers_;
    std::vector<MetricsEngine::TopicMatchInfo> topic_matches_;
    std::unordered_map<uint8_t, uint64_t> submsg_counts_;
    // Topics of the currently selected node, fetched ONCE per RefreshData tick.
    // Both the home-page preview and the detail view render from this cache —
    // querying the engine inside render lambdas caused visible ←→ lag.
    std::vector<MetricsEngine::TopicInfo> cached_topics_;

    // Node list entries for the home page
    std::vector<std::string> menu_entries_;

    // Running state
    std::atomic<bool> running_{false};
};

}  // namespace ondemand_monitor
