
#include <string>
#include <chrono>
#include <thread>

#include "Monitor.h"
#include "optionparser.h"
#include "arg_configuration.h"
#include "log/logger.h"
#include "MonitorDataBase.h"
#include "MonitorDataBaseManager.h"
#include "MonitorUi.h"

enum EntityType { PUBLISHER, SUBSCRIBER, MONITOR };

int main(int argc, char **argv)
{
    Logger::Instance().Init("log/myapp.log", Logger::console, Logger::info, 60, 5);

    int domain = 0;
    int n_bins = 1;
    int t_interval = 3000;
    std::string dump_file = "";
    bool reset = false;
    uint32_t columns = 80;
    if (argc > 1) {

        argc -= (argc > 0);
        argv += (argc > 0);
        option::Stats stats(usage, argc, argv);
        std::vector<option::Option> options(stats.options_max);
        std::vector<option::Option> buffer(stats.buffer_max);
        option::Parser parse(usage, argc, argv, &options[0], &buffer[0]);
        if (parse.error()) {
            option::printUsage(fwrite, stdout, usage, columns);
            return 1;
        }
        if (options[optionIndex::HELP]) {
            option::printUsage(fwrite, stdout, usage, columns);
            return 0;
        }

        for (int i = 0; i < parse.optionsCount(); ++i) {
            option::Option &opt = buffer[i];
            switch (opt.index()) {
                case optionIndex::HELP:
                    break;
                case optionIndex::DOMAIN_ID:
                    domain = strtol(opt.arg, nullptr, 10);
                    break;

                case optionIndex::N_BINS:

                    n_bins = strtol(opt.arg, nullptr, 10);
                    if (n_bins < 0) {
                        std::cerr << "ERROR: only nonnegative values accepted for --bins option."
                                  << std::endl;
                        option::printUsage(fwrite, stdout, usage, columns);
                        return 1;
                    }

                    break;
                case optionIndex::T_INTERVAL:

                    t_interval = strtol(opt.arg, nullptr, 10);
                    if (t_interval < 1) {
                        std::cerr << "ERROR: only positive values accepted for --time option."
                                  << std::endl;
                        option::printUsage(fwrite, stdout, usage, columns);
                        return 1;
                    }

                    break;

                case optionIndex::RESET:
                    reset = true;
                    break;

                case optionIndex::DUMP_FILE:
                    dump_file = opt.arg;
                    break;

                case optionIndex::UNKNOWN_OPT:
                    std::cerr << "ERROR: " << opt.name << " is not a valid argument." << std::endl;
                    option::printUsage(fwrite, stdout, usage, columns);
                    return 1;
                    break;
            }
        }
    }

    auto &manager = Monitor::MonitorDataBaseManager::getInstance();
    if (!manager.init(domain, n_bins, t_interval, dump_file, reset)) {
        LOG(error) << "MonitorDataBaseManager init failed";
        return 1;
    }
    else{
        LOG(info) << "MonitorDataBaseManager initialized successfully";
        std::thread([&manager]() { manager.run();}).detach();
    }
    int count = 0;

    //Monitor::launchTopicMonitorUI();
    while (true) {
        
        if (count++ % 5 == 0) {
            LOG(info) << "Monitor is running...";
        }
        // if(count % 10 == 0) {
        //   manager.stop();
        //   LOG(info) << "MonitorDataBaseManager stopped successfully";
        //   break;
        // }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}
