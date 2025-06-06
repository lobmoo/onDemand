/**
 * @file MonitorDataBase.h
 * @brief 
 * @author wwk (1162431386@qq.com)
 * @version 1.0
 * @date 2025-05-21
 * 
 * @copyright Copyright (c) 2025  by  宝信
 * 
 * @par 修改日志:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2025-05-21     <td>1.0     <td>wwk   <td>修改?
 * </table>
 */
#ifndef MONITOR_DATA_BASE_H
#define MONITOR_DATA_BASE_H

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>
#include <log/logger.h>
#include "EntityID.h"

namespace Monitor
{

using json = nlohmann::json;

inline std::string indent(int level)
{
    return std::string(level * 2, ' ');
}

// Duration helper struct
struct Duration {
    int seconds;
    int nanoseconds;

    void parse(const json &j)
    {
        if (j.contains("seconds"))
            seconds = j["seconds"].get<int>();
        else
            seconds = 0;
        if (j.contains("nanoseconds"))
            nanoseconds = j["nanoseconds"].get<int>();
        else
            nanoseconds = 0;
    }
    void print(int indent_level = 0) const
    {
        std::cout << indent(indent_level) << "Duration: " << seconds << "s " << nanoseconds
                  << "ns\n";
    };
};
// Count helper struct
struct Count {
    int count;
    std::string src_time;

    void parse(const json &j)
    {
        if (j.contains("count"))
            count = j["count"].get<int>();
        else
            count = 0;
        if (j.contains("src_time"))
            src_time = j["src_time"].get<std::string>();
        else
            src_time = "N/A";
    }

    void print(int indent_level = 0) const
    {
        std::cout << indent(indent_level) << "Count: " << count << " (src_time: " << src_time
                  << ")\n";
    }
};

// DataReader struct
struct DataReader {
    std::string alias;
    bool alive;
    struct Data {
        std::vector<int> acknack_count;
        Count last_reported_acknack_count;
        Count last_reported_nackfrag_count;
        std::vector<int> nackfrag_count;
        std::vector<int> subscription_throughput;

        void parse(const json &j)
        {
            if (j.contains("acknack_count"))
                acknack_count = j["acknack_count"].get<std::vector<int>>();
            else
                acknack_count = {};
            if (j.contains("last_reported_acknack_count"))
                last_reported_acknack_count.parse(j["last_reported_acknack_count"]);
            else
                last_reported_acknack_count = Count{};
            if (j.contains("last_reported_nackfrag_count"))
                last_reported_nackfrag_count.parse(j["last_reported_nackfrag_count"]);
            else
                last_reported_nackfrag_count = Count{};
            if (j.contains("nackfrag_count"))
                nackfrag_count = j["nackfrag_count"].get<std::vector<int>>();
            else
                nackfrag_count = {};
            if (j.contains("subscription_throughput"))
                subscription_throughput = j["subscription_throughput"].get<std::vector<int>>();
            else
                subscription_throughput = {};
        }

        void print(int indent_level = 0) const
        {
            std::cout << indent(indent_level) << "Acknack Count: ";
            for (auto c : acknack_count)
                std::cout << c << " ";
            std::cout << "\n";
            std::cout << indent(indent_level) << "Last Reported Acknack Count:\n";
            last_reported_acknack_count.print(indent_level + 1);
            std::cout << indent(indent_level) << "Last Reported Nackfrag Count:\n";
            last_reported_nackfrag_count.print(indent_level + 1);
            std::cout << indent(indent_level) << "Nackfrag Count: ";
            for (auto c : nackfrag_count)
                std::cout << c << " ";
            std::cout << "\n";
            std::cout << indent(indent_level) << "Subscription Throughput: ";
            for (auto t : subscription_throughput)
                std::cout << t << " ";
            std::cout << "\n";
        }
    } data;
    std::string guid;
    std::vector<std::string> locators;
    bool metatraffic;
    std::string name;
    std::string participant;
    struct Qos {
        struct DataSharing {
            std::vector<uint64_t> domain_ids;
            std::string kind;
            int max_domains;

            void parse(const json &j)
            {
                if (j.contains("domain_ids"))
                    domain_ids = j["domain_ids"].get<std::vector<uint64_t>>();
                else
                    domain_ids = {};
                if (j.contains("kind"))
                    kind = j["kind"].get<std::string>();
                else
                    kind = "N/A";
                if (j.contains("max_domains"))
                    max_domains = j["max_domains"].get<int>();
                else
                    max_domains = 0;
            }

            void print(int indent_level = 0) const
            {
                std::cout << indent(indent_level) << "Data Sharing: " << kind << "\n";
                std::cout << indent(indent_level) << "Domain IDs: ";
                for (auto id : domain_ids)
                    std::cout << id << " ";
                std::cout << "\n";
                std::cout << indent(indent_level) << "Max Domains: " << max_domains << "\n";
            }
        } data_sharing;

        struct Deadline {
            Duration period;

            void parse(const json &j)
            {
                if (j.contains("period"))
                    period.parse(j["period"]);
                else
                    period = Duration{};
            }

            void print(int indent_level = 0) const
            {
                std::cout << indent(indent_level) << "Deadline: \n";
                period.print(indent_level + 1);
            }
        } deadline;
        struct DestinationOrder {
            std::string kind;

            void parse(const json &j)
            {
                if (j.contains("kind"))
                    kind = j["kind"].get<std::string>();
                else
                    kind = "N/A";
            }

            void print(int indent_level = 0) const
            {
                std::cout << indent(indent_level) << "Destination Order: " << kind << "\n";
            }
        } destination_order;
        struct DisablePositiveAcks {
            Duration duration;
            std::string enabled;

            void parse(const json &j)
            {
                if (j.contains("duration"))
                    duration.parse(j["duration"]);
                else
                    duration = Duration{};
                if (j.contains("enabled"))
                    enabled = j["enabled"].get<std::string>();
                else
                    enabled = "N/A";
            }

