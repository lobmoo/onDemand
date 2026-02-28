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
    pub.start();
    std::vector<DSF::Var::Define> vars;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        DSF::Var::Define var;
        var.name("var" + std::to_string(i));
        var.nodeName("pubNode");
        var.modelName("int");
        vars.push_back(std::move(var));
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    LOG(info) << "create " << vars.size() << " vars, cost " << duration << " ms";
    pub.createVars(vars);
    std::this_thread::sleep_for(std::chrono::seconds(10));
    std::vector<std::string> varDelNames;
    for (int i = 0; i < 100; ++i) {
        std::string varName = "var" + std::to_string(i);
        varDelNames.push_back(varName);
    }
   // pub.deleteVars(varDelNames);

    std::this_thread::sleep_for(std::chrono::seconds(100000));
}

void subscribe()
{
    dsf::ondemand::OnDemandSub sub;
    sub.init("subNode");
    sub.start();
    std::this_thread::sleep_for(std::chrono::seconds(5));
    LOG(critical) << "total received vars: " << sub.getTotalReceivedVars();
   
    
    std::vector<dsf::ondemand::SubscriptionItem> items;
    for (int i = 0; i < 100; ++i) {
        std::string varName = "var" + std::to_string(i);
        items.push_back({varName, 100});
    }
    sub.subscribe("pubNode", items);
    // sub.stop(); 
    std::this_thread::sleep_for(std::chrono::seconds(1000000));
}

int main(int argc, char** argv)
{
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