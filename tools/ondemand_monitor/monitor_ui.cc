#include "monitor_ui.h"
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <map>
#include <cstdio>
#include <ftxui/component/loop.hpp>

namespace ondemand_monitor {

MonitorUi::MonitorUi(PcapWorker& worker, MetricsEngine& engine)
    : worker_(worker), engine_(engine),
      screen_(ftxui::ScreenInteractive::Fullscreen()) {
}

ftxui::Element MonitorUi::FormatHeader(const std::string& title) {
    return ftxui::text(title) | ftxui::bold | ftxui::center | ftxui::border;
}

ftxui::Element MonitorUi::FormatRow(const std::vector<std::string>& cells, bool selected) {
    ftxui::Elements elements;
    for (size_t i = 0; i < cells.size(); ++i) {
        elements.push_back(ftxui::text(cells[i]) | ftxui::flex);
        if (i < cells.size() - 1) {
            elements.push_back(ftxui::text(" | "));
        }
    }

    ftxui::Element row = ftxui::hbox(std::move(elements));
    if (selected) {
        row = row | ftxui::inverted | ftxui::focus;
    }
    return row;
}

ftxui::Element MonitorUi::FormatTable(const std::vector<std::string>& headers,
                                        const std::vector<std::vector<std::string>>& rows,
                                        int selected_row) {
    ftxui::Elements elements;

    // Header
    ftxui::Elements header_elements;
    for (size_t i = 0; i < headers.size(); ++i) {
        header_elements.push_back(ftxui::text(headers[i]) | ftxui::bold);
        if (i < headers.size() - 1) {
            header_elements.push_back(ftxui::text(" | "));
        }
    }
    elements.push_back(ftxui::hbox(std::move(header_elements)));
    elements.push_back(ftxui::separator());

    // Rows
    for (size_t i = 0; i < rows.size(); ++i) {
        elements.push_back(FormatRow(rows[i], selected_row == static_cast<int>(i)));
    }

    return ftxui::vbox(std::move(elements));
}

std::string MonitorUi::FormatGuid(const GUID_t& guid) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < 12; ++i) {
        oss << std::setw(2) << static_cast<int>(guid.prefix[i]);
        if (i == 3 || i == 7) oss << "-";
    }
    oss << ":" << std::setw(2) << static_cast<int>(guid.entityId[0])
        << std::setw(2) << static_cast<int>(guid.entityId[1])
        << std::setw(2) << static_cast<int>(guid.entityId[2])
        << std::setw(2) << static_cast<int>(guid.entityId[3]);
    return oss.str();
}

std::string MonitorUi::FormatSize(uint64_t bytes) {
    std::ostringstream oss;
    if (bytes < 1024) {
        oss << bytes << " B";
    } else if (bytes < 1024 * 1024) {
        oss << std::fixed << std::setprecision(1) << (bytes / 1024.0) << " KB";
    } else if (bytes < 1024ULL * 1024 * 1024) {
        oss << std::fixed << std::setprecision(1) << (bytes / (1024.0 * 1024)) << " MB";
    } else {
        oss << std::fixed << std::setprecision(1) << (bytes / (1024.0 * 1024 * 1024)) << " GB";
    }
    return oss.str();
}

std::string MonitorUi::FormatNumber(uint64_t num) {
    std::ostringstream oss;
    if (num < 1000) {
        oss << num;
    } else if (num < 1000000) {
        oss << std::fixed << std::setprecision(1) << (num / 1000.0) << "K";
    } else {
        oss << std::fixed << std::setprecision(1) << (num / 1000000.0) << "M";
    }
    return oss.str();
}

void MonitorUi::RefreshData() {
    summary_ = engine_.GetSummary();
    participants_ = engine_.GetParticipants();
    transfers_ = engine_.GetTransferStats();
    topic_matches_ = engine_.GetAllTopicMatches();
}