            void print(int indent_level = 0) const
            {
                std::cout << indent(indent_level) << "Disable Positive Acks: " << enabled << "\n";
                duration.print(indent_level + 1);
            }
        } disable_positive_acks;
        struct Durability {
            std::string kind;

            void parse(const json &j)
            {
                if (j.contains("kind"))
                    kind = j["kind"].get<std::string>();
                else
                    kind = "N/A";
            }

            void print(int indent_level = 0) const
            {
                std::cout << indent(indent_level) << "Durability: " << kind << "\n";
            }
        } durability;
        struct Liveliness {
            Duration announcement_period;
            std::string kind;
            Duration lease_duration;

            void parse(const json &j)
            {
                if (j.contains("announcement_period"))
                    announcement_period.parse(j["announcement_period"]);
                else
                    announcement_period = Duration{};
                if (j.contains("kind"))
                    kind = j["kind"].get<std::string>();
                else
                    kind = "N/A";
                if (j.contains("lease_duration"))
                    lease_duration.parse(j["lease_duration"]);
                else
                    lease_duration = Duration{};
            }

            void print(int indent_level = 0) const
            {
                std::cout << indent(indent_level) << "Liveliness: " << kind << "\n";
                std::cout << indent(indent_level) << "Announcement Period:\n";
                announcement_period.print(indent_level + 1);
                std::cout << indent(indent_level) << "Lease Duration:\n";
                lease_duration.print(indent_level + 1);
            }
        } liveliness;
        struct Ownership {
            std::string kind;

            void parse(const json &j)
            {
                if (j.contains("kind"))
                    kind = j["kind"].get<std::string>();
                else
                    kind = "N/A";
            }

            void print(int indent_level = 0) const
            {
                std::cout << indent(indent_level) << "Ownership: " << kind << "\n";
            }
        } ownership;
        struct Reliability {
            std::string kind;
            Duration max_blocking_time;

            void parse(const json &j)
            {
                if (j.contains("kind"))
                    kind = j["kind"].get<std::string>();
                else
                    kind = "N/A";
                if (j.contains("max_blocking_time"))
                    max_blocking_time.parse(j["max_blocking_time"]);
                else
                    max_blocking_time = Duration{};
            }

            void print(int indent_level = 0) const
            {
                std::cout << indent(indent_level) << "Reliability: " << kind << "\n";
                std::cout << indent(indent_level) << "Max Blocking Time:\n";
                max_blocking_time.print(indent_level + 1);
            }
        } reliability;
        struct TimeBasedFilter {
            Duration minimum_separation;

            void parse(const json &j)
            {
                if (j.contains("minimum_separation"))
                    minimum_separation.parse(j["minimum_separation"]);
                else
                    minimum_separation = Duration{};
            }

            void print(int indent_level = 0) const
            {
                std::cout << indent(indent_level) << "Time Based Filter:\n";
                minimum_separation.print(indent_level + 1);
            }
        } time_based_filter;
        struct TypeConsistency {
            std::string force_type_validation;
            std::string ignore_member_names;
            std::string ignore_sequence_bounds;
            std::string ignore_string_bounds;
            std::string kind;
            std::string prevent_type_widening;

            void parse(const json &j)
            {
                if (j.contains("force_type_validation"))
                    force_type_validation = j["force_type_validation"].get<std::string>();
                else
                    force_type_validation = "N/A";
                if (j.contains("ignore_member_names"))
                    ignore_member_names = j["ignore_member_names"].get<std::string>();
                else
                    ignore_member_names = "N/A";
                if (j.contains("ignore_sequence_bounds"))
                    ignore_sequence_bounds = j["ignore_sequence_bounds"].get<std::string>();
                else
                    ignore_sequence_bounds = "N/A";
                if (j.contains("ignore_string_bounds"))
                    ignore_string_bounds = j["ignore_string_bounds"].get<std::string>();
                else
                    ignore_string_bounds = "N/A";
                if (j.contains("kind"))
                    kind = j["kind"].get<std::string>();
                else
                    kind = "N/A";
                if (j.contains("prevent_type_widening"))
                    prevent_type_widening = j["prevent_type_widening"].get<std::string>();
                else
                    prevent_type_widening = "N/A";
            }

            void print(int indent_level = 0) const
            {
                std::cout << indent(indent_level) << "Type Consistency: " << kind << "\n";
                std::cout << indent(indent_level)
                          << "Force Type Validation: " << force_type_validation << "\n";
                std::cout << indent(indent_level) << "Ignore Member Names: " << ignore_member_names
                          << "\n";
                std::cout << indent(indent_level)
                          << "Ignore Sequence Bounds: " << ignore_sequence_bounds << "\n";
                std::cout << indent(indent_level)
                          << "Ignore String Bounds: " << ignore_string_bounds << "\n";
                std::cout << indent(indent_level)
                          << "Prevent Type Widening: " << prevent_type_widening << "\n";
            }
        } type_consistency;

        void parse(const json &j)
        {
            if (j.contains("data_sharing"))
                data_sharing.parse(j["data_sharing"]);
            else
                data_sharing = DataSharing{};
            if (j.contains("deadline"))
                deadline.parse(j["deadline"]);
            else
                deadline = Deadline{};
            if (j.contains("destination_order"))
                destination_order.parse(j["destination_order"]);
            else
                destination_order = DestinationOrder{};
            if (j.contains("disable_positive_acks"))
                disable_positive_acks.parse(j["disable_positive_acks"]);
            else
                disable_positive_acks = DisablePositiveAcks{};
            if (j.contains("durability"))
                durability.parse(j["durability"]);
            else
                durability = Durability{};
            if (j.contains("liveliness"))
                liveliness.parse(j["liveliness"]);
            else
                liveliness = Liveliness{};
            if (j.contains("ownership"))
                ownership.parse(j["ownership"]);
            else
                ownership = Ownership{};
            if (j.contains("reliability"))
                reliability.parse(j["reliability"]);
            else
                reliability = Reliability{};
            if (j.contains("time_based_filter"))
                time_based_filter.parse(j["time_based_filter"]);
            else
                time_based_filter = TimeBasedFilter{};
            if (j.contains("type_consistency"))
                type_consistency.parse(j["type_consistency"]);
            else
                type_consistency = TypeConsistency{};
        }

