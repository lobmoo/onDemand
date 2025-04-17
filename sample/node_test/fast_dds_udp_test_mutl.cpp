#include <cstring>
#include <iostream>
#include <memory>
#include <thread>
#include <gtest/gtest.h>

#include "TestMessage_1k/TestMessage.hpp"
#include "TestMessage_1k/TestMessagePubSubTypes.hpp"

#include "TestMessage_100k/TestMessage100k.hpp"
#include "TestMessage_100k/TestMessage100kPubSubTypes.hpp"

#include "TestMessage_1M/TestMessage1M.hpp"
#include "TestMessage_1M/TestMessage1MPubSubTypes.hpp"

#include "TestMessage_5M/TestMessage5M.hpp"
#include "TestMessage_5M/TestMessage5MPubSubTypes.hpp"

#include "TestMessage_100M/TestMessage100M.hpp"
#include "TestMessage_100M/TestMessage100MPubSubTypes.hpp"

#include "fastdds_wrapper/DataNode.h"
#include "fastdds_wrapper/DDSParticipantListener.h"
#include "log/logger.h"
using namespace std;

static int32_t get_hostname()
{
    char hostname[1024];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        std::hash<std::string> hasher;
        size_t hash_value = hasher(hostname);
        return static_cast<int32_t>(hash_value);
    } else {
        return -1; // 错误处理
    }
}

static std::mutex delays_mutex_512;
static std::mutex delays_mutex_51200;
static std::mutex delays_mutex_524288;
static std::mutex delays_mutex_2621440;