ftxui::Component MonitorUi::BuildListView() {
    // Update menu entries from participants
    auto update_entries = [this] {
        menu_entries_.clear();
        for (const auto& p : participants_) {
            if (p.domain_id == 0 && p.name.empty()) continue;
            if (p.domain_id == 0 && p.endpoints_count == 0 && !p.is_active) continue;

            std::string name = p.name.empty() ? "Participant" : p.name;
            char entry[128];
            snprintf(entry, sizeof(entry), "[Domain:%u] %-20s Endpoints:%-4u %s",
                     p.domain_id, name.c_str(), p.endpoints_count,
                     p.is_active ? "Active" : "Inactive");
            menu_entries_.push_back(entry);
        }
    };

    // Initial update
    update_entries();

    // Menu option with on_enter callback
    auto menu_option = ftxui::MenuOption::Vertical();
    menu_option.on_enter = [this] {
        view_mode_ = ViewMode::DETAIL_VIEW;
    };

    auto menu = ftxui::Menu(&menu_entries_, &selected_index_, menu_option);

    // Wrap menu with stats display
    return ftxui::Renderer(menu, [this, &menu, update_entries] {
        // Update entries each frame
        update_entries();

        // Stats summary
        auto s = summary_;
        auto stats = ftxui::vbox({
            ftxui::hbox({
                ftxui::text("Domain: ") | ftxui::bold,
                ftxui::text(std::to_string(s.total_participants > 0 ? 1 : 0)) | ftxui::color(ftxui::Color::Green),
                ftxui::text("  Participant: ") | ftxui::bold,
                ftxui::text(std::to_string(s.total_participants)) | ftxui::color(ftxui::Color::Green),
                ftxui::text("  Endpoint: ") | ftxui::bold,
                ftxui::text(std::to_string(s.total_endpoints)) | ftxui::color(ftxui::Color::Green),
                ftxui::text("  DataMsg: ") | ftxui::bold,
                ftxui::text(FormatNumber(s.total_data_messages)) | ftxui::color(ftxui::Color::Cyan),
                ftxui::text("  Byte: ") | ftxui::bold,
                ftxui::text(FormatSize(s.total_bytes)) | ftxui::color(ftxui::Color::Cyan),
            }),
            ftxui::hbox({
                ftxui::text("Heartbeat: ") | ftxui::bold,
                ftxui::text(FormatNumber(s.total_heartbeats)) | ftxui::color(ftxui::Color::Yellow),
                ftxui::text("  ACKNACK: ") | ftxui::bold,
                ftxui::text(FormatNumber(s.total_acknacks)) | ftxui::color(ftxui::Color::Yellow),
                ftxui::text("  NACK: ") | ftxui::bold,
                ftxui::text(FormatNumber(s.total_nacks)) | ftxui::color(ftxui::Color::Red),
            }),
        });

        return ftxui::vbox({
            ftxui::text("OnDemand Monitor - Participant List") | ftxui::bold | ftxui::center,
            ftxui::text("[↑↓ Navigate] [Enter: View Details] [q: Quit]") | ftxui::dim | ftxui::center,
            ftxui::separator(),
            stats,
            ftxui::separator(),
            menu->Render() | ftxui::flex | ftxui::border,
        });
    });
}

