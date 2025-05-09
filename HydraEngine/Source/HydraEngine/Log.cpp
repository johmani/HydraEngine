module;

#include "HydraEngine/Base.h"

#ifdef HE_ENABLE_LOGGING
#define	FMT_UNICODE 0
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h> 
#endif

module HE;

#ifdef HE_ENABLE_LOGGING

namespace HE {

    static std::shared_ptr<spdlog::logger> s_CoreLogger;
    static std::shared_ptr<spdlog::logger> s_ClientLogger;

    void Log::Init(const char* client)
    {
        std::vector<spdlog::sink_ptr> logSinks;
        logSinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
        logSinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(std::string(client) + ".log", true));

        logSinks[0]->set_pattern("%^[%T] %n: %v%$");
        logSinks[1]->set_pattern("[%T] [%l] %n: %v");

        s_CoreLogger = std::make_shared<spdlog::logger>("Core", begin(logSinks), end(logSinks));
        spdlog::register_logger(s_CoreLogger);
        s_CoreLogger->set_level(spdlog::level::trace);
        s_CoreLogger->flush_on(spdlog::level::trace);

        std::filesystem::path p(client);
        std::string loggerName = p.stem().string();

        s_ClientLogger = std::make_shared<spdlog::logger>(loggerName, begin(logSinks), end(logSinks));
        spdlog::register_logger(s_ClientLogger);
        s_ClientLogger->set_level(spdlog::level::trace);
        s_ClientLogger->flush_on(spdlog::level::trace);
    }

    void Log::Shutdown()
    {
        s_ClientLogger.reset();
        s_CoreLogger.reset();
        spdlog::drop_all();
    }

    void Log::CoreTrace(const char* s) { s_CoreLogger->trace(s); }
    void Log::CoreInfo(const char* s) { s_CoreLogger->info(s); }
    void Log::CoreWarn(const char* s) { s_CoreLogger->warn(s); }
    void Log::CoreError(const char* s) { s_CoreLogger->error(s); }
    void Log::CoreCritical(const char* s) { s_CoreLogger->critical(s); }

    void Log::ClientTrace(const char* s) { s_ClientLogger->trace(s); }
    void Log::ClientInfo(const char* s) { s_ClientLogger->info(s); }
    void Log::ClientWarn(const char* s) { s_ClientLogger->warn(s); }
    void Log::ClientError(const char* s) { s_ClientLogger->error(s); }
    void Log::ClientCritical(const char* s) { s_ClientLogger->critical(s); }
}

#endif
