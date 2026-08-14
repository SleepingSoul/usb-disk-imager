#include <managers/loggingmanager.hpp>

#include <algorithm>
#include <array>
#include <utility>

#include <spdlog/sinks/dup_filter_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QtMessageHandler>


namespace UDI
{
namespace
{
const char* const LoggerName = "app_logger";
const char* const LogFileName = "usb-disk-imager.log";
const QLatin1StringView QtLoggerPattern{ "%{if-category}[%{category}] %{endif}%{message}" };
const char* const SpdlogPattern = "[%Y-%m-%d %H:%M:%S.%e] [%l] %v";

using namespace spdlog::level;

const std::array QtMsgTypeToSpdlogLogLevelMapping{
    std::pair{ QtMsgType::QtDebugMsg, level_enum::debug },
    std::pair{ QtMsgType::QtWarningMsg, level_enum::warn },
    std::pair{ QtMsgType::QtCriticalMsg, level_enum::critical },
    std::pair{ QtMsgType::QtInfoMsg, level_enum::info },
    std::pair{ QtMsgType::QtFatalMsg, level_enum::critical }
};

spdlog::level::level_enum convertQtMsgTypeToSpdlogLogLevel(QtMsgType logLevel)
{
    const auto mapping = std::find_if(QtMsgTypeToSpdlogLogLevelMapping.cbegin(),
        QtMsgTypeToSpdlogLogLevelMapping.cend(),
        [logLevel](const auto& entry)
        {
            return entry.first == logLevel;
        });

    // An unrecognised message type is a bug, so it is logged at a level nobody filters out.
    return mapping != QtMsgTypeToSpdlogLogLevelMapping.cend() ? mapping->second : level_enum::critical;
}

void logMessageHandler(QtMsgType messageType, const QMessageLogContext& context, const QString& message)
{
    getManager<LoggingManager>().applicationLog(messageType, context, message);
}

QString resolveLogsDirectoryPath(const QString& configuredPath)
{
    if (configuredPath.isEmpty())
    {
        return QDir{ QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) }
            .filePath(QStringLiteral("logs"));
    }

    if (QFileInfo{ configuredPath }.isAbsolute())
    {
        return configuredPath;
    }

    return QDir{ QDir::homePath() }.filePath(configuredPath);
}
} // namespace

LoggingManager::LoggingManager()
    : Manager("LoggingManager")
{
    qRegisterMetaType<QtMsgType>("QtMsgType");

    const auto& config = getConfig();

    m_logsDirectoryPath = resolveLogsDirectoryPath(config.logsDirectoryPath);

    const QDir logsDirectory{ m_logsDirectoryPath };

    // spdlog would create the directory itself but reports failure by throwing, which this codebase does
    // not use, so create it here and fail loudly instead.
    if (!logsDirectory.mkpath(QStringLiteral(".")))
    {
        qFatal("LoggingManager: failed to create logs directory %s",
            qUtf8Printable(logsDirectory.absolutePath()));
    }

    const QString logFilePath = logsDirectory.filePath(QLatin1StringView{ LogFileName });

    std::shared_ptr<spdlog::sinks::dist_sink_mt> distributingSink = nullptr;

    if (config.appLoggerConfig.preventDuplicates)
    {
        distributingSink = std::make_shared<spdlog::sinks::dup_filter_sink_mt>(
            std::chrono::milliseconds{ config.appLoggerConfig.duplicatesTimeoutMilliseconds });
    }
    else
    {
        distributingSink = std::make_shared<spdlog::sinks::dist_sink_mt>();
    }

    spdlog::sink_ptr rotatingFileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        logFilePath.toStdString(),
        config.appLoggerConfig.maxLogFileSizeBytes,
        config.appLoggerConfig.maxLogFileCount);

    rotatingFileSink->set_level(spdlog::level::level_enum::info);

    distributingSink->add_sink(rotatingFileSink);

    m_appLogger = std::make_shared<spdlog::logger>(std::string{ LoggerName }, distributingSink);
    m_appLogger->flush_on(spdlog::level::level_enum::err);
    m_appLogger->set_level(spdlog::level::level_enum::debug);
    m_appLogger->set_pattern(SpdlogPattern);

    spdlog::register_logger(m_appLogger);

    qSetMessagePattern(QtLoggerPattern);

    m_defaultMessageHandler = qInstallMessageHandler(&logMessageHandler);

    qInfo() << "Log session starts at:" << QDateTime::currentDateTime();

    m_flushTimer.setInterval(config.appLoggerConfig.flushIntervalMilliseconds);

    connect(&m_flushTimer, &QTimer::timeout, this, [this]
    {
        m_appLogger->flush();
    });

    m_flushTimer.start();
}

LoggingManager::~LoggingManager()
{
    spdlog::shutdown();
    qInstallMessageHandler(m_defaultMessageHandler);
}

void LoggingManager::applicationLog(QtMsgType messageType,
    const QMessageLogContext& context,
    const QString& message)
{
    const QString formattedMessage = qFormatLogMessage(messageType, context, message);

    if (Q_LIKELY(m_appLogger))
    {
        m_appLogger->log(convertQtMsgTypeToSpdlogLogLevel(messageType), formattedMessage.toStdString());
    }

    const auto& disabledCategories = getConfig().appLoggerConfig.disabledLoggingCategories;
    const bool categoryDisabled = std::any_of(disabledCategories.cbegin(), disabledCategories.cend(),
        [&context](const QString& category)
        {
            return category == QLatin1StringView{ context.category };
        });

    if (categoryDisabled)
    {
        return;
    }

    if (m_defaultMessageHandler)
    {
        if (messageType == QtMsgType::QtFatalMsg)
        {
            // The default handler aborts, so the log file has to be flushed and closed before it runs.
            spdlog::shutdown();
        }

        m_defaultMessageHandler(messageType, context, message);
    }
}
} // namespace UDI