ftxui::Component MonitorUi::BuildDetailView() {
    auto component = ftxui::Renderer([this] {
        // Get filtered participants (same filter as list view)
        std::vector<const ParticipantInfo*> filtered;
        for (const auto& p : participants_) {
            if (p.domain_id == 0 && p.name.empty()) continue;
            if (p.domain_id == 0 && p.endpoints_count == 0 && !p.is_active) continue;
            filtered.push_back(&p);
        }

        // Check if selected index is valid
        if (filtered.empty() || selected_index_ >= static_cast<int>(filtered.size())) {
            return ftxui::vbox({
                ftxui::text("No participant selected") | ftxui::dim | ftxui::center,
                ftxui::text("Press ESC to return to list") | ftxui::dim | ftxui::center,
            });
        }

        const auto& p = *filtered[selected_index_];
        std::string name = p.name.empty() ? "Participant-" + std::to_string(selected_index_ + 1) : p.name;

        // Participant info header
        auto info = ftxui::vbox({
            ftxui::hbox({
                ftxui::text("◀ ") | ftxui::bold,
                ftxui::text(name) | ftxui::bold | ftxui::color(ftxui::Color::Cyan),
                ftxui::text("  (Domain: " + std::to_string(p.domain_id) + ")") | ftxui::dim,
            }),
            ftxui::hbox({
                ftxui::text("  GUID: ") | ftxui::dim,
                ftxui::text(FormatGuid(p.guid)),
            }),
            ftxui::hbox({
                ftxui::text("  Endpoints: ") | ftxui::dim,
                ftxui::text(std::to_string(p.endpoints_count)) | ftxui::color(ftxui::Color::Green),
                ftxui::text("  Status: ") | ftxui::dim,
                ftxui::text(p.is_active ? "Active" : "Inactive") | ftxui::color(p.is_active ? ftxui::Color::Green : ftxui::Color::Red),
            }),
        });

        // Get topics for this participant
        auto topics = engine_.GetParticipantTopics(p.guid);

        ftxui::Elements topic_lines;

        if (topics.empty()) {
            topic_lines.push_back(ftxui::text("  No topics discovered yet...") | ftxui::dim);
        } else {
            // Column widths
            constexpr int COL_TOPIC = 36;
            constexpr int COL_ROLE = 5;
            constexpr int COL_MATCH = 20;
            constexpr int COL_STATUS = 9;
            constexpr int COL_COUNT = 9;
            constexpr int COL_BYTES = 9;
            constexpr int COL_LOSS = 8;
            constexpr int COL_NACK = 7;
            constexpr int COL_RETRANS = 7;
            constexpr int COL_FREQ = 9;

            // Table header
            topic_lines.push_back(ftxui::hbox({
                ftxui::text("  Topic Name") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, COL_TOPIC),
                ftxui::text("Role") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, COL_ROLE),
                ftxui::text("Match With") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, COL_MATCH),
                ftxui::text("Status") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, COL_STATUS),
                ftxui::text("Count") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, COL_COUNT) | ftxui::align_right,
                ftxui::text("Bytes") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, COL_BYTES) | ftxui::align_right,
                ftxui::text("Loss%") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, COL_LOSS) | ftxui::align_right,
                ftxui::text("NACK") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, COL_NACK) | ftxui::align_right,
                ftxui::text("Ret%") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, COL_RETRANS) | ftxui::align_right,
                ftxui::text("Freq") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, COL_FREQ) | ftxui::align_right,
            }));
            topic_lines.push_back(ftxui::separator());

            // Topic rows
            for (const auto& topic : topics) {
                // Find matching info from topic_matches_
                const MetricsEngine::TopicMatchInfo* match_info = nullptr;
                for (const auto& m : topic_matches_) {
                    if (m.topic_name == topic.topic_name) {
                        match_info = &m;
                        break;
                    }
                }

                // Determine role
                std::string role;
                if (topic.has_writer && topic.has_reader) {
                    role = "W+R";
                } else if (topic.has_writer) {
                    role = "W";
                } else {
                    role = "R";
                }

                // Build matched participants list
                std::string matched_with;
                if (match_info) {
                    std::vector<std::string> matched_names;
                    if (topic.has_writer) {
                        for (const auto& reader : match_info->reader_participants) {
                            if (reader != name) {
                                matched_names.push_back(reader);
                            }
                        }
                    } else {
                        for (const auto& writer : match_info->writer_participants) {
                            if (writer != name) {
                                matched_names.push_back(writer);
                            }
                        }
                    }

                    if (matched_names.empty()) {
                        matched_with = "--";
                    } else {
                        for (size_t j = 0; j < matched_names.size(); ++j) {
                            if (j > 0) matched_with += ", ";
                            matched_with += matched_names[j];
                        }
                    }
                } else {
                    matched_with = "--";
                }

                // Truncate match names if too long
                if (matched_with.size() > static_cast<size_t>(COL_MATCH - 1)) {
                    matched_with = matched_with.substr(0, COL_MATCH - 4) + "...";
                }

                // Match status
                bool matched = match_info && match_info->is_matched;
                std::string match_status = matched ? "Matched" : "No";

                // Format loss rate
                char loss_str[16];
                if (topic.loss_rate > 0.001) {
                    snprintf(loss_str, sizeof(loss_str), "%.1f%%", topic.loss_rate * 100.0);
                } else {
                    snprintf(loss_str, sizeof(loss_str), "0%%");
                }

                // Format retransmit rate
                char retrans_str[16];
                if (topic.retransmit_rate > 0.001) {
                    snprintf(retrans_str, sizeof(retrans_str), "%.1f%%", topic.retransmit_rate * 100.0);
                } else {
                    snprintf(retrans_str, sizeof(retrans_str), "0%%");
                }

                // Format frequency
                char freq_str[16];
                if (topic.send_frequency_hz > 0.1) {
                    snprintf(freq_str, sizeof(freq_str), "%.1f Hz", topic.send_frequency_hz);
                } else {
                    snprintf(freq_str, sizeof(freq_str), "--");
                }

                // Color elements
                auto status_elem = matched ?
                    (ftxui::text(match_status) | ftxui::color(ftxui::Color::Green)) :
                    (ftxui::text(match_status) | ftxui::color(ftxui::Color::Red));

                auto loss_elem = topic.loss_rate > 0.01 ?
                    (ftxui::text(loss_str) | ftxui::color(ftxui::Color::Red)) :
                    ftxui::text(loss_str);

                auto nack_elem = topic.nack_count > 0 ?
                    (ftxui::text(std::to_string(topic.nack_count)) | ftxui::color(ftxui::Color::Yellow)) :
                    ftxui::text(std::to_string(topic.nack_count));

                auto retrans_elem = topic.retransmit_rate > 0.01 ?
                    (ftxui::text(retrans_str) | ftxui::color(ftxui::Color::Red)) :
                    ftxui::text(retrans_str);

                topic_lines.push_back(ftxui::hbox({
                    ftxui::text(topic.topic_name) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, COL_TOPIC),
                    ftxui::text(role) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, COL_ROLE),
                    ftxui::text(matched_with) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, COL_MATCH),
                    status_elem | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, COL_STATUS),
                    ftxui::text(FormatNumber(topic.data_count)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, COL_COUNT) | ftxui::align_right,
                    ftxui::text(FormatSize(topic.bytes_sent)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, COL_BYTES) | ftxui::align_right,
                    loss_elem | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, COL_LOSS) | ftxui::align_right,
                    nack_elem | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, COL_NACK) | ftxui::align_right,
                    retrans_elem | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, COL_RETRANS) | ftxui::align_right,
                    ftxui::text(freq_str) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, COL_FREQ) | ftxui::align_right,
                }));
            }
        }

        // Navigation hint
        auto nav_hint = ftxui::text("[ESC: Back to List] [q: Quit]") | ftxui::dim | ftxui::center;

        return ftxui::vbox({
            ftxui::text("Participant Details") | ftxui::bold | ftxui::center,
            ftxui::separator(),
            info,
            ftxui::separator(),
            ftxui::text("Topics:") | ftxui::bold,
            ftxui::vbox(std::move(topic_lines)) | ftxui::flex,
            ftxui::separator(),
            nav_hint,
        });
    });

    return component;
}