        void print(int indent_level = 0) const
        {
            data_sharing.print(indent_level);
            deadline.print(indent_level);
            destination_order.print(indent_level);
            disable_positive_acks.print(indent_level);
            durability.print(indent_level);
            liveliness.print(indent_level);
            ownership.print(indent_level);
            reliability.print(indent_level);
            time_based_filter.print(indent_level);
            type_consistency.print(indent_level);
        }
    } qos;
    int status;
    std::string topic;
    bool virtual_metatraffic;

    void parse(const json &j)
    {
        if (j.contains("alias"))
            alias = j["alias"].get<std::string>();
        else
            alias = "N/A";
        if (j.contains("alive"))
            alive = j["alive"].get<bool>();
        else
            alive = false;
        if (j.contains("data"))
            data.parse(j["data"]);
        else
            data = Data{};
        if (j.contains("guid"))
            guid = j["guid"].get<std::string>();
        else
            guid = "N/A";
        if (j.contains("locators"))
            locators = j["locators"].get<std::vector<std::string>>();
        else
            locators = {};
        if (j.contains("metatraffic"))
            metatraffic = j["metatraffic"].get<bool>();
        else
            metatraffic = false;
        if (j.contains("name"))
            name = j["name"].get<std::string>();
        else
            name = "N/A";
        if (j.contains("participant"))
            participant = j["participant"].get<std::string>();
        else
            participant = "N/A";
        if (j.contains("qos"))
            qos.parse(j["qos"]);
        else
            qos = Qos{};
        if (j.contains("status"))
            status = j["status"].get<int>();
        else
            status = 0;
        if (j.contains("topic"))
            topic = j["topic"].get<std::string>();
        else
            topic = "N/A";
        if (j.contains("virtual_metatraffic"))
            virtual_metatraffic = j["virtual_metatraffic"].get<bool>();
        else
            virtual_metatraffic = false;
    }

    void print(int indent_level = 0) const
    {
        std::cout << indent(indent_level) << "DataReader:\n";
        std::cout << indent(indent_level + 1) << "Alias: " << alias << "\n";
        std::cout << indent(indent_level + 1) << "Alive: " << (alive ? "Yes" : "No") << "\n";
        std::cout << indent(indent_level + 1) << "GUID: " << guid << "\n";
        std::cout << indent(indent_level + 1) << "Name: " << name << "\n";
        std::cout << indent(indent_level + 1) << "Participant: " << participant << "\n";
        std::cout << indent(indent_level + 1) << "Status: " << status << "\n";
        std::cout << indent(indent_level + 1) << "Topic: " << topic << "\n";
        std::cout << indent(indent_level + 1) << "Metatraffic: " << (metatraffic ? "Yes" : "No")
                  << "\n";
        std::cout << indent(indent_level + 1)
                  << "Virtual Metatraffic: " << (virtual_metatraffic ? "Yes" : "No") << "\n";
        std::cout << indent(indent_level + 1) << "Locators: ";
        for (auto l : locators)
            std::cout << l << " ";
        std::cout << "\n";
        std::cout << indent(indent_level + 1) << "Data:\n";
        data.print(indent_level + 2);
        std::cout << indent(indent_level + 1) << "QoS:\n";
        qos.print(indent_level + 2);
        std::cout << indent(indent_level) << "------------------------\n";
    }
};

// DataWriter struct
struct DataWriter {
    std::string alias;
    bool alive;
    struct Data {
        std::vector<int> data_count;
        std::vector<int> gap_count;
        std::vector<int> heartbeat_count;
        std::map<std::string, std::string> history2history_latency;
        Count last_reported_data_count;
        Count last_reported_gap_count;
        Count last_reported_heartbeat_count;
        Count last_reported_resent_datas;
        std::vector<int> publication_throughput;
        std::vector<int> resent_datas;
        std::map<std::string, std::string> samples_datas;

        void parse(const json &j)
        {
            if (j.contains("data_count"))
                data_count = j["data_count"].get<std::vector<int>>();
            else
                data_count = {};
            if (j.contains("gap_count"))
                gap_count = j["gap_count"].get<std::vector<int>>();
            else
                gap_count = {};
            if (j.contains("heartbeat_count"))
                heartbeat_count = j["heartbeat_count"].get<std::vector<int>>();
            else
                heartbeat_count = {};
            if (j.contains("history2history_latency"))
                history2history_latency =
                    j["history2history_latency"].get<std::map<std::string, std::string>>();
            else
                history2history_latency = {};
            if (j.contains("last_reported_data_count"))
                last_reported_data_count.parse(j["last_reported_data_count"]);
            else
                last_reported_data_count = Count{};
            if (j.contains("last_reported_gap_count"))
                last_reported_gap_count.parse(j["last_reported_gap_count"]);
            else
                last_reported_gap_count = Count{};
            if (j.contains("last_reported_heartbeat_count"))
                last_reported_heartbeat_count.parse(j["last_reported_heartbeat_count"]);
            else
                last_reported_heartbeat_count = Count{};
            if (j.contains("last_reported_resent_datas"))
                last_reported_resent_datas.parse(j["last_reported_resent_datas"]);
            else
                last_reported_resent_datas = Count{};
            if (j.contains("publication_throughput"))
                publication_throughput = j["publication_throughput"].get<std::vector<int>>();
            else
                publication_throughput = {};
            if (j.contains("resent_datas"))
                resent_datas = j["resent_datas"].get<std::vector<int>>();
            else
                resent_datas = {};
            if (j.contains("samples_datas"))
                samples_datas = j["samples_datas"].get<std::map<std::string, std::string>>();
            else
                samples_datas = {};
        }

