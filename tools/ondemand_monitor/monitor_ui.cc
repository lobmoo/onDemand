#include "monitor_ui.h"
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdio>
#include <arpa/inet.h>
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

void MonitorUi::UpdateTopicCache() {
    cached_topics_.clear();
    std::vector<const ParticipantInfo*> filtered;
    for (const auto& p : participants_) {
        // 只过滤完全无效的participant（无endpoints且不活跃）
        if (p.endpoints_count == 0 && !p.is_active) continue;
        filtered.push_back(&p);
    }
    if (!filtered.empty() && selected_index_ >= 0 &&
        selected_index_ < static_cast<int>(filtered.size())) {
        cached_topics_ = engine_.GetParticipantTopics(filtered[selected_index_]->guid);
    }
}

void MonitorUi::RefreshData() {
    summary_ = engine_.GetSummary();
    participants_ = engine_.GetParticipants();
    transfers_ = engine_.GetTransferStats();
    topic_matches_ = engine_.GetAllTopicMatches();
    submsg_counts_ = engine_.GetSubmsgIdCounts();
    UpdateTopicCache();
}

ftxui::Component MonitorUi::BuildListView() {
    // Update menu entries from participants
    auto update_entries = [this] {
        menu_entries_.clear();
        for (const auto& p : participants_) {
            if (p.domain_id == 0 && p.name.empty()) continue;
            if (p.domain_id == 0 && p.endpoints_count == 0 && !p.is_active) continue;

            std::string name = p.name.empty() ? "Participant" : p.name;
            char entry[192];
            snprintf(entry, sizeof(entry),
                     "[Domain:%u] %-16s Endpoints:%-4u %-7s IP:%-15s Mcast:%s",
                     p.domain_id, name.c_str(), p.endpoints_count,
                     p.is_active ? "Active" : "Inactive",
                     p.src_ip.empty() ? "-" : p.src_ip.c_str(),
                     p.multicast_ip.empty() ? "-" : p.multicast_ip.c_str());
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

    // Build the topic-preview table for one node (compact single-line rows).
    // Used by the home-page preview pane; selection changes refresh it live.
    // Fixed-width snprintf cells — same alignment technique as detail view.
    auto build_preview = [this](const ParticipantInfo& p, const std::string& name,
                                ftxui::Elements& out) {
        constexpr int COL_TOPIC = 46;   // fits dsf/message/commandRequest/subTableRegister
        constexpr int COL_ROLE = 5;
        constexpr int COL_MATCH = 14;
        constexpr int COL_STATUS = 9;
        constexpr int COL_COUNT = 8;
        constexpr int COL_BYTES = 9;
        constexpr int COL_LOSS = 7;
        constexpr int COL_NACK = 6;
        constexpr int COL_RET = 7;
        constexpr int COL_RETCNT = 6;

        char hdr[512];
        snprintf(hdr, sizeof(hdr),
                 "  %-*s %-*s %-*s %-*s %*s %*s %*s %*s %*s %*s",
                 COL_TOPIC - 2, "Topic Name", COL_ROLE, "Role",
                 COL_MATCH, "Match With", COL_STATUS, "Status",
                 COL_COUNT, "Count", COL_BYTES, "Bytes", COL_LOSS, "Loss%",
                 COL_NACK, "NACK", COL_RET, "Ret%", COL_RETCNT, "Ret#");
        out.push_back(ftxui::text(hdr) | ftxui::bold);
        out.push_back(ftxui::separator());

        // Render from the per-tick cache (see RefreshData) — querying the
        // engine here ran a locked full-endpoint scan on every frame
        std::vector<MetricsEngine::TopicInfo> all_topics = cached_topics_;

        // **FIX: Filter out bucket transfer topics on home page preview**
        // Home-page preview shows only control/business topics (tableDefine, register).
        // The 20 bucket transfer topics flood the pane — those stay in detail view only.
        all_topics.erase(
            std::remove_if(all_topics.begin(), all_topics.end(),
                           [](const MetricsEngine::TopicInfo& t) {
                               return t.topic_name.find("bucket_") != std::string::npos;
                           }),
            all_topics.end());

        if (all_topics.empty()) {
            out.push_back(ftxui::text("  No topics discovered yet...") | ftxui::dim);
            return;
        }

        // Business-first ordering: TableDefine -> Registration
        std::sort(all_topics.begin(), all_topics.end(),
                  [](const MetricsEngine::TopicInfo& a,
                     const MetricsEngine::TopicInfo& b) {
                      int ca = a.topic_name.find("tableDefine") != std::string::npos ? 0 :
                               a.topic_name.find("subTableRegister") != std::string::npos ? 1 : 2;
                      int cb = b.topic_name.find("tableDefine") != std::string::npos ? 0 :
                               b.topic_name.find("subTableRegister") != std::string::npos ? 1 : 2;
                      if (ca != cb) return ca < cb;
                      return a.topic_name < b.topic_name;
                  });

        for (const auto& topic : all_topics) {
            const MetricsEngine::TopicMatchInfo* match_info = nullptr;
            for (const auto& m : topic_matches_) {
                if (m.topic_name == topic.topic_name) {
                    match_info = &m;
                    break;
                }
            }

            std::string role;
            if (topic.has_writer && topic.has_reader) role = "W+R";
            else if (topic.has_writer) role = "W";
            else role = "R";

            bool matched = match_info && match_info->is_matched;

            // Opposite-side participants for this node
            std::string matched_with;
            if (match_info) {
                std::vector<std::string> names;
                if (topic.has_writer) {
                    for (const auto& r : match_info->reader_participants)
                        if (r != name) names.push_back(r);
                } else {
                    for (const auto& w : match_info->writer_participants)
                        if (w != name) names.push_back(w);
                }
                for (size_t j = 0; j < names.size(); ++j) {
                    if (j > 0) matched_with += ",";
                    matched_with += names[j];
                }
            }
            if (matched_with.empty()) matched_with = "--";
            if (matched_with.size() > static_cast<size_t>(COL_MATCH - 1))
                matched_with = matched_with.substr(0, COL_MATCH - 4) + "...";

            std::string topic_cut = topic.topic_name.substr(0, COL_TOPIC - 2);
            char prefix[128], status_str[16], count_str[16], bytes_str[16],
                 loss_str[16], nack_str[16], ret_str[16], retrans_cnt_str[16];
            snprintf(prefix, sizeof(prefix), "  %-*s %-*s %-*s ",
                     COL_TOPIC - 2, topic_cut.c_str(),
                     COL_ROLE, role.c_str(),
                     COL_MATCH, matched_with.c_str());
            snprintf(status_str, sizeof(status_str), "%-*s", COL_STATUS,
                     matched ? "Matched" : "Unmatched");
            snprintf(count_str, sizeof(count_str), " %*llu", COL_COUNT, (unsigned long long)topic.data_count);
            snprintf(bytes_str, sizeof(bytes_str), " %*s", COL_BYTES, FormatSize(topic.bytes_sent).c_str());
            if (topic.lost_count > 0)
                snprintf(loss_str, sizeof(loss_str), " %*.1f%%", COL_LOSS - 1, topic.loss_rate * 100.0);
            else
                snprintf(loss_str, sizeof(loss_str), " %*s", COL_LOSS - 1, "0.0%");
            snprintf(nack_str, sizeof(nack_str), " %*llu", COL_NACK, (unsigned long long)topic.nack_count);
            if (topic.retransmit_count > 0)
                snprintf(ret_str, sizeof(ret_str), " %*.1f%%", COL_RET - 1, topic.retransmit_rate * 100.0);
            else
                snprintf(ret_str, sizeof(ret_str), " %*s", COL_RET - 1, "0.0%");
            snprintf(retrans_cnt_str, sizeof(retrans_cnt_str), " %*llu", COL_RETCNT, (unsigned long long)topic.retransmit_count);

            out.push_back(ftxui::hbox({
                ftxui::text(prefix),
                matched ? ftxui::text(status_str) | ftxui::color(ftxui::Color::Green)
                        : ftxui::text(status_str) | ftxui::color(ftxui::Color::Red),
                ftxui::text(count_str),
                ftxui::text(bytes_str),
                topic.lost_count > 0 ? ftxui::text(loss_str) | ftxui::color(ftxui::Color::Red)
                                     : ftxui::text(loss_str),
                topic.nack_count > 0 ? ftxui::text(nack_str) | ftxui::color(ftxui::Color::Red)
                                     : ftxui::text(nack_str),
                topic.retransmit_count > 0 ? ftxui::text(ret_str) | ftxui::color(ftxui::Color::Yellow)
                                           : ftxui::text(ret_str),
                topic.retransmit_count > 0 ? ftxui::text(retrans_cnt_str) | ftxui::color(ftxui::Color::Yellow)
                                           : ftxui::text(retrans_cnt_str),
            }));
        }
    };

    // Wrap menu with stats display.
    // NOTE: `menu` MUST be captured BY VALUE here. Capturing it by reference
    // (`&menu`) dangles the moment BuildListView returns — every later frame
    // dereferences a dead stack slot (observed as 100%-CPU render spin and
    // SIGSEGV inside ComponentBase::Render).
    return ftxui::Renderer(menu, [this, menu, update_entries, build_preview] {
        // Update entries each frame
        update_entries();

        // Stats summary
        auto s = summary_;
        auto stats = ftxui::vbox({
            ftxui::hbox({
                ftxui::text("Participant: ") | ftxui::bold,
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
                ftxui::text("(raw:") | ftxui::dim,
                ftxui::text(FormatNumber(s.total_heartbeats_raw)) | ftxui::dim,
                ftxui::text(")  ACKNACK: ") | ftxui::dim,
                ftxui::text(FormatNumber(s.total_acknacks)) | ftxui::color(ftxui::Color::Yellow),
                ftxui::text("  NACK: ") | ftxui::bold,
                ftxui::text(FormatNumber(s.total_nacks)) | ftxui::color(ftxui::Color::Red),
            }),
            ftxui::hbox({
                ftxui::text("SubmsgIDs: ") | ftxui::bold | ftxui::dim,
                ftxui::text("DATA=") | ftxui::dim,
                ftxui::text(FormatNumber(submsg_counts_[0x15])) | ftxui::color(ftxui::Color::Cyan),
                ftxui::text(" HB=") | ftxui::dim,
                ftxui::text(FormatNumber(submsg_counts_[0x07])) | ftxui::color(ftxui::Color::Yellow),
                ftxui::text(" ACK=") | ftxui::dim,
                ftxui::text(FormatNumber(submsg_counts_[0x06])) | ftxui::color(ftxui::Color::Yellow),
                ftxui::text(" FRAG=") | ftxui::dim,
                ftxui::text(FormatNumber(submsg_counts_[0x16])) | ftxui::color(ftxui::Color::Cyan),
                ftxui::text(" GAP=") | ftxui::dim,
                ftxui::text(FormatNumber(submsg_counts_[0x08])) | ftxui::dim,
                ftxui::text(" INFO_TS=") | ftxui::dim,
                ftxui::text(FormatNumber(submsg_counts_[0x09])) | ftxui::dim,
                ftxui::text(" INFO_DST=") | ftxui::dim,
                ftxui::text(FormatNumber(submsg_counts_[0x0E])) | ftxui::dim,
                ftxui::text(" INFO_SRC=") | ftxui::dim,
                ftxui::text(FormatNumber(submsg_counts_[0x0C])) | ftxui::dim,
            }),
        });

        // Topic preview of the currently highlighted node (same filter/order
        // as the detail view, so Enter shows exactly what's previewed here)
        std::vector<const ParticipantInfo*> filtered;
        for (const auto& p : participants_) {
            if (p.domain_id == 0 && p.name.empty()) continue;
            if (p.domain_id == 0 && p.endpoints_count == 0 && !p.is_active) continue;
            filtered.push_back(&p);
        }

        ftxui::Elements preview_lines;
        std::string preview_title = "Topic Preview";
        if (!filtered.empty() && selected_index_ >= 0 &&
            selected_index_ < static_cast<int>(filtered.size())) {
            const auto& sel = *filtered[selected_index_];
            std::string name = sel.name.empty()
                ? "Participant-" + std::to_string(selected_index_ + 1) : sel.name;
            preview_title = "Topic Preview - " + name;
            build_preview(sel, name, preview_lines);
        } else {
            preview_lines.push_back(
                ftxui::text("  Select a node to preview its topics") | ftxui::dim);
        }

        auto preview = ftxui::vbox({
            ftxui::text(preview_title) | ftxui::bold | ftxui::color(ftxui::Color::Cyan),
            ftxui::vbox(std::move(preview_lines)),
        }) | ftxui::border | ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, 12);

        return ftxui::vbox({
            ftxui::text("OnDemand Monitor - Node List") | ftxui::bold | ftxui::center,
            ftxui::text("[↑↓ Select Node] [Enter: View Topics] [q: Quit]") | ftxui::dim | ftxui::center,
            ftxui::separator(),
            stats,
            ftxui::separator(),
            menu->Render() | ftxui::flex | ftxui::border,
            preview,
        });
    });
}

ftxui::Component MonitorUi::BuildDetailView() {
    // Helper: categorize topic by name
    auto categorize_topic = [](const std::string& topic_name) -> int {
        if (topic_name.find("dsf/sys/var/tableDefine") != std::string::npos)
            return 0;  // TableDefine
        if (topic_name.find("dsf/message/commandRequest/subTableRegister") != std::string::npos)
            return 1;  // Registration
        if (topic_name.find("dsf/var/data/transfer/bucket_") != std::string::npos)
            return 2;  // DataTransfer
        return 2;  // Default to DataTransfer for unknown topics
    };

    auto component = ftxui::Renderer([this, categorize_topic] {
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
                ftxui::text("  IP: ") | ftxui::dim,
                ftxui::text(p.src_ip.empty() ? "-" : p.src_ip) | ftxui::color(ftxui::Color::Cyan),
                ftxui::text("  Mcast: ") | ftxui::dim,
                ftxui::text(p.multicast_ip.empty() ? "-" : p.multicast_ip) | ftxui::color(ftxui::Color::Yellow),
                ftxui::text("  Endpoints: ") | ftxui::dim,
                ftxui::text(std::to_string(p.endpoints_count)) | ftxui::color(ftxui::Color::Green),
                ftxui::text("  Status: ") | ftxui::dim,
                ftxui::text(p.is_active ? "Active" : "Inactive") | ftxui::color(p.is_active ? ftxui::Color::Green : ftxui::Color::Red),
            }),
        });

        // Get topics for this participant and filter by tab (from per-tick cache)
        std::vector<MetricsEngine::TopicInfo> topics;
        for (const auto& t : cached_topics_) {
            if (categorize_topic(t.topic_name) == topic_tab_index_) {
                topics.push_back(t);
            }
        }

        // Tab header
        const char* tab_names[] = {"TableDefine", "Registration", "DataTransfer"};
        ftxui::Elements tab_elements;
        for (int i = 0; i < 3; i++) {
            if (i > 0) tab_elements.push_back(ftxui::text(" | "));
            auto tab_text = ftxui::text(tab_names[i]);
            if (i == topic_tab_index_) {
                tab_text = tab_text | ftxui::bold | ftxui::color(ftxui::Color::Cyan) | ftxui::inverted;
            } else {
                tab_text = tab_text | ftxui::dim;
            }
            tab_elements.push_back(tab_text);
        }
        auto tab_header = ftxui::hbox(std::move(tab_elements)) | ftxui::center;

        ftxui::Elements topic_lines;

        if (topics.empty()) {
            topic_lines.push_back(ftxui::text("  No topics in this category...") | ftxui::dim);
        } else {
            // Fixed-width text cells built with snprintf: terminal fonts are
            // monospace, so padded strings give a perfectly aligned grid
            // without relying on ftxui size constraints.
            constexpr int COL_TOPIC = 46;  // fits dsf/message/commandRequest/subTableRegister
            constexpr int COL_ROLE = 5;
            constexpr int COL_MATCH = 14;
            constexpr int COL_STATUS = 9;
            constexpr int COL_COUNT = 8;
            constexpr int COL_BYTES = 9;
            constexpr int COL_LOSS = 7;
            constexpr int COL_NACK = 6;
            constexpr int COL_RET = 7;
            constexpr int COL_RETCNT = 6;
            constexpr int COL_FRAGS = 5;
            constexpr int COL_FREQ = 9;

            char hdr[512];
            snprintf(hdr, sizeof(hdr),
                     "  %-*s %-*s %-*s %-*s %*s %*s %*s %*s %*s %*s %*s %*s",
                     COL_TOPIC - 2, "Topic Name", COL_ROLE, "Role",
                     COL_MATCH, "Match With", COL_STATUS, "Status",
                     COL_COUNT, "Count", COL_BYTES, "Bytes", COL_LOSS, "Loss%",
                     COL_NACK, "NACK", COL_RET, "Ret%", COL_RETCNT, "Ret#",
                     COL_FRAGS, "Frag", COL_FREQ, "Freq");
            topic_lines.push_back(ftxui::text(hdr) | ftxui::bold);
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
                            if (reader != name) matched_names.push_back(reader);
                        }
                    } else {
                        for (const auto& writer : match_info->writer_participants) {
                            if (writer != name) matched_names.push_back(writer);
                        }
                    }
                    for (size_t j = 0; j < matched_names.size(); ++j) {
                        if (j > 0) matched_with += ",";
                        matched_with += matched_names[j];
                    }
                }
                if (matched_with.empty()) matched_with = "--";
                if (matched_with.size() > static_cast<size_t>(COL_MATCH - 1)) {
                    matched_with = matched_with.substr(0, COL_MATCH - 4) + "...";
                }

                bool matched = match_info && match_info->is_matched;

                // Format fixed-width cells
                std::string topic_cut = topic.topic_name.substr(0, COL_TOPIC - 2);
                char prefix[128], status_str[16], count_str[16], bytes_str[16],
                     loss_str[16], nack_str[16], ret_str[16],
                     retrans_cnt_str[16], frags_str[16], freq_str[16];
                snprintf(prefix, sizeof(prefix), "  %-*s %-*s %-*s ",
                         COL_TOPIC - 2, topic_cut.c_str(),
                         COL_ROLE, role.c_str(),
                         COL_MATCH, matched_with.c_str());
                snprintf(status_str, sizeof(status_str), "%-*s", COL_STATUS,
                         matched ? "Matched" : "Unmatched");
                snprintf(count_str, sizeof(count_str), " %*llu", COL_COUNT, (unsigned long long)topic.data_count);
                snprintf(bytes_str, sizeof(bytes_str), " %*s", COL_BYTES, FormatSize(topic.bytes_sent).c_str());
                if (topic.lost_count > 0)
                    snprintf(loss_str, sizeof(loss_str), " %*.1f%%", COL_LOSS - 1, topic.loss_rate * 100.0);
                else
                    snprintf(loss_str, sizeof(loss_str), " %*s", COL_LOSS - 1, "0.0%");
                snprintf(nack_str, sizeof(nack_str), " %*llu", COL_NACK, (unsigned long long)topic.nack_count);
                if (topic.retransmit_count > 0)
                    snprintf(ret_str, sizeof(ret_str), " %*.1f%%", COL_RET - 1, topic.retransmit_rate * 100.0);
                else
                    snprintf(ret_str, sizeof(ret_str), " %*s", COL_RET - 1, "0.0%");
                snprintf(retrans_cnt_str, sizeof(retrans_cnt_str), " %*llu", COL_RETCNT, (unsigned long long)topic.retransmit_count);
                snprintf(frags_str, sizeof(frags_str), " %*llu", COL_FRAGS, (unsigned long long)topic.frag_count);
                // Publish rates are integral by design — round to nearest Hz
                int hz_rounded = static_cast<int>(topic.send_frequency_hz + 0.5);
                if (hz_rounded >= 1)
                    snprintf(freq_str, sizeof(freq_str), " %*dHz", COL_FREQ - 2, hz_rounded);
                else
                    snprintf(freq_str, sizeof(freq_str), " %*s", COL_FREQ - 1, "-");

                topic_lines.push_back(ftxui::hbox({
                    ftxui::text(prefix),
                    matched ? ftxui::text(status_str) | ftxui::color(ftxui::Color::Green)
                            : ftxui::text(status_str) | ftxui::color(ftxui::Color::Red),
                    ftxui::text(count_str),
                    ftxui::text(bytes_str),
                    topic.lost_count > 0 ? ftxui::text(loss_str) | ftxui::color(ftxui::Color::Red)
                                         : ftxui::text(loss_str),
                    topic.nack_count > 0 ? ftxui::text(nack_str) | ftxui::color(ftxui::Color::Red)
                                         : ftxui::text(nack_str),
                    topic.retransmit_count > 0 ? ftxui::text(ret_str) | ftxui::color(ftxui::Color::Yellow)
                                               : ftxui::text(ret_str),
                    topic.retransmit_count > 0 ? ftxui::text(retrans_cnt_str) | ftxui::color(ftxui::Color::Yellow)
                                               : ftxui::text(retrans_cnt_str),
                    ftxui::text(frags_str),
                    ftxui::text(freq_str),
                }));
            }
        }

        auto nav_hint = ftxui::text("[←→ Switch Category] [ESC: Back to Node List]") | ftxui::dim | ftxui::center;

        return ftxui::vbox({
            info,
            ftxui::separator(),
            tab_header,
            ftxui::separator(),
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
        auto pcap_stats = worker_.GetPcapStats();
        auto s = summary_;  // Use cached data

        // Dynamic hint based on current view mode
        std::string mode_hint;
        if (view_mode_ == ViewMode::LIST_VIEW) {
            mode_hint = "[↑↓ Navigate] [Enter: Details] [q: Quit]";
        } else {
            mode_hint = "[ESC: Back] [q: Quit]";
        }

        ftxui::Elements status_elements;
        status_elements.push_back(ftxui::text(" " + mode_hint) | ftxui::dim);
        status_elements.push_back(ftxui::filler());
        status_elements.push_back(ftxui::text("Packets: " + FormatNumber(s.total_data_messages) + "  "));
        // Queue drops (user-space)
        status_elements.push_back(ftxui::text("QDrop: " + std::to_string(dropped) + "  ") |
            (dropped > 0 ? ftxui::color(ftxui::Color::Red) : ftxui::color(ftxui::Color::Default)));
        // Kernel drops (most important for high-traffic scenarios)
        if (pcap_stats.kernel_dropped > 0 || pcap_stats.iface_dropped > 0) {
            status_elements.push_back(ftxui::text("KDrop: " + std::to_string(pcap_stats.kernel_dropped) + "  ") |
                ftxui::color(ftxui::Color::Red));
            if (pcap_stats.iface_dropped > 0) {
                status_elements.push_back(ftxui::text("IFDrop: " + std::to_string(pcap_stats.iface_dropped) + "  ") |
                    ftxui::color(ftxui::Color::Red));
            }
        }

        return ftxui::hbox(std::move(status_elements)) | ftxui::border;
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
        // Left/Right arrow to switch topic tabs in detail view. Refresh the
        // topic cache SYNCHRONOUSLY here: the frame that consumes this event
        // renders immediately, and without a refresh it would show the PREVIOUS
        // tick's cache — every tab switch felt one full frame late.
        if (view_mode_ == ViewMode::DETAIL_VIEW) {
            if (event == ftxui::Event::ArrowLeft) {
                topic_tab_index_ = (topic_tab_index_ + 2) % 3;  // Wrap around
                UpdateTopicCache();
                return true;
            }
            if (event == ftxui::Event::ArrowRight) {
                topic_tab_index_ = (topic_tab_index_ + 1) % 3;  // Wrap around
                UpdateTopicCache();
                return true;
            }
        }
        return false;
    });

    // Data processing thread
    std::thread process_thread([this] {
        // Dynamic batch buffer: starts at 256, grows up to 2048 based on queue depth.
        // At 10k+ variables, packet rate can exceed 10k/s (20 buckets × 50Hz + discovery
        // + heartbeats). The old fixed size of 64 capped throughput at ~6400 pps,
        // causing queue buildup and packet drops under sustained load.
        std::vector<RawPacket> packets(256);
        size_t consecutive_full_batches = 0;

        while (running_.load()) {
            // Adaptive batch sizing: grow buffer when consistently processing full batches
            // (sign of sustained high load), shrink when idle to reduce memory footprint.
            auto stats = worker_.GetCaptureStats();
            uint64_t backlog = stats.queue_packets;

            size_t target_batch = 256;  // baseline
            if (backlog > 5000) {
                target_batch = 2048;  // high load: drain faster
            } else if (backlog > 1000) {
                target_batch = 1024;  // moderate load
            } else if (backlog > 100) {
                target_batch = 512;
            }

            if (packets.size() != target_batch) {
                packets.resize(target_batch);
            }

            size_t count = worker_.PopPackets(packets.data(), packets.size());
            if (count > 0) {
                // Track consecutive full batches to detect sustained load
                if (count == packets.size()) {
                    consecutive_full_batches++;
                } else {
                    consecutive_full_batches = 0;
                }

                // Batch mode: acquire lock once for the entire batch, reducing
                // lock overhead from once-per-packet to once-per-batch.
                engine_.BeginBatch();
                for (size_t i = 0; i < count; ++i) {
                    RtpsMessage msg;
                    bool parsed = RtpsParser::ParseHeader(packets[i].data.data(),
                                                          packets[i].len, msg);
                    if (parsed) {
                        engine_.OnPacketSource(msg.source_guid_prefix,
                                               packets[i].src_ip, packets[i].dst_ip);

                        struct Context {
                            MetricsEngine* engine;
                            uint64_t timestamp;
                            uint16_t src_port;
                            uint16_t dst_port;
                        };
                        Context ctx{&engine_, packets[i].timestamp_us,
                                    packets[i].src_port, packets[i].dst_port};

                        RtpsParser::ParseSubmessages(
                            packets[i].data.data(), packets[i].len,
                            msg.source_guid_prefix, &ctx,
                            [](void* user, DataSubmessage& data) {
                                auto* ctx = static_cast<Context*>(user);
                                data.src_port = ctx->src_port;
                                data.dst_port = ctx->dst_port;
                                ctx->engine->CountSubmsgId(0x15);  // DATA
                                ctx->engine->OnData(data, ctx->timestamp);
                            },
                            [](void* user, const HeartbeatSubmessage& hb) {
                                auto* ctx = static_cast<Context*>(user);
                                ctx->engine->CountSubmsgId(0x07);  // HEARTBEAT
                                ctx->engine->OnHeartbeat(hb, ctx->timestamp);
                            },
                            [](void* user, const AcknackSubmessage& ack) {
                                auto* ctx = static_cast<Context*>(user);
                                ctx->engine->CountSubmsgId(0x06);  // ACKNACK
                                ctx->engine->OnAcknack(ack, ctx->timestamp);
                            },
                            [](void* user, FragSubmessage& frag) {
                                auto* ctx = static_cast<Context*>(user);
                                frag.src_port = ctx->src_port;
                                frag.dst_port = ctx->dst_port;
                                ctx->engine->CountSubmsgId(0x16);  // DATAFRAG
                                ctx->engine->OnFragment(frag, ctx->timestamp);
                            }
                        );
                    }
                }
                engine_.EndBatch();

                // Under sustained high load (consecutive full batches), yield immediately
                // to drain the queue faster instead of sleeping 10ms.
                if (consecutive_full_batches < 3) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            } else {
                // In offline mode, check if file is fully read
                if (worker_.IsOffline() && worker_.IsFinished()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                } else {
                    // Adaptive sleep: shorter when queue has backlog, longer when idle
                    int sleep_ms = (backlog > 100) ? 1 : 10;
                    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
                }
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
