#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

#include "quill/Backend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "toml++/toml.hpp"


namespace Logging
{

class Logger
{
   public:
    Logger(quill::Logger*);

    ~Logger() = default;

    quill::Logger* get();

   private:
    quill::Logger* logger_;
};

class LoggerFactory
{
   public:
    LoggerFactory() = delete;
    LoggerFactory(LoggerFactory&&) = delete;
    LoggerFactory(const LoggerFactory&) = delete;

    static void Init(toml::table& config);

    static Logger& GetLogger(const std::string& logger_file);

   private:
    static quill::Logger* createLogger(const std::string&);

   private:
    static inline std::filesystem::path logs_path_;

    static inline quill::BackendOptions backend_options_;

    static inline quill::LogLevel level_;

    static inline quill::PatternFormatterOptions pfo_;

    static inline std::unordered_map<std::string, std::unique_ptr<Logger>> loggers_;
};

}  // namespace Logging