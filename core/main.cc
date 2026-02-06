#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <filesystem>
#include "log/logger.h"
#include "ondemand/on_demand_pub.h"
#include "ondemand/on_demand_sub.h"

void publish()
{
    dsf::ondemand::OnDemandPub pub;
    pub.init("pubNode");

    std::vector<DSF::Var::Define> vars;
    for (int i = 0; i < 100; ++i) {
        DSF::Var::Define var;
        var.name("var" + std::to_string(i));
        var.nodeName("pubNode");
        var.modelName("int");
        vars.push_back(std::move(var));
    }
    pub.CreateVars(vars);

    std::this_thread::sleep_for(std::chrono::seconds(100000));
}

void subscribe()
{
    dsf::ondemand::OnDemandSub sub;
    sub.init("subNode");
    std::this_thread::sleep_for(std::chrono::seconds(100000));
}

int main(int argc, char** argv)
{
    Logger::GetInstance()->Init("log/1.log", Logger::console, Logger::info, 10, 3, true);
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