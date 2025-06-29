// Logger.cpp
#include "Logger.h"

std::shared_ptr<spdlog::logger> Logger::logger;

void Logger::init(const std::string& logFile) {
    // 初始化基本的文件日志器
    spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");
    logger = spdlog::basic_logger_mt("flashsale", logFile);
    spdlog::set_default_logger(logger);
}

std::shared_ptr<spdlog::logger> Logger::getLogger() {
    return logger;
}