class UDPTestFixtureMult : public ::testing::Test
{

private:
    std::vector<std::string> topic_names = {
        "topic1",   "topic2",   "topic3",   "topic4",   "topic5",   "topic6",   "topic7",
        "topic8",   "topic9",   "topic10",  "topic11",  "topic12",  "topic13",  "topic14",
        "topic15",  "topic16",  "topic17",  "topic18",  "topic19",  "topic20",  "topic21",
        "topic22",  "topic23",  "topic24",  "topic25",  "topic26",  "topic27",  "topic28",
        "topic29",  "topic30",  "topic31",  "topic32",  "topic33",  "topic34",  "topic35",
        "topic36",  "topic37",  "topic38",  "topic39",  "topic40",  "topic41",  "topic42",
        "topic43",  "topic44",  "topic45",  "topic46",  "topic47",  "topic48",  "topic49",
        "topic50",  "topic51",  "topic52",  "topic53",  "topic54",  "topic55",  "topic56",
        "topic57",  "topic58",  "topic59",  "topic60",  "topic61",  "topic62",  "topic63",
        "topic64",  "topic65",  "topic66",  "topic67",  "topic68",  "topic69",  "topic70",
        "topic71",  "topic72",  "topic73",  "topic74",  "topic75",  "topic76",  "topic77",
        "topic78",  "topic79",  "topic80",  "topic81",  "topic82",  "topic83",  "topic84",
        "topic85",  "topic86",  "topic87",  "topic88",  "topic89",  "topic90",  "topic91",
        "topic92",  "topic93",  "topic94",  "topic95",  "topic96",  "topic97",  "topic98",
        "topic99",  "topic100", "topic101", "topic102", "topic103", "topic104", "topic105",
        "topic106", "topic107", "topic108", "topic109", "topic110", "topic111", "topic112",
        "topic113", "topic114", "topic115", "topic116", "topic117", "topic118", "topic119",
        "topic120", "topic121", "topic122", "topic123", "topic124", "topic125", "topic126",
        "topic127", "topic128", "topic129", "topic130", "topic131", "topic132", "topic133",
        "topic134", "topic135", "topic136", "topic137", "topic138", "topic139", "topic140",
        "topic141", "topic142", "topic143", "topic144", "topic145", "topic146", "topic147",
        "topic148", "topic149", "topic150", "topic151", "topic152", "topic153", "topic154",
        "topic155", "topic156", "topic157", "topic158", "topic159", "topic160", "topic161",
        "topic162", "topic163", "topic164", "topic165", "topic166", "topic167", "topic168",
        "topic169", "topic170", "topic171", "topic172", "topic173", "topic174", "topic175",
        "topic176", "topic177", "topic178", "topic179", "topic180", "topic181", "topic182",
        "topic183", "topic184", "topic185", "topic186", "topic187", "topic188", "topic189",
        "topic190", "topic191", "topic192", "topic193", "topic194", "topic195", "topic196",
        "topic197", "topic198", "topic199", "topic200", "topic201", "topic202", "topic203",
        "topic204", "topic205", "topic206", "topic207", "topic208", "topic209", "topic210",
        "topic211", "topic212", "topic213", "topic214", "topic215", "topic216", "topic217",
        "topic218", "topic219", "topic220", "topic221", "topic222", "topic223", "topic224",
        "topic225", "topic226", "topic227", "topic228", "topic229", "topic230", "topic231",
        "topic232", "topic233", "topic234", "topic235", "topic236", "topic237", "topic238",
        "topic239", "topic240", "topic241", "topic242", "topic243", "topic244", "topic245",
        "topic246", "topic247", "topic248", "topic249", "topic250", "topic251", "topic252",
        "topic253", "topic254", "topic255", "topic256", "topic257", "topic258", "topic259",
        "topic260", "topic261", "topic262", "topic263", "topic264", "topic265", "topic266",
        "topic267", "topic268", "topic269", "topic270", "topic271", "topic272", "topic273",
        "topic274", "topic275", "topic276", "topic277", "topic278", "topic279", "topic280",
        "topic281", "topic282", "topic283", "topic284", "topic285", "topic286", "topic287",
        "topic288", "topic289", "topic290", "topic291", "topic292", "topic293", "topic294",
        "topic295", "topic296", "topic297", "topic298", "topic299", "topic300", "topic301",
        "topic302", "topic303", "topic304", "topic305", "topic306", "topic307", "topic308",
        "topic309", "topic310", "topic311", "topic312", "topic313", "topic314", "topic315",
        "topic316", "topic317", "topic318", "topic319", "topic320", "topic321", "topic322",
        "topic323", "topic324", "topic325", "topic326", "topic327", "topic328", "topic329",
        "topic330", "topic331", "topic332", "topic333", "topic334", "topic335", "topic336",
        "topic337", "topic338", "topic339", "topic340", "topic341", "topic342", "topic343",
        "topic344", "topic345", "topic346", "topic347", "topic348", "topic349", "topic350",
        "topic351", "topic352", "topic353", "topic354", "topic355", "topic356", "topic357",
        "topic358", "topic359", "topic360", "topic361", "topic362", "topic363", "topic364",
        "topic365", "topic366", "topic367", "topic368", "topic369", "topic370", "topic371",
        "topic372", "topic373", "topic374", "topic375", "topic376", "topic377", "topic378",
        "topic379", "topic380", "topic381", "topic382", "topic383", "topic384", "topic385",
        "topic386", "topic387", "topic388", "topic389", "topic390", "topic391", "topic392",
        "topic393", "topic394", "topic395", "topic396", "topic397", "topic398", "topic399",
        "topic400", "topic401", "topic402", "topic403", "topic404", "topic405", "topic406",
        "topic407", "topic408", "topic409", "topic410", "topic411", "topic412", "topic413",
        "topic414", "topic415", "topic416", "topic417", "topic418", "topic419", "topic420",
        "topic421", "topic422", "topic423", "topic424", "topic425", "topic426", "topic427",
        "topic428", "topic429", "topic430", "topic431", "topic432", "topic433", "topic434",
        "topic435", "topic436", "topic437", "topic438", "topic439", "topic440", "topic441",
        "topic442", "topic443", "topic444", "topic445", "topic446", "topic447", "topic448",
        "topic449", "topic450", "topic451", "topic452", "topic453", "topic454", "topic455",
        "topic456", "topic457", "topic458", "topic459", "topic460", "topic461", "topic462",
        "topic463", "topic464", "topic465", "topic466", "topic467", "topic468", "topic469",
        "topic470", "topic471", "topic472", "topic473", "topic474", "topic475", "topic476",
        "topic477", "topic478", "topic479", "topic480", "topic481", "topic482", "topic483",
        "topic484", "topic485", "topic486", "topic487", "topic488", "topic489", "topic490",
        "topic491", "topic492", "topic493", "topic494", "topic495", "topic496", "topic497",
        "topic498", "topic499", "topic500", "topic501", "topic502", "topic503", "topic504",
        "topic505", "topic506", "topic507", "topic508", "topic509", "topic510", "topic511",
        "topic512"};

protected:
    void SetUp() override
    {

        delays_512 = std::make_shared<std::unordered_map<int32_t, std::vector<uint64_t>>>();
        delays_51200 = std::make_shared<std::unordered_map<int32_t, std::vector<uint64_t>>>();
        delays_524288 = std::make_shared<std::unordered_map<int32_t, std::vector<uint64_t>>>();
        delays_2621440 = std::make_shared<std::unordered_map<int32_t, std::vector<uint64_t>>>();

        
        senderNode = std::make_unique<DataNode>(0, "sender_node");
        receiverNode = std::make_unique<DataNode>(0, "receiver_node");
        for (auto topic_name : topic_names) {
            senderNode->registerTopicType<Message_512PubSubType>(topic_name);
            receiverNode->registerTopicType<Message_512PubSubType>(topic_name);

            senderNode->registerTopicType<Message_51200PubSubType>(topic_name);
            receiverNode->registerTopicType<Message_51200PubSubType>(topic_name);

            senderNode->registerTopicType<Message_524288PubSubType>(topic_name);
            receiverNode->registerTopicType<Message_524288PubSubType>(topic_name);

            senderNode->registerTopicType<Message_2621440PubSubType>(topic_name);
            receiverNode->registerTopicType<Message_2621440PubSubType>(topic_name);

            dataWriter_512[topic_name] = senderNode->createDataWriter<Message_512>(topic_name);
            dataWriter_51200[topic_name] = senderNode->createDataWriter<Message_51200>(topic_name);
            dataWriter_524288[topic_name] =
                senderNode->createDataWriter<Message_524288>(topic_name);
            dataWriter_2621440[topic_name] =
                senderNode->createDataWriter<Message_2621440>(topic_name);
        }

        for (auto topic_name : topic_names) {
            auto dataReader_512 = receiverNode->createDataReader<Message_512>(
                topic_name,
                [delays_512 = delays_512](const std::string &topic_name, std::shared_ptr<Message_512> data) {
                    auto now = std::chrono::system_clock::now();
                    auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
                    auto epoch = value.time_since_epoch();
                    auto timestamp =
                        std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();

                  
                    int64_t delay_time = timestamp - data->timestamp();
                    {
                        std::lock_guard<std::mutex> lock(delays_mutex_512);
                        auto &delays = (*delays_512)[data->id()]; // 确保访问的是已存在的元素
                        delays.push_back(delay_time);          // 添加延迟数据
                    }
                    LOG(debug) << "recv message [" << topic_name << "]: " << data->id()
                    << " recv message delay time: " << delay_time
                    << "data:" << data->data()[0];
                });
            auto dataReader_51200 = receiverNode->createDataReader<Message_51200>(
                topic_name,
                [delays_51200 = delays_51200](const std::string &topic_name, std::shared_ptr<Message_51200> data) {
                    auto now = std::chrono::system_clock::now();
                    auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
                    auto epoch = value.time_since_epoch();
                    auto timestamp =
                        std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();

                    int64_t delay_time = timestamp - data->timestamp();
                    {
                        std::lock_guard<std::mutex> lock(delays_mutex_51200);
                        auto &delays = (*delays_51200)[data->id()]; // 确保访问的是已存在的元素
                        delays.push_back(delay_time);            // 添加延迟数据
                    }

                    LOG(debug) << "recv message [" << topic_name << "]: " << data->id()
                               << " recv message delay time: " << delay_time
                               << "data:" << data->data()[0];
                });

            auto dataReader_524288 = receiverNode->createDataReader<Message_524288>(
                topic_name,
                [delays_524288 = delays_524288](const std::string &topic_name, std::shared_ptr<Message_524288> data) {
                    auto now = std::chrono::system_clock::now();
                    auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
                    auto epoch = value.time_since_epoch();
                    auto timestamp =
                        std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();

                    int64_t delay_time = timestamp - data->timestamp();
                    {
                        std::lock_guard<std::mutex> lock(delays_mutex_524288);
                        auto &delays = (*delays_524288)[data->id()]; // 确保访问的是已存在的元素
                        delays.push_back(delay_time);             // 添加延迟数据
                    }

                    LOG(debug) << "recv message [" << topic_name << "]: " << data->id()
                               << " recv message delay time: " << delay_time
                               << "data:" << data->data()[0];
                });

            auto dataReader_2621440 = receiverNode->createDataReader<Message_2621440>(
                topic_name,
                [delays_2621440 = delays_2621440](const std::string &topic_name, std::shared_ptr<Message_2621440> data) {
                    auto now = std::chrono::system_clock::now();
                    auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
                    auto epoch = value.time_since_epoch();
                    auto timestamp =
                        std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();

                    int64_t delay_time = timestamp - data->timestamp();
                    {
                        std::lock_guard<std::mutex> lock(delays_mutex_2621440);
                        auto &delays = (*delays_2621440)[data->id()]; // 确保访问的是已存在的元素
                        delays.push_back(delay_time);              // 添加延迟数据
                    }

                    LOG(debug) << "recv message [" << topic_name << "]: " << data->id()
                               << " recv message delay time: " << delay_time
                               << "data:" << data->data()[0];
                });
        }
    }

