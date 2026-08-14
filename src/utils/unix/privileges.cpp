#include <utils/privileges.hpp>

#include <array>

#include <QCoreApplication>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QtGlobal>

#include <unistd.h>


namespace UDI
{
using namespace Qt::Literals::StringLiterals;

namespace
{
const QLatin1StringView PolicyKitExecutable{ "pkexec" };

// pkexec deliberately starts the target with a minimal environment, so the few variables a GUI process
// needs to find the display have to be carried over explicitly.
const std::array ForwardedEnvironmentVariables{
    "DISPLAY"_L1, "WAYLAND_DISPLAY"_L1, "XAUTHORITY"_L1, "XDG_RUNTIME_DIR"_L1, "QT_QPA_PLATFORM"_L1
};

QString findPolicyKit()
{
    return QStandardPaths::findExecutable(PolicyKitExecutable);
}
} // namespace

bool Privileges::isElevated()
{
    return ::geteuid() == 0;
}

bool Privileges::canElevate()
{
    if (isElevated())
    {
        return false;
    }

#if defined(Q_OS_MACOS)
    // A GUI app relaunched through AppleScript's administrator privileges loses its session and comes up
    // without a window server connection, so there is nothing honest to offer here.
    return false;
#else
    return !findPolicyKit().isEmpty();
#endif
}

bool Privileges::relaunchElevated()
{
#if defined(Q_OS_MACOS)
    return false;
#else
    const QString policyKit = findPolicyKit();

    if (policyKit.isEmpty())
    {
        return false;
    }

    const QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();

    QStringList arguments{ QStringLiteral("env") };

    for (const QLatin1StringView variableName : ForwardedEnvironmentVariables)
    {
        const QString value = environment.value(variableName);

        if (!value.isEmpty())
        {
            arguments.push_back(QStringLiteral("%1=%2").arg(variableName, value));
        }
    }

    arguments.push_back(QCoreApplication::applicationFilePath());

    const QStringList applicationArguments = QCoreApplication::arguments();
    for (qsizetype index = 1; index < applicationArguments.size(); ++index)
    {
        arguments.push_back(applicationArguments.at(index));
    }

    if (!QProcess::startDetached(policyKit, arguments))
    {
        qWarning() << "Could not start an elevated instance through" << policyKit;
        return false;
    }

    return true;
#endif
}

QString Privileges::getElevationHint()
{
#if defined(Q_OS_MACOS)
    return tr("Raw disk access needs root rights. Quit the app and start it from Terminal with "
              "“sudo %1”.").arg(QCoreApplication::applicationFilePath());
#else
    return tr("Raw disk access needs root rights. Restart the app as administrator, run it with sudo, or "
              "add your user to the “disk” group and log in again.");
#endif
}
} // namespace UDI
