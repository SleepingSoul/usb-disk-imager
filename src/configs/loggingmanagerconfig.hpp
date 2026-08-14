#pragma once

#include <vector>

#include <app/managerconfig.hpp>


namespace UDI
{
using namespace Qt::Literals::StringLiterals;

class AppLoggerConfig
{
public:
    quint64 maxLogFileSizeBytes;
    unsigned maxLogFileCount;
    int flushIntervalMilliseconds;
    bool preventDuplicates;
    unsigned duplicatesTimeoutMilliseconds;
    std::vector<QString> disabledLoggingCategories;

    bool parse(const ManagerConfigMap& config)
    {
        bool result = true;

        result = config["max_log_file_size_b"_L1].get(maxLogFileSizeBytes) && result;
        result = config["max_log_files"_L1].get(maxLogFileCount) && result;
        result = config["flush_interval_ms"_L1].get(flushIntervalMilliseconds) && result;
        result = config["prevent_duplicates"_L1].get(preventDuplicates) && result;
        result = config["duplicates_timeout_ms"_L1].get(duplicatesTimeoutMilliseconds) && result;
        result = config["disabled_logging_categories"_L1].get(disabledLoggingCategories) && result;

        return result;
    }
};

class LoggingManagerConfig
{
public:
    AppLoggerConfig appLoggerConfig;
    // An empty path means the platform's own application data location, which is where a desktop app's
    // logs belong; a relative path is resolved against the user's home directory and an absolute one is
    // used verbatim.
    QString logsDirectoryPath;

    bool parse(const ManagerConfigMap& config)
    {
        bool result = true;

        result = config["app_logger"_L1].getNested(appLoggerConfig) && result;
        result = config["path"_L1].get(logsDirectoryPath) && result;

        return result;
    }

    QString getFileName() const
    {
        return QStringLiteral("loggingmanagerconfig.json");
    }
};
} // namespace UDI