        void print(int indent_level = 0) const
        {
            std::cout << indent(indent_level) << "Data Count: ";
            for (auto c : data_count)
                std::cout << c << " ";
            std::cout << "\n";
            std::cout << indent(indent_level) << "Gap Count: ";
            for (auto c : gap_count)
                std::cout << c << " ";
            std::cout << "\n";
            std::cout << indent(indent_level) << "Heartbeat Count: ";
            for (auto c : heartbeat_count)
                std::cout << c << " ";
            std::cout << "\n";
            std::cout << indent(indent_level) << "Last Reported Data Count:\n";
            last_reported_data_count.print(indent_level + 1);
            std::cout << indent(indent_level) << "Last Reported Gap Count:\n";
            last_reported_gap_count.print(indent_level + 1);
            std::cout << indent(indent_level) << "Last Reported Heartbeat Count:\n";
            last_reported_heartbeat_count.print(indent_level + 1);
            std::cout << indent(indent_level) << "Last Reported Resent Datas:\n";
            last_reported_resent_datas.print(indent_level + 1);
            std::cout << indent(indent_level) << "Publication Throughput: ";
            for (auto t : publication_throughput)
                std::cout << t << " ";
            std::cout << "\n";
            std::cout << indent(indent_level) << "Resent Datas: ";
            for (auto d : resent_datas)
                std::cout << d << " ";
            std::cout << "\n";
        }
    } data;
    std::string guid;
    std::vector<std::string> locators;
    bool metatraffic;
    std::string name;
    std::string participant;
    struct Qos {
        std::string description;

        void parse(const json &j)
        {
            if (j.contains("description"))
                description = j["description"].get<std::string>();
            else
                description = "N/A";
        }

        void print(int indent_level = 0) const
        {
            std::cout << indent(indent_level) << "QoS Description: " << description << "\n";
        }
    } qos;
    int status;
    std::string topic;
    bool virtual_metatraffic;

    void parse(const json &j)
    {
        if (j.contains("alias"))
            alias = j["alias"].get<std::string>();
        else
            alias = "N/A";
        if (j.contains("alive"))
            alive = j["alive"].get<bool>();
        else
            alive = false;
        if (j.contains("data"))
            data.parse(j["data"]);
        else
            data = Data{};
        if (j.contains("guid"))
            guid = j["guid"].get<std::string>();
        else
            guid = "N/A";
        if (j.contains("locators"))
            locators = j["locators"].get<std::vector<std::string>>();
        else
            locators = {};
        if (j.contains("metatraffic"))
            metatraffic = j["metatraffic"].get<bool>();
        else
            metatraffic = false;
        if (j.contains("name"))
            name = j["name"].get<std::string>();
        else
            name = "N/A";
        if (j.contains("participant"))
            participant = j["participant"].get<std::string>();
        else
            participant = "N/A";
        if (j.contains("qos"))
            qos.parse(j["qos"]);
        else
            qos = Qos{};
        if (j.contains("status"))
            status = j["status"].get<int>();
        else
            status = 0;
        if (j.contains("topic"))
            topic = j["topic"].get<std::string>();
        else
            topic = "N/A";
        if (j.contains("virtual_metatraffic"))
            virtual_metatraffic = j["virtual_metatraffic"].get<bool>();
        else
            virtual_metatraffic = false;
    }

    void print(int indent_level = 0) const
    {
        std::cout << indent(indent_level) << "DataWriter:\n";
        std::cout << indent(indent_level + 1) << "Alias: " << alias << "\n";
        std::cout << indent(indent_level + 1) << "Alive: " << (alive ? "Yes" : "No") << "\n";
        std::cout << indent(indent_level + 1) << "GUID: " << guid << "\n";
        std::cout << indent(indent_level + 1) << "Name: " << name << "\n";
        std::cout << indent(indent_level + 1) << "Participant: " << participant << "\n";
        std::cout << indent(indent_level + 1) << "Status: " << status << "\n";
        std::cout << indent(indent_level + 1) << "Topic: " << topic << "\n";
        std::cout << indent(indent_level + 1) << "Metatraffic: " << (metatraffic ? "Yes" : "No")
                  << "\n";
        std::cout << indent(indent_level + 1)
                  << "Virtual Metatraffic: " << (virtual_metatraffic ? "Yes" : "No") << "\n";
        std::cout << indent(indent_level + 1) << "Locators: ";
        for (auto l : locators)
            std::cout << l << " ";
        std::cout << "\n";
        std::cout << indent(indent_level + 1) << "Data:\n";
        data.print(indent_level + 2);
        std::cout << indent(indent_level + 1) << "QoS:\n";
        qos.print(indent_level + 2);
        std::cout << indent(indent_level) << "------------------------\n";
    }
};

// Domain struct
struct Domain {
    std::string alias;
    bool alive;
    bool metatraffic;
    std::string name;
    std::vector<std::string> participants;
    int status;
    std::vector<std::string> topics;

    void parse(const json &j)
    {
        if (j.contains("alias"))
            alias = j["alias"].get<std::string>();
        else
            alias = "N/A";
        if (j.contains("alive"))
            alive = j["alive"].get<bool>();
        else
            alive = false;
        if (j.contains("metatraffic"))
            metatraffic = j["metatraffic"].get<bool>();
        else
            metatraffic = false;
        if (j.contains("name"))
            name = j["name"].get<std::string>();
        else
            name = "N/A";
        if (j.contains("participants"))
            participants = j["participants"].get<std::vector<std::string>>();
        else
            participants = {};
        if (j.contains("status"))
            status = j["status"].get<int>();
        else
            status = 0;
        if (j.contains("topics"))
            topics = j["topics"].get<std::vector<std::string>>();
        else
            topics = {};
    }

    void print(int indent_level = 0) const
    {
        std::cout << indent(indent_level) << "Domain:\n";
        std::cout << indent(indent_level + 1) << "Alias: " << alias << "\n";
        std::cout << indent(indent_level + 1) << "Alive: " << (alive ? "Yes" : "No") << "\n";
        std::cout << indent(indent_level + 1) << "Name: " << name << "\n";
        std::cout << indent(indent_level + 1) << "Status: " << status << "\n";
        std::cout << indent(indent_level + 1) << "Metatraffic: " << (metatraffic ? "Yes" : "No")
                  << "\n";
        std::cout << indent(indent_level + 1) << "Participants: ";
        for (auto p : participants)
            std::cout << p << " ";
        std::cout << "\n";
        std::cout << indent(indent_level + 1) << "Topics: ";
        for (auto t : topics)
            std::cout << t << " ";
        std::cout << "\n";
        std::cout << indent(indent_level) << "------------------------\n";
    }
};

// Host struct
struct Host {
    std::string alias;
    bool alive;
    bool metatraffic;
    std::string name;
    int status;
    std::vector<std::string> users;