    // 清理函数
    void TearDown() override
    {
        // 销毁节点和其他资源
        senderNode.reset();
        receiverNode.reset();
    }

    // 公共成员变量
    std::unique_ptr<DataNode> senderNode;
    std::unique_ptr<DataNode> receiverNode;

    std::unordered_map<std::string, DDSTopicDataWriter<Message_512> *> dataWriter_512;
    std::unordered_map<std::string, DDSTopicDataWriter<Message_51200> *> dataWriter_51200;
    std::unordered_map<std::string, DDSTopicDataWriter<Message_524288> *> dataWriter_524288;
    std::unordered_map<std::string, DDSTopicDataWriter<Message_2621440> *> dataWriter_2621440;

    std::shared_ptr<std::unordered_map<int32_t, std::vector<uint64_t>>> delays_512;
    std::shared_ptr<std::unordered_map<int32_t, std::vector<uint64_t>>> delays_51200;
    std::shared_ptr<std::unordered_map<int32_t, std::vector<uint64_t>>> delays_524288;
    std::shared_ptr<std::unordered_map<int32_t, std::vector<uint64_t>>> delays_2621440;
};

static void calculateAverageDelay(std::mutex &mutex_,
                                  const std::shared_ptr<std::unordered_map<int32_t, std::vector<uint64_t>>> delays,
                                  std::string tag)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (delays->empty()) {
        return;
    }
    for (const auto &[id, delay] : *delays) {
        uint64_t sum = 0;
        for (auto delay_time : delay) {
            sum += delay_time;
        }
        if (!delay.empty()) {
            LOG(info) << "HOST ID: " << get_hostname() << "  Average delay for message [" << tag
                      << "][" << id << "]: " << static_cast<double>(sum) / delay.size();
        } else {
            LOG(warning) << "No messages received for message [" << id << "].";
        }
    }
    return;
}

