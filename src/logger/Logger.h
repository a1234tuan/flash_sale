// Logger.h
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

class Logger {
public:
    static void init(const std::string& logFile);
    static std::shared_ptr<spdlog::logger> getLogger();
private:
    static std::shared_ptr<spdlog::logger> logger;
};
