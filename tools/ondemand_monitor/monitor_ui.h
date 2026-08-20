#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <functional>
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
    // UI components
    ftxui::Component BuildOverviewPage();
    ftxui::Component BuildParticipantsPage();
    ftxui::Component BuildEndpointsPage();
    ftxui::Component BuildTransferPage();
    ftxui::Component BuildStatusBar();

    // Data refresh
    void RefreshData();

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

    // UI state
    int selected_tab_ = 0;
    int selected_participant_ = 0;
    int selected_endpoint_ = 0;
    int selected_transfer_ = 0;

    // Data snapshots
    MetricsEngine::Summary summary_;
    std::vector<ParticipantInfo> participants_;
    std::vector<EndpointInfo> endpoints_;
    std::vector<TransferStats> transfers_;
    std::vector<MetricsEngine::TopicMatchInfo> topic_matches_;

    // Running state
    std::atomic<bool> running_{false};
};

}  // namespace ondemand_monitor
