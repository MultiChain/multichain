// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "v8_win/v8utils.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"

namespace mc_v8
{
void CreateLogger()
{
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::warn);
//    console_sink->set_pattern("[multi_sink_example] [%^%l%$] %v");

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>((dataDir / "v8_win.log").string(), true);
    file_sink->set_level(spdlog::level::trace);

    std::vector<spdlog::sink_ptr> sinks = { console_sink, file_sink };
    logger = std::make_shared<spdlog::logger>("v8_win", sinks.begin(), sinks.end());
    logger->set_level(spdlog::level::trace);
}

bool fDebug = false;
fs::path dataDir;
std::shared_ptr<spdlog::logger> logger;
} // namespace mc_v8
