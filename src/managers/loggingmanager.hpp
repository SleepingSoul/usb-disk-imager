#pragma once

#include <memory>

#include <spdlog/spdlog.h>

#include <QTimer>

#include <app/manager.hpp>
#include <configs/loggingmanagerconfig.hpp>


namespace UDI
{
class LoggingManager : public Manager<LoggingManager, LoggingManagerConfig>
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(LoggingManager)
public:
    LoggingManager();
    ~LoggingManager() override;

    void applicationLog(QtMsgType messageType, const QMessageLogContext& context, const QString& message);

    // Shown on the About page so a failed transfer can be reported with its log attached.
    const QString& getLogsDirectoryPath() const { return m_logsDirectoryPath; }

private:
    std::shared_ptr<spdlog::logger> m_appLogger;
    QtMessageHandler m_defaultMessageHandler{ nullptr };
    QString m_logsDirectoryPath;
    QTimer m_flushTimer;
};
} // namespace UDI