    void parse(const json &j)
    {
        if (j.contains("alias"))
            alias = j["alias"].get<std::string>();
        else
            alias = "N/A";
        if (j.contains("alive"))
            alive = j["alive"].get<bool>();
        else
            alive = false;
        if (j.contains("metatraffic"))
            metatraffic = j["metatraffic"].get<bool>();
        else
            metatraffic = false;
        if (j.contains("name"))
            name = j["name"].get<std::string>();
        else
            name = "N/A";
        if (j.contains("status"))
            status = j["status"].get<int>();
        else
            status = 0;
        if (j.contains("users"))
            users = j["users"].get<std::vector<std::string>>();
        else
            users = {};
    }

    void print(int indent_level = 0) const
    {
        std::cout << indent(indent_level) << "Host:\n";
        std::cout << indent(indent_level + 1) << "Alias: " << alias << "\n";
        std::cout << indent(indent_level + 1) << "Alive: " << (alive ? "Yes" : "No") << "\n";
        std::cout << indent(indent_level + 1) << "Name: " << name << "\n";
        std::cout << indent(indent_level + 1) << "Status: " << status << "\n";
        std::cout << indent(indent_level + 1) << "Metatraffic: " << (metatraffic ? "Yes" : "No")
                  << "\n";
        std::cout << indent(indent_level + 1) << "Users: ";
        for (auto u : users)
            std::cout << u << " ";
        std::cout << "\n";
        std::cout << indent(indent_level) << "------------------------\n";
    }
};

// Locator struct
struct Locator {
    std::string alias;
    bool alive;
    std::vector<std::string> datareaders;
    std::vector<std::string> datawriters;
    bool metatraffic;
    std::string name;
    int status;

    void parse(const json &j)
    {
        if (j.contains("alias"))
            alias = j["alias"].get<std::string>();
        else
            alias = "N/A";
        if (j.contains("alive"))
            alive = j["alive"].get<bool>();
        else
            alive = false;
        if (j.contains("datareaders"))
            datareaders = j["datareaders"].get<std::vector<std::string>>();
        else
            datareaders = {};
        if (j.contains("datawriters"))
            datawriters = j["datawriters"].get<std::vector<std::string>>();
        else
            datawriters = {};
        if (j.contains("metatraffic"))
            metatraffic = j["metatraffic"].get<bool>();
        else
            metatraffic = false;
        if (j.contains("name"))
            name = j["name"].get<std::string>();
        else
            name = "N/A";
        if (j.contains("status"))
            status = j["status"].get<int>();
        else
            status = 0;
    }

    void print(int indent_level = 0) const
    {
        std::cout << indent(indent_level) << "Locator:\n";
        std::cout << indent(indent_level + 1) << "Alias: " << alias << "\n";
        std::cout << indent(indent_level + 1) << "Alive: " << (alive ? "Yes" : "No") << "\n";
        std::cout << indent(indent_level + 1) << "Name: " << name << "\n";
        std::cout << indent(indent_level + 1) << "Status: " << status << "\n";
        std::cout << indent(indent_level + 1) << "Metatraffic: " << (metatraffic ? "Yes" : "No")
                  << "\n";
        std::cout << indent(indent_level + 1) << "DataReaders: ";
        for (auto dr : datareaders)
            std::cout << dr << " ";
        std::cout << "\n";
        std::cout << indent(indent_level + 1) << "DataWriters: ";
        for (auto dw : datawriters)
            std::cout << dw << " ";
        std::cout << "\n";
        std::cout << indent(indent_level) << "------------------------\n";
    }
};

// Participant struct
struct Participant {
    std::string alias;
    bool alive;
    struct Data {
        Count last_reported_edp_packets;
        Count last_reported_pdp_packets;
        std::map<std::string, std::string> last_reported_rtps_bytes_lost,
            last_reported_rtps_bytes_sent, last_reported_rtps_packets_lost,
            last_reported_rtps_packets_sent;
        std::map<std::string, std::string> network_latency_per_locator;
        std::vector<int> edp_packets, pdp_packets;
        std::map<std::string, std::string> rtps_bytes_lost, rtps_bytes_sent;
        std::map<std::string, std::string> rtps_packets_lost, rtps_packets_sent;

