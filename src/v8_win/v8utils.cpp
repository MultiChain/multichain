#include "v8_win/v8utils.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace mc_v8
{
	bool fDebug = false;
	fs::path dataDir;
    std::shared_ptr<spdlog::logger> logger = spdlog::stdout_color_mt("v8_win");
} // namespace mc_v8
