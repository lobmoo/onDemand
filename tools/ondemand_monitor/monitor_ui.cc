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

    // Get endpoints for selected participant
    if (selected_participant_ < static_cast<int>(participants_.size())) {
        endpoints_ = engine_.GetEndpoints(participants_[selected_participant_].guid);
    } else {
        endpoints_.clear();
    }
}

ftxui::Component MonitorUi::BuildOverviewPage() {
    return ftxui::Renderer([this] {
        // Use cached data from RefreshData()
        auto s = summary_;
        auto participants = participants_;

        // Group participants by domain ID, filtering out noise participants
        std::map<uint32_t, std::vector<ParticipantInfo>> domain_map;
        for (const auto& p : participants) {
            // Skip participants that are likely noise:
            // - Domain 0 with no name (other DDS nodes on network)
            // - Domain 0 with name but no endpoints (inactive discovery)
            if (p.domain_id == 0 && p.name.empty()) {
                continue;
            }
            // Also skip Domain 0 participants with name but no endpoints and no recent activity
            if (p.domain_id == 0 && p.endpoints_count == 0 && !p.is_active) {
                continue;
            }
            domain_map[p.domain_id].push_back(p);
        }

        // Stats summary (compact, single line)
        auto stats = ftxui::vbox({
            ftxui::hbox({
                ftxui::text("Domain: ") | ftxui::bold,
                ftxui::text(std::to_string(domain_map.size())) | ftxui::color(ftxui::Color::Green),
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

        // Build participant table with fixed-width columns for alignment
        ftxui::Elements table_lines;

        // Column widths for alignment
        constexpr int COL_DOMAIN = 10;
        constexpr int COL_NAME = 21;
        constexpr int COL_ENDPOINTS = 11;
        constexpr int COL_HEARTBEAT = 11;
        constexpr int COL_ACKNACK = 11;
        constexpr int COL_NACK = 11;
        constexpr int COL_STATUS = 12;

        // Table header with consistent column widths
        char hdr_domain[16], hdr_name[32], hdr_endpoints[16], hdr_heartbeat[16], hdr_acknack[16], hdr_nack[16], hdr_status[16];
        snprintf(hdr_domain, sizeof(hdr_domain), "%-*s", COL_DOMAIN, "Domain");
        snprintf(hdr_name, sizeof(hdr_name), "%-*s", COL_NAME, "Participant Name");
        snprintf(hdr_endpoints, sizeof(hdr_endpoints), "%-*s", COL_ENDPOINTS, "Endpoints");
        snprintf(hdr_heartbeat, sizeof(hdr_heartbeat), "%-*s", COL_HEARTBEAT, "Heartbeat");
        snprintf(hdr_acknack, sizeof(hdr_acknack), "%-*s", COL_ACKNACK, "ACKNACK");
        snprintf(hdr_nack, sizeof(hdr_nack), "%-*s", COL_NACK, "NACK");
        snprintf(hdr_status, sizeof(hdr_status), "%-*s", COL_STATUS, "Status");

        table_lines.push_back(ftxui::hbox({
            ftxui::text("  "),
            ftxui::text(hdr_domain) | ftxui::bold,
            ftxui::text(hdr_name) | ftxui::bold,
            ftxui::text(hdr_endpoints) | ftxui::bold,
            ftxui::text(hdr_heartbeat) | ftxui::bold,
            ftxui::text(hdr_acknack) | ftxui::bold,
            ftxui::text(hdr_nack) | ftxui::bold,
            ftxui::text(hdr_status) | ftxui::bold,
        }));
        table_lines.push_back(ftxui::separator());

        // Table rows
        for (const auto& [domain_id, domain_participants] : domain_map) {
            for (size_t i = 0; i < domain_participants.size(); ++i) {
                const auto& p = domain_participants[i];

                // Format participant name (use GUID if name is empty)
                std::string name = p.name.empty() ? "Participant-" + std::to_string(i + 1) : p.name;

                // Format all columns with consistent widths
                char domain_str[16], name_str[32], endpoints_str[16], heartbeat_str[16], acknack_str[16], nack_str[16], status_str[16];
                snprintf(domain_str, sizeof(domain_str), "%-*u", COL_DOMAIN, domain_id);
                snprintf(name_str, sizeof(name_str), "%-*s", COL_NAME, name.c_str());
                snprintf(endpoints_str, sizeof(endpoints_str), "%-*u", COL_ENDPOINTS, p.endpoints_count);
                snprintf(heartbeat_str, sizeof(heartbeat_str), "%-*llu", COL_HEARTBEAT, (unsigned long long)p.heartbeat_count);
                snprintf(acknack_str, sizeof(acknack_str), "%-*llu", COL_ACKNACK, (unsigned long long)p.acknack_count);
                snprintf(nack_str, sizeof(nack_str), "%-*llu", COL_NACK, (unsigned long long)p.nack_count);
                snprintf(status_str, sizeof(status_str), "%-*s", COL_STATUS, p.is_active ? "Active" : "Inactive");

                auto status_color = p.is_active ? ftxui::Color::Green : ftxui::Color::Red;

                table_lines.push_back(ftxui::hbox({
                    ftxui::text("  "),
                    ftxui::text(domain_str),
                    ftxui::text(name_str),
                    ftxui::text(endpoints_str),
                    ftxui::text(heartbeat_str),
                    ftxui::text(acknack_str),
                    ftxui::text(nack_str),
                    ftxui::text(status_str) | ftxui::color(status_color),
                }));
            }
        }

        // If no participants found
        if (domain_map.empty()) {
            table_lines.push_back(ftxui::text("  No participants discovered yet...") | ftxui::dim);
        }

        auto table = ftxui::vbox(std::move(table_lines)) | ftxui::border;

        return ftxui::vbox({
            ftxui::text("OnDemand Monitor - Discovery Overview") | ftxui::bold | ftxui::center,
            ftxui::separator(),
            stats,
            ftxui::separator(),
            table | ftxui::flex,
        });
    });
}

ftxui::Component MonitorUi::BuildParticipantsPage() {
    return ftxui::Renderer([this] {
        ftxui::Elements lines;

        // Use cached topic matches
        auto all_matches = topic_matches_;

        // Fixed widths for all columns
        constexpr int COL_TOPIC = 40;    // Topic Name
        constexpr int COL_ROLE = 6;      // Role
        constexpr int COL_MATCH = 20;    // Match With
        constexpr int COL_STATUS = 10;   // Status (longest: "Unmatched" = 9 chars)
        constexpr int COL_COUNT = 10;    // Data Count
        constexpr int COL_BYTES = 10;    // Bytes
        constexpr int COL_NACK = 8;      // NACK

        // Group topics by participant, filtering out Domain 0 noise
        for (size_t i = 0; i < participants_.size(); ++i) {
            auto& p = participants_[i];

            // Skip Domain 0 participants with no name (noise from network)
            if (p.domain_id == 0 && p.name.empty()) {
                continue;
            }
            // Skip Domain 0 participants with no endpoints and inactive
            if (p.domain_id == 0 && p.endpoints_count == 0 && !p.is_active) {
                continue;
            }

            std::string name = p.name.empty() ? "Participant-" + std::to_string(i + 1) : p.name;

            // Participant header
            lines.push_back(ftxui::hbox({
                ftxui::text("▼ " + name) | ftxui::bold | ftxui::color(ftxui::Color::Cyan),
                ftxui::text(" (Domain: " + std::to_string(p.domain_id) + ")") | ftxui::dim,
                ftxui::text("  Endpoints: " + std::to_string(p.endpoints_count)) | ftxui::dim,
            }));

            // Get topics for this participant
            auto topics = engine_.GetParticipantTopics(p.guid);

            if (topics.empty()) {
                lines.push_back(ftxui::text("    No topics discovered yet...") | ftxui::dim);
            } else {
                // Table header with fixed widths
                char hdr_topic[64], hdr_role[16], hdr_match[32], hdr_status[16], hdr_count[16], hdr_bytes[16], hdr_nack[16];
                snprintf(hdr_topic, sizeof(hdr_topic), "    %-*s", COL_TOPIC - 4, "Topic Name");
                snprintf(hdr_role, sizeof(hdr_role), "%-*s", COL_ROLE, "Role");
                snprintf(hdr_match, sizeof(hdr_match), "%-*s", COL_MATCH, "Match With");
                snprintf(hdr_status, sizeof(hdr_status), "%-*s", COL_STATUS, "Status");
                snprintf(hdr_count, sizeof(hdr_count), "%-*s", COL_COUNT, "Count");
                snprintf(hdr_bytes, sizeof(hdr_bytes), "%-*s", COL_BYTES, "Bytes");
                snprintf(hdr_nack, sizeof(hdr_nack), "%-*s", COL_NACK, "NACK");

                lines.push_back(ftxui::hbox({
                    ftxui::text(hdr_topic) | ftxui::bold,
                    ftxui::text(hdr_role) | ftxui::bold,
                    ftxui::text(hdr_match) | ftxui::bold,
                    ftxui::text(hdr_status) | ftxui::bold,
                    ftxui::text(hdr_count) | ftxui::bold,
                    ftxui::text(hdr_bytes) | ftxui::bold,
                    ftxui::text(hdr_nack) | ftxui::bold,
                }));
                lines.push_back(ftxui::separator());

                // Topic rows
                for (const auto& topic : topics) {
                    // Find matching info from all_matches
                    const MetricsEngine::TopicMatchInfo* match_info = nullptr;
                    for (const auto& m : all_matches) {
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

                    // Format all columns with fixed widths
                    char col_topic[64], col_role[16], col_match[32], col_status[16], col_count[16], col_bytes[16], col_nack[16];
                    snprintf(col_topic, sizeof(col_topic), "    %-*s", COL_TOPIC - 4, topic.topic_name.c_str());
                    snprintf(col_role, sizeof(col_role), "%-*s", COL_ROLE, role.c_str());
                    snprintf(col_match, sizeof(col_match), "%-*s", COL_MATCH, matched_with.c_str());
                    snprintf(col_status, sizeof(col_status), "%-*s", COL_STATUS, match_status.c_str());
                    snprintf(col_count, sizeof(col_count), "%-*llu", COL_COUNT, (unsigned long long)topic.data_count);
                    snprintf(col_bytes, sizeof(col_bytes), "%-*s", COL_BYTES, FormatSize(topic.bytes_sent).c_str());
                    snprintf(col_nack, sizeof(col_nack), "%-*llu", COL_NACK, (unsigned long long)topic.nack_count);

                    // Color elements
                    auto status_elem = matched ?
                        (ftxui::text(col_status) | ftxui::color(ftxui::Color::Green)) :
                        (ftxui::text(col_status) | ftxui::color(ftxui::Color::Red));

                    auto nack_elem = topic.nack_count > 0 ?
                        (ftxui::text(col_nack) | ftxui::color(ftxui::Color::Red)) :
                        ftxui::text(col_nack);

                    lines.push_back(ftxui::hbox({
                        ftxui::text(col_topic),
                        ftxui::text(col_role),
                        ftxui::text(col_match),
                        status_elem,
                        ftxui::text(col_count),
                        ftxui::text(col_bytes),
                        nack_elem,
                    }));
                }
            }

            lines.push_back(ftxui::separator());
        }

        if (participants_.empty()) {
            lines.push_back(ftxui::text("No participants discovered yet...") | ftxui::dim | ftxui::center);
        }

        return ftxui::vbox({
            ftxui::text("Participants - Topic Matching") | ftxui::bold | ftxui::center,
            ftxui::separator(),
            ftxui::vbox(std::move(lines)) | ftxui::yframe,
        });
    });
}

ftxui::Component MonitorUi::BuildEndpointsPage() {
    return ftxui::Renderer([this] {
        ftxui::Elements lines;

        // Header
        {
            ftxui::Elements header;
            header.push_back(ftxui::text("R") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 2));
            header.push_back(ftxui::text(" | "));
            header.push_back(ftxui::text("Topic") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 36));
            header.push_back(ftxui::text(" | "));
            header.push_back(ftxui::text("DataCount") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 10));
            header.push_back(ftxui::text(" | "));
            header.push_back(ftxui::text("Bytes") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 9));
            header.push_back(ftxui::text(" | "));
            header.push_back(ftxui::text("Loss%") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 8));
            header.push_back(ftxui::text(" | "));
            header.push_back(ftxui::text("Lost") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 8));
            header.push_back(ftxui::text(" | "));
            header.push_back(ftxui::text("NACK") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 6));
            header.push_back(ftxui::text(" | "));
            header.push_back(ftxui::text("LastSeen") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 8));
            lines.push_back(ftxui::hbox(std::move(header)));
        }
        lines.push_back(ftxui::separator());

        auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

        for (size_t i = 0; i < endpoints_.size(); ++i) {
            auto& ep = endpoints_[i];

            // Calculate packet loss rate
            std::string loss_pct;
            if (ep.data_count > 0 && ep.lost_count > 0) {
                double rate = (double)ep.lost_count / (double)(ep.data_count + ep.lost_count) * 100.0;
                char buf[16];
                snprintf(buf, sizeof(buf), "%.2f%%", rate);
                loss_pct = buf;
            } else {
                loss_pct = "0%";
            }

            // Format last seen age
            std::string last_seen;
            if (ep.last_seen_us > 0 && now_us > ep.last_seen_us) {
                auto age_ms = (now_us - ep.last_seen_us) / 1000;
                if (age_ms < 1000) {
                    last_seen = std::to_string(age_ms) + "ms";
                } else {
                    last_seen = std::to_string(age_ms / 1000) + "s";
                }
            } else {
                last_seen = "-";
            }

            ftxui::Elements row;
            row.push_back(ftxui::text(ep.is_writer ? "W" : "R") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 2));
            row.push_back(ftxui::text(" | "));
            row.push_back(ftxui::text(ep.topic_name) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 36) | ftxui::bold);
            row.push_back(ftxui::text(" | "));
            row.push_back(ftxui::text(std::to_string(ep.data_count)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 10) | ftxui::align_right);
            row.push_back(ftxui::text(" | "));
            row.push_back(ftxui::text(FormatSize(ep.bytes_sent)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 9) | ftxui::align_right);
            row.push_back(ftxui::text(" | "));
            if (ep.lost_count > 0) {
                row.push_back(ftxui::text(loss_pct) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 8) | ftxui::align_right | ftxui::color(ftxui::Color::Red));
            } else {
                row.push_back(ftxui::text(loss_pct) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 8) | ftxui::align_right);
            }
            row.push_back(ftxui::text(" | "));
            if (ep.lost_count > 0) {
                row.push_back(ftxui::text(std::to_string(ep.lost_count)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 8) | ftxui::align_right | ftxui::color(ftxui::Color::Red));
            } else {
                row.push_back(ftxui::text(std::to_string(ep.lost_count)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 8) | ftxui::align_right);
            }
            row.push_back(ftxui::text(" | "));
            row.push_back(ftxui::text(std::to_string(ep.nack_count)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 6) | ftxui::align_right);
            row.push_back(ftxui::text(" | "));
            row.push_back(ftxui::text(last_seen) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 8) | ftxui::align_right);

            lines.push_back(ftxui::hbox(std::move(row)) | (selected_endpoint_ == static_cast<int>(i) ? ftxui::inverted : ftxui::nothing));
        }

        return ftxui::vbox({
            ftxui::text("Endpoints  [↑↓ Navigate]") | ftxui::bold | ftxui::center,
            ftxui::separator(),
            ftxui::vbox(std::move(lines)) | ftxui::yframe,
        });
    });
}

