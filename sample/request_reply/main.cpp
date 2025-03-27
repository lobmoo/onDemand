// Copyright 2024 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @file main.cpp
 *
 */

#include <csignal>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include "Application.hpp"
#include "CLIParser.hpp"
#include "app_utils.hpp"
#include "ServerApp.hpp"
#include "ClientApp.hpp"

using eprosima::fastdds::dds::Log;

using namespace eprosima::fastdds::examples::request_reply;

std::function<void(int)> stop_app_handler;

void signal_handler(int signum)
{
    stop_app_handler(signum);
}

int main(int argc, char **argv)
{
    auto ret = EXIT_SUCCESS;
    const std::string service_name = "calculator_service";

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " svr/cli" << std::endl;
        return -1;
    }
    if (strcmp(argv[1], "svr") == 0) {
        auto entity = std::make_shared<ServerApp>(service_name);
    } else if (strcmp(argv[1], "cli") == 0) {
        CLIParser::config config;
        //config.entity = CLIParser::EntityKind::CLIENT;
        config.x = 10;
        config.y = 10;
        auto entity = std::make_shared<ClientApp>(config, service_name);
        entity->run();
    } else {
        std::cerr << "unknown command: " << argv[1] << std::endl;
    }
    while (std::cin.get() != '\n') {
    }

    return ret;
}