        void parse(const json &j)
        {
            if (j.contains("last_reported_edp_packets"))
                last_reported_edp_packets.parse(j["last_reported_edp_packets"]);
            else
                last_reported_edp_packets = Count{};
            if (j.contains("last_reported_pdp_packets"))
                last_reported_pdp_packets.parse(j["last_reported_pdp_packets"]);
            else
                last_reported_pdp_packets = Count{};
            if (j.contains("last_reported_rtps_bytes_lost"))
                last_reported_rtps_bytes_lost =
                    j["last_reported_rtps_bytes_lost"].get<std::map<std::string, std::string>>();
            else
                last_reported_rtps_bytes_lost = {};
            if (j.contains("last_reported_rtps_bytes_sent"))
                last_reported_rtps_bytes_sent =
                    j["last_reported_rtps_bytes_sent"].get<std::map<std::string, std::string>>();
            else
                last_reported_rtps_bytes_sent = {};
            if (j.contains("last_reported_rtps_packets_lost"))
                last_reported_rtps_packets_lost =
                    j["last_reported_rtps_packets_lost"].get<std::map<std::string, std::string>>();
            else
                last_reported_rtps_packets_lost = {};
            if (j.contains("last_reported_rtps_packets_sent"))
                last_reported_rtps_packets_sent =
                    j["last_reported_rtps_packets_sent"].get<std::map<std::string, std::string>>();
            else
                last_reported_rtps_packets_sent = {};
            if (j.contains("network_latency_per_locator"))
                network_latency_per_locator =
                    j["network_latency_per_locator"].get<std::map<std::string, std::string>>();
            else
                network_latency_per_locator = {};
            if (j.contains("edp_packets"))
                edp_packets = j["edp_packets"].get<std::vector<int>>();
            else
                edp_packets = {};
            if (j.contains("pdp_packets"))
                pdp_packets = j["pdp_packets"].get<std::vector<int>>();
            else
                pdp_packets = {};
            if (j.contains("rtps_bytes_lost"))
                rtps_bytes_lost = j["rtps_bytes_lost"].get<std::map<std::string, std::string>>();
            else
                rtps_bytes_lost = {};
            if (j.contains("rtps_bytes_sent"))
                rtps_bytes_sent = j["rtps_bytes_sent"].get<std::map<std::string, std::string>>();
            else
                rtps_bytes_sent = {};
            if (j.contains("rtps_packets_lost"))
                rtps_packets_lost =
                    j["rtps_packets_lost"].get<std::map<std::string, std::string>>();
            else
                rtps_packets_lost = {};
            if (j.contains("rtps_packets_sent"))
                rtps_packets_sent =
                    j["rtps_packets_sent"].get<std::map<std::string, std::string>>();
            else
                rtps_packets_sent = {};
        }

        void print(int indent_level = 0) const
        {
            std::cout << indent(indent_level) << "Last Reported EDP Packets:\n";
            last_reported_edp_packets.print(indent_level + 1);
            std::cout << indent(indent_level) << "Last Reported PDP Packets:\n";
            last_reported_pdp_packets.print(indent_level + 1);
            std::cout << indent(indent_level) << "Network Latency per Locator:\n";
            for (auto nl : network_latency_per_locator) {
                std::cout << indent(indent_level + 1) << "Locator: " << nl.first
                          << " Latency: " << nl.second << "\n";
            }
        }
    } data;
    std::vector<std::string> datareaders;
    std::vector<std::string> datawriters;
    std::string domain;
    std::string guid;
    bool metatraffic;
    std::string name;
    std::string process;
    struct Qos {
        Duration lease_duration;
        std::vector<std::map<std::string, std::string>> properties;
        std::string user_data;
        std::vector<int> vendor_id;

        void parse(const json &j)
        {
            if (j.contains("lease_duration"))
                lease_duration.parse(j["lease_duration"]);
            else
                lease_duration = Duration{};
            if (j.contains("user_data"))
                user_data = j["user_data"].get<std::string>();
            else
                user_data = "N/A";
            if (j.contains("vendor_id"))
                vendor_id = j["vendor_id"].get<std::vector<int>>();
            else
                vendor_id = {};
            if (j.contains("properties")) {
                for (auto &prop : j["properties"]) {
                    properties.push_back(prop.get<std::map<std::string, std::string>>());
                }
            } else {
                properties = {};
            }
        }

        void print(int indent_level = 0) const
        {
            std::cout << indent(indent_level) << "QoS:\n";
            std::cout << indent(indent_level + 1) << "Lease Duration:\n";
            lease_duration.print(indent_level + 2);
            std::cout << indent(indent_level + 1) << "User Data: " << user_data << "\n";
            std::cout << indent(indent_level + 1) << "Vendor ID: ";
            for (auto vi : vendor_id)
                std::cout << vi << " ";
            std::cout << "\n";
            std::cout << indent(indent_level + 1) << "Properties:\n";
            for (auto &prop : properties) {
                for (auto &p : prop) {
                    std::cout << indent(indent_level + 2) << p.first << ": " << p.second << "\n";
                }
            }
            std::cout << "\n";
        }
    } qos;
    int status;

    void parse(const json &j)
    {
        if (j.contains("alias"))
            alias = j["alias"].get<std::string>();
        else
            alias = "N/A";
        if (j.contains("alive"))
            alive = j["alive"].get<bool>();
        else
            alive = false;
        if (j.contains("data"))
            data.parse(j["data"]);
        else
            data = Data{};
        if (j.contains("datareaders"))
            datareaders = j["datareaders"].get<std::vector<std::string>>();
        else
            datareaders = {};
        if (j.contains("datawriters"))
            datawriters = j["datawriters"].get<std::vector<std::string>>();
        else
            datawriters = {};
        if (j.contains("domain"))
            domain = j["domain"].get<std::string>();
        else
            domain = "N/A";
        if (j.contains("guid"))
            guid = j["guid"].get<std::string>();
        else
            guid = "N/A";
        if (j.contains("metatraffic"))
            metatraffic = j["metatraffic"].get<bool>();
        else
            metatraffic = false;
        if (j.contains("name"))
            name = j["name"].get<std::string>();
        else
            name = "N/A";
        if (j.contains("process"))
            process = j["process"].get<std::string>();
        else
            process = "N/A";
        if (j.contains("qos"))
            qos.parse(j["qos"]);
        else
            qos = Qos{};
        if (j.contains("status"))
            status = j["status"].get<int>();
        else
            status = 0;
    }

    void print(int indent_level = 0) const
    {
        std::cout << indent(indent_level) << "Participant:\n";
        std::cout << indent(indent_level + 1) << "Alias: " << alias << "\n";
        std::cout << indent(indent_level + 1) << "Alive: " << (alive ? "Yes" : "No") << "\n";
        std::cout << indent(indent_level + 1) << "GUID: " << guid << "\n";
        std::cout << indent(indent_level + 1) << "Name: " << name << "\n";
        std::cout << indent(indent_level + 1) << "Domain: " << domain << "\n";
        std::cout << indent(indent_level + 1) << "Process: " << process << "\n";
        std::cout << indent(indent_level + 1) << "Status: " << status << "\n";
        std::cout << indent(indent_level + 1) << "Metatraffic: " << (metatraffic ? "Yes" : "No")
                  << "\n";
        std::cout << indent(indent_level + 1) << "DataReaders: ";
        for (auto dr : datareaders)
            std::cout << dr << " ";
        std::cout << "\n";
        std::cout << indent(indent_level + 1) << "DataWriters: ";
        for (auto dw : datawriters)
            std::cout << dw << " ";
        std::cout << "\n";
        std::cout << indent(indent_level + 1) << "Data:\n";
        data.print(indent_level + 2);
        std::cout << indent(indent_level + 1) << "QoS:\n";
        qos.print(indent_level + 2);
        std::cout << indent(indent_level) << "------------------------\n";
    }
};

// Process struct
struct Process {
    std::string alias;
    bool alive;
    bool metatraffic;
    std::string name;
    std::vector<std::string> participants;
    std::string pid;
    int status;
    std::string user;