ftxui::Component MonitorUi::BuildStatusBar() {
    return ftxui::Renderer([this] {
        auto dropped = worker_.GetDropped();
        auto s = summary_;  // Use cached data

        // Dynamic hint based on current view mode
        std::string mode_hint;
        if (view_mode_ == ViewMode::LIST_VIEW) {
            mode_hint = "[↑↓ Navigate] [Enter: Details] [q: Quit]";
        } else {
            mode_hint = "[ESC: Back] [q: Quit]";
        }

        return ftxui::hbox({
            ftxui::text(" " + mode_hint) | ftxui::dim,
            ftxui::filler(),
            ftxui::text("Packets: " + FormatNumber(s.total_data_messages) + "  "),
            ftxui::text("Dropped: " + std::to_string(dropped) + "  ") |
                (dropped > 0 ? ftxui::color(ftxui::Color::Red) : ftxui::color(ftxui::Color::Default)),
        }) | ftxui::border;
    });
}

void MonitorUi::Run() {
    running_.store(true);

    // Build the two view components
    auto list_view = BuildListView();
    auto detail_view = BuildDetailView();
    auto status_bar = BuildStatusBar();

    // Container with all components
    ftxui::Components children = {list_view, detail_view, status_bar};
    auto container = ftxui::Container::Vertical(children);

    // Main component with conditional view switching
    auto component = ftxui::Renderer(container, [&] {
        ftxui::Element main_content;

        if (view_mode_ == ViewMode::LIST_VIEW) {
            main_content = list_view->Render();
        } else {
            main_content = detail_view->Render();
        }

        return ftxui::vbox({
            main_content | ftxui::flex,
            status_bar->Render(),
        });
    });

    // Global keyboard handling
    component |= ftxui::CatchEvent([&](ftxui::Event event) {
        // q to quit
        if (event == ftxui::Event::Character('q')) {
            running_.store(false);
            return true;
        }
        // ESC to go back from detail to list
        if (event == ftxui::Event::Escape) {
            if (view_mode_ == ViewMode::DETAIL_VIEW) {
                view_mode_ = ViewMode::LIST_VIEW;
                return true;
            }
            // If already in list view, ESC quits
            running_.store(false);
            return true;
        }
        return false;
    });

    // Data processing thread
    std::thread process_thread([this] {
        RawPacket packets[64];
        while (running_.load()) {
            size_t count = worker_.PopPackets(packets, 64);
            for (size_t i = 0; i < count; ++i) {
                RtpsMessage msg;
                bool parsed = RtpsParser::ParseHeader(packets[i].data, packets[i].len, msg);
                if (parsed) {
                    struct Context {
                        MetricsEngine* engine;
                        uint64_t timestamp;
                        uint16_t src_port;
                        uint16_t dst_port;
                    };
                    Context ctx{&engine_, packets[i].timestamp_us,
                                packets[i].src_port, packets[i].dst_port};

                    RtpsParser::ParseSubmessages(
                        packets[i].data, packets[i].len,
                        msg.source_guid_prefix, &ctx,
                        [](void* user, DataSubmessage& data) {
                            auto* ctx = static_cast<Context*>(user);
                            data.src_port = ctx->src_port;
                            data.dst_port = ctx->dst_port;
                            ctx->engine->OnData(data, ctx->timestamp);
                        },
                        [](void* user, const HeartbeatSubmessage& hb) {
                            auto* ctx = static_cast<Context*>(user);
                            ctx->engine->OnHeartbeat(hb, ctx->timestamp);
                        },
                        [](void* user, const AcknackSubmessage& ack) {
                            auto* ctx = static_cast<Context*>(user);
                            ctx->engine->OnAcknack(ack, ctx->timestamp);
                        }
                    );
                }
            }
            if (count == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    });

    // Use FTXUI Loop with manual frame control
    ftxui::Loop loop(&screen_, component);

    while (!loop.HasQuitted() && running_.load()) {
        // Refresh data
        RefreshData();

        // Force a re-render by posting a custom event
        screen_.PostEvent(ftxui::Event::Custom);

        // Run one frame of the FTXUI loop
        loop.RunOnce();

        // Sleep to control frame rate (~10 FPS)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Cleanup
    running_.store(false);
    if (process_thread.joinable()) {
        process_thread.join();
    }
}

}  // namespace ondemand_monitor
