#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <filesystem>
#include <unistd.h>
#include "log/logger.h"
#include "ondemand/on_demand_pub.h"
#include "ondemand/on_demand_sub.h"
#include <fastdds/dds/log/Log.hpp>

void publish()
{
    dsf::ondemand::OnDemandPub pub;
    pub.init("pubNode");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    pub.start();
    std::vector<DSF::Var::Define> vars;
    for (int i = 0; i < 10; ++i) {
        DSF::Var::Define var;
        var.name("var" + std::to_string(i));
        var.nodeName("pubNode");
        var.modelName("int");
        vars.push_back(std::move(var));
    }
    pub.createVars(vars);

    std::this_thread::sleep_for(std::chrono::seconds(10));
    std::vector<std::string> varDelNames;
    for (int i = 0; i < 10; ++i) {
        std::string varName = "var" + std::to_string(i);
        varDelNames.push_back(varName);
    }
    // pub.deleteVars(varDelNames);

    std::this_thread::sleep_for(std::chrono::seconds(100000));
}

void subscribe()
{
    dsf::ondemand::OnDemandSub sub;
    std::string nodeName = "subNode" + std::to_string(getpid());
    sub.init(nodeName);
    sub.start();
    LOG(critical) << "total received vars: " << sub.getTotalReceivedVars();

    // std::vector<dsf::ondemand::SubscriptionItem> items;
    //  std::vector<std::string> unitems;
    // for (int i = 0; i < 5; ++i) {
    //     std::string varName = "var" + std::to_string(i);
    //     items.push_back({varName, 500});
    //     unitems.push_back(varName);
    // }
    // sub.subscribe("pubNode", items);
    // std::this_thread::sleep_for(std::chrono::seconds(5));
    // sub.unsubscribe("pubNode", unitems);
    // sub.subscribe("pubNode", items);
    // std::this_thread::sleep_for(std::chrono::seconds(2));
    // sub.subscribe("pubNode", items);
    // std::this_thread::sleep_for(std::chrono::seconds(2));
    // sub.stop();
    std::this_thread::sleep_for(std::chrono::seconds(1000000));
}

int main(int argc, char **argv)
{
    eprosima::fastdds::dds::Log::SetVerbosity(eprosima::fastdds::dds::Log::Kind::Info);
    Logger::GetInstance()->Init("log/1.log", Logger::console, Logger::info, 10, 3);
    LOG(info) << "start on demand demo";

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " sub/pub" << std::endl;
        return -1;
    }

    if (strcmp(argv[1], "sub") == 0) {
        subscribe();
    } else if (strcmp(argv[1], "pub") == 0) {
        publish();
    } else {
        std::cerr << "unknown command: " << argv[1] << std::endl;
    }

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}