    void parse(const json &j)
    {
        if (j.contains("alias"))
            alias = j["alias"].get<std::string>();
        else
            alias = "N/A";
        if (j.contains("alive"))
            alive = j["alive"].get<bool>();
        else
            alive = false;
        if (j.contains("metatraffic"))
            metatraffic = j["metatraffic"].get<bool>();
        else
            metatraffic = false;
        if (j.contains("name"))
            name = j["name"].get<std::string>();
        else
            name = "N/A";
        if (j.contains("participants"))
            participants = j["participants"].get<std::vector<std::string>>();
        else
            participants = {};
        if (j.contains("pid"))
            pid = j["pid"].get<std::string>();
        else
            pid = "N/A";
        if (j.contains("status"))
            status = j["status"].get<int>();
        else
            status = 0;
        if (j.contains("user"))
            user = j["user"].get<std::string>();
        else
            user = "N/A";
    }

    void print(int indent_level = 0) const
    {
        std::cout << indent(indent_level) << "Process:\n";
        std::cout << indent(indent_level + 1) << "Alias: " << alias << "\n";
        std::cout << indent(indent_level + 1) << "Alive: " << (alive ? "Yes" : "No") << "\n";
        std::cout << indent(indent_level + 1) << "Name: " << name << "\n";
        std::cout << indent(indent_level + 1) << "PID: " << pid << "\n";
        std::cout << indent(indent_level + 1) << "User: " << user << "\n";
        std::cout << indent(indent_level + 1) << "Status: " << status << "\n";
        std::cout << indent(indent_level + 1) << "Metatraffic: " << (metatraffic ? "Yes" : "No")
                  << "\n";
        std::cout << indent(indent_level + 1) << "Participants: ";
        for (auto p : participants)
            std::cout << p << " ";
        std::cout << "\n";
        std::cout << indent(indent_level) << "------------------------\n";
    }
};

struct Topic {
    std::string alias;
    bool alive;
    std::string data_type;
    std::vector<std::string> datareaders;
    std::vector<std::string> datawriters;
    std::string domain;
    bool metatraffic;
    std::string name;
    int status;

    void parse(const json &j)
    {
        if (j.contains("alias"))
            alias = j["alias"].get<std::string>();
        else
            alias = "N/A";
        if (j.contains("alive"))
            alive = j["alive"].get<bool>();
        else
            alive = false;
        if (j.contains("data_type"))
            data_type = j["data_type"].get<std::string>();
        else
            data_type = "N/A";
        if (j.contains("datareaders"))
            datareaders = j["datareaders"].get<std::vector<std::string>>();
        else
            datareaders = {};
        if (j.contains("datawriters"))
            datawriters = j["datawriters"].get<std::vector<std::string>>();
        else
            datawriters = {};
        if (j.contains("domain"))
            domain = j["domain"].get<std::string>();
        else
            domain = "N/A";
        if (j.contains("metatraffic"))
            metatraffic = j["metatraffic"].get<bool>();
        else
            metatraffic = false;
        if (j.contains("name"))
            name = j["name"].get<std::string>();
        else
            name = "N/A";
        if (j.contains("status"))
            status = j["status"].get<int>();
        else
            status = 0;
    }

    void print(int indent_level = 0) const
    {
        std::cout << indent(indent_level) << "Topic:\n";
        std::cout << indent(indent_level + 1) << "Alias: " << alias << "\n";
        std::cout << indent(indent_level + 1) << "Alive: " << (alive ? "Yes" : "No") << "\n";
        std::cout << indent(indent_level + 1) << "Name: " << name << "\n";
        std::cout << indent(indent_level + 1) << "Data Type: " << data_type << "\n";
        std::cout << indent(indent_level + 1) << "Domain: " << domain << "\n";
        std::cout << indent(indent_level + 1) << "Status: " << status << "\n";
        std::cout << indent(indent_level + 1) << "Metatraffic: " << (metatraffic ? "Yes" : "No")
                  << "\n";
        std::cout << indent(indent_level + 1) << "DataReaders: ";
        for (auto dr : datareaders)
            std::cout << dr << " ";
        std::cout << "\n";
        std::cout << indent(indent_level + 1) << "DataWriters: ";
        for (auto dw : datawriters)
            std::cout << dw << " ";
        std::cout << "\n";
        std::cout << indent(indent_level) << "------------------------\n";
    }
};

struct User {
    std::string alias;
    bool alive;
    std::string host;
    bool metatraffic;
    std::string name;
    std::vector<std::string> processes;
    int status;

    void parse(const json &j)
    {
        if (j.contains("alias"))
            alias = j["alias"].get<std::string>();
        else
            alias = "N/A";
        if (j.contains("alive"))
            alive = j["alive"].get<bool>();
        else
            alive = false;
        if (j.contains("host"))
            host = j["host"].get<std::string>();
        else
            host = "N/A";
        if (j.contains("metatraffic"))
            metatraffic = j["metatraffic"].get<bool>();
        else
            metatraffic = false;
        if (j.contains("name"))
            name = j["name"].get<std::string>();
        else
            name = "N/A";
        if (j.contains("processes"))
            processes = j["processes"].get<std::vector<std::string>>();
        else
            processes = {};
        if (j.contains("status"))
            status = j["status"].get<int>();
        else
            status = 0;
    }