ftxui::Component MonitorUi::BuildTransferPage() {
    return ftxui::Renderer([this] {
        return ftxui::vbox({
            ftxui::text("Transfer Stats") | ftxui::bold | ftxui::center,
            ftxui::separator(),
            ftxui::text(""),
            ftxui::text("  [TODO] 此页面待开发...") | ftxui::dim,
            ftxui::text(""),
        });
    });
}

ftxui::Component MonitorUi::BuildStatusBar() {
    return ftxui::Renderer([this] {
        auto dropped = worker_.GetDropped();
        auto s = summary_;  // Use cached data
        return ftxui::hbox({
            ftxui::text(" [Tab] Switch Page  [q] Quit") | ftxui::dim,
            ftxui::filler(),
            ftxui::text("Packets: " + FormatNumber(s.total_data_messages) + "  "),
            ftxui::text("Dropped: " + std::to_string(dropped) + "  ") |
                (dropped > 0 ? ftxui::color(ftxui::Color::Red) : ftxui::color(ftxui::Color::Default)),
        }) | ftxui::border;
    });
}

void MonitorUi::Run() {
    running_.store(true);

    // Build tab structure
    auto overview = BuildOverviewPage();
    auto participants = BuildParticipantsPage();
    auto endpoints = BuildEndpointsPage();
    auto transfer = BuildTransferPage();

    // Use Container::Tab
    ftxui::Components pages = {overview, participants, endpoints, transfer};
    auto tabs = ftxui::Container::Tab(pages, &selected_tab_);

    // Use Toggle for tab selection
    std::vector<std::string> tab_labels = {
        "1:Overview", "2:Participants", "3:Endpoints", "4:Transfer"
    };
    ftxui::Component tab_selector = ftxui::Toggle(tab_labels, &selected_tab_);

    auto status_bar = BuildStatusBar();

    ftxui::Components main_components = {tab_selector, tabs, status_bar};
    auto layout = ftxui::Container::Vertical(main_components);

    auto component = ftxui::Renderer(layout, [&] {
        return ftxui::vbox({
            tab_selector->Render(),
            ftxui::separator(),
            tabs->Render() | ftxui::flex,
            status_bar->Render(),
        });
    });

    // Keyboard handling
    component |= ftxui::CatchEvent([&](ftxui::Event event) {
        if (event == ftxui::Event::Character('q') || event == ftxui::Event::Escape) {
            running_.store(false);
            return true;
        }
        if (event == ftxui::Event::Tab) {
            selected_tab_ = (selected_tab_ + 1) % 4;
            return true;
        }
        if (event == ftxui::Event::TabReverse) {
            selected_tab_ = (selected_tab_ + 3) % 4;
            return true;
        }
        // Number keys for direct tab selection
        if (event == ftxui::Event::Character('1')) {
            selected_tab_ = 0;
            return true;
        }
        if (event == ftxui::Event::Character('2')) {
            selected_tab_ = 1;
            return true;
        }
        if (event == ftxui::Event::Character('3')) {
            selected_tab_ = 2;
            return true;
        }
        if (event == ftxui::Event::Character('4')) {
            selected_tab_ = 3;
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