TEST_F(UDPTestFixtureMult, MultiSenderReceiverTest1k)
{
    uint32_t cnt = 0;
    int index = 0;

    while (cnt < 10) {
        Message_512 message;
        auto now = std::chrono::system_clock::now();
        auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
        auto epoch = value.time_since_epoch();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
        message.timestamp(timestamp);
        message.id(get_hostname());
        std::unique_ptr<std::array<int16_t, 512>> data =
            std::make_unique<std::array<int16_t, 512>>();
        for (size_t i = 0; i < data->size(); ++i) {
            (*data)[i] = static_cast<int16_t>(cnt); // 示例数据
        }
        message.data(*data);

        for (auto &[topic_name, dataWriter] : dataWriter_512) {
            if (dataWriter->writeMessage(message)) {
                LOG(debug) << "send message: [" << topic_name << "]" << message.id();
            }
        }
        cnt++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    calculateAverageDelay(delays_mutex_512, delays_512, "1k");
}

// TEST_F(UDPTestFixtureMult, MultiSenderReceiverTest100k)
// {
//     uint32_t cnt = 0;
//     int index = 0;

//     while (cnt < 10) {
//         Message_51200 message;
//         auto now = std::chrono::system_clock::now();
//         auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
//         auto epoch = value.time_since_epoch();
//         auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
//         message.timestamp(timestamp);
//         message.id(get_hostname());
//         std::unique_ptr<std::array<int16_t, 51200>> data =
//             std::make_unique<std::array<int16_t, 51200>>();
//         for (size_t i = 0; i < data->size(); ++i) {
//             (*data)[i] = static_cast<int16_t>(cnt); // 示例数据
//         }
//         message.data(*data);
//         for (auto &[topic_name, dataWriter] : dataWriter_51200) {
//             if (dataWriter->writeMessage(message)) {
//                 LOG(debug) << "send message: " << message.id();
//             }
//         }
//         cnt++;
//         std::this_thread::sleep_for(std::chrono::milliseconds(100));
//     }
//     calculateAverageDelay(delays_mutex_51200, delays_51200, "100k");
// }

// TEST_F(UDPTestFixtureMult, MultiSenderReceiverTest1M)
// {
//     uint32_t cnt = 0;
//     int index = 0;

//     while (cnt < 10) {
//         Message_524288 message;
//         auto now = std::chrono::system_clock::now();
//         auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
//         auto epoch = value.time_since_epoch();
//         auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
//         message.timestamp(timestamp);
//         message.id(get_hostname());
//         std::unique_ptr<std::array<int16_t, 524288>> data =
//             std::make_unique<std::array<int16_t, 524288>>();
//         for (size_t i = 0; i < data->size(); ++i) {
//             (*data)[i] = static_cast<int16_t>(cnt); // 示例数据
//         }
//         message.data(*data);
//         for (auto &[topic_name, dataWriter] : dataWriter_524288) {
//             if (dataWriter->writeMessage(message)) {
//                 LOG(debug) << "send message: " << message.id();
//             }
//         }
//         cnt++;
//         std::this_thread::sleep_for(std::chrono::milliseconds(100));
//     }
//     calculateAverageDelay(delays_mutex_524288, delays_524288, "1M");
// }

// TEST_F(UDPTestFixtureMult, MultiSenderReceiverTest5M)
// {
//     uint32_t cnt = 0;
//     int index = 0;

//     while (cnt < 10) {
//         Message_2621440 message;
//         auto now = std::chrono::system_clock::now();
//         auto value = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
//         auto epoch = value.time_since_epoch();
//         auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
//         message.timestamp(timestamp);
//         message.id(get_hostname());
//         std::unique_ptr<std::array<int16_t, 2621440>> data =
//             std::make_unique<std::array<int16_t, 2621440>>();
//         for (size_t i = 0; i < data->size(); ++i) {
//             (*data)[i] = static_cast<int16_t>(cnt); // 示例数据
//         }
//         message.data(*data);
//         for (auto &[topic_name, dataWriter] : dataWriter_2621440) {
//             if (dataWriter->writeMessage(message)) {
//                 LOG(debug) << "send message: " << message.id();
//             }
//         }
//         cnt++;
//         std::this_thread::sleep_for(std::chrono::milliseconds(100));
//     }
//     calculateAverageDelay(delays_mutex_2621440, delays_2621440, "5M");
// }