    void print(int indent_level = 0) const
    {
        std::cout << indent(indent_level) << "User:\n";
        std::cout << indent(indent_level + 1) << "Alias: " << alias << "\n";
        std::cout << indent(indent_level + 1) << "Alive: " << (alive ? "Yes" : "No") << "\n";
        std::cout << indent(indent_level + 1) << "Name: " << name << "\n";
        std::cout << indent(indent_level + 1) << "Host: " << host << "\n";
        std::cout << indent(indent_level + 1) << "Status: " << status << "\n";
        std::cout << indent(indent_level + 1) << "Metatraffic: " << (metatraffic ? "Yes" : "No")
                  << "\n";
        std::cout << indent(indent_level + 1) << "Processes: ";
        for (auto p : processes)
            std::cout << p << " ";
        std::cout << "\n";
        std::cout << indent(indent_level) << "------------------------\n";
    }
};


class MonitorDataBase
{

public:
    ~MonitorDataBase() {};
    static MonitorDataBase &getInstance()
    {
        static MonitorDataBase instance;
        return instance;
    }

private:
    MonitorDataBase() : isMetatraffic_(false) {}
    MonitorDataBase(const MonitorDataBase &) = delete;
    MonitorDataBase &operator=(const MonitorDataBase &) = delete;
    bool isMetatraffic_;
    mutable std::mutex mutex_;

private:
    std::unordered_map<EntityID, DataReader> datareaders;
    std::unordered_map<EntityID, DataWriter> datawriters;
    std::unordered_map<EntityID, Domain> domains;
    std::unordered_map<EntityID, Host> hosts;
    std::unordered_map<EntityID, Locator> locators;
    std::unordered_map<EntityID, Participant> participants;
    std::unordered_map<EntityID, Process> processes;
    std::unordered_map<EntityID, Topic> topics;
    std::unordered_map<EntityID, User> users;
    std::unordered_map<std::string, EntityID> Topic2IDMap;
    std::string version;

public:
    const DataReader &getDataReader(const EntityID &id) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = datareaders.find(id);
        if (it == datareaders.end()) {
            throw std::out_of_range("DataReader not found: " + id.toString());
        }
        return it->second;
    }

    const DataWriter &getDataWriter(const EntityID &id) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = datawriters.find(id);
        if (it == datawriters.end()) {
            throw std::out_of_range("DataWriter not found: " + id.toString());
        }
        return it->second;
    }
    const Domain &getDomain(const EntityID &id) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = domains.find(id);
        if (it == domains.end()) {
            throw std::out_of_range("Domain not found: " + id.toString());
        }
        return it->second;
    }

    const Host &getHost(const EntityID &id) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = hosts.find(id);
        if (it == hosts.end()) {

            throw std::out_of_range("Host not found: " + id.toString());
        }
        return it->second;
    }

    const Locator &getLocator(const EntityID &id) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = locators.find(id);
        if (it == locators.end()) {
            throw std::out_of_range("Locator not found: " + id.toString());
        }
        return it->second;
    }

    const Participant &getParticipant(const EntityID &id) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = participants.find(id);
        if (it == participants.end()) {
            throw std::out_of_range("Participant not found: " + id.toString());
        }
        return it->second;
    }

    const Process &getProcess(const EntityID &id) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = processes.find(id);
        if (it == processes.end()) {
            throw std::out_of_range("Process not found: " + id.toString());
        }
        return it->second;
    }

    const Topic &getTopic(const EntityID &id) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = topics.find(id);
        if (it == topics.end()) {
            throw std::out_of_range("Topic not found: " + id.toString());
        }
        return it->second;
    }

    const User &getUser(const EntityID &id) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = users.find(id);
        if (it == users.end()) {
            throw std::out_of_range("User not found: " + id.toString());
        }
        return it->second;
    }

    EntityID getTopicID(const std::string &topicName) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = Topic2IDMap.find(topicName);
        if (it == Topic2IDMap.end()) {
            throw std::out_of_range("Topic ID not found for topic: " + topicName);
        }
        return it->second;
    }
    const std::vector<std::string> getAllTopicsName() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> allTopics;
        for (const auto &[id, topic] : topics) {
            allTopics.push_back(topic.name);
        }
        return allTopics;
    }
    void setMetatraffic(bool isMetatraffic)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        isMetatraffic_ = isMetatraffic;
    }

    template <typename T>
    void parseMap(const nlohmann::json &j, const std::string &key, std::unordered_map<EntityID, T> &map)
    {
        if (j.contains(key)) {
            for (const auto &[k, v] : j[key].items()) {
                T obj;
                EntityID id({key, k});
                //LOG(debug) << "Parsing " << id;
                obj.parse(v);
                if (isMetatraffic_ || !obj.metatraffic) {
                    map[id] = obj;
                    if("topics" == key) {
                        Topic2IDMap[obj.name] = id;
                    }
                }
            }
        }
    }

    void parse(const json &j)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        parseMap(j, "datareaders", datareaders);
        parseMap(j, "datawriters", datawriters);
        parseMap(j, "domains", domains);
        parseMap(j, "hosts", hosts);
        parseMap(j, "locators", locators);
        parseMap(j, "participants", participants);
        parseMap(j, "processes", processes);
        parseMap(j, "topics", topics);
        parseMap(j, "users", users);
        if (j.contains("version"))
            version = j["version"].get<std::string>();
        else
            version = "N/A";
    }

    template <typename T>
    void printMap(T objects, int indent_level = 0) const
    {
        for (const auto &[key, obj] : objects) {
            std::cout << indent(indent_level) << key << ":\n";
            obj.print(indent_level + 1);
        }
    }

    void print() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "Monitor Data - Version " << version << "\n\n";
        printMap(domains);
        printMap(hosts);
        printMap(users);
        printMap(processes);
        printMap(topics);
        printMap(participants);
        printMap(locators);
        printMap(datareaders);
        printMap(datawriters);
        LOG(info) << "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n";
    }
};
} // namespace Monitor
#endif // MONITOR_DATA_BASE_H