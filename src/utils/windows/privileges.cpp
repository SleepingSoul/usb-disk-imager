#include <utils/privileges.hpp>

#include <QCoreApplication>
#include <QDir>

#include <disk/windows/windowsutils.hpp>

#include <shellapi.h>


namespace UDI
{
namespace
{
QString quoteArgument(const QString& argument)
{
    return QStringLiteral("\"%1\"").arg(QString{ argument }.replace(u'"', QLatin1StringView{ "\\\"" }));
}
} // namespace

bool Privileges::isElevated()
{
    Windows::ScopedHandle token;

    {
        HANDLE rawToken = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &rawToken))
        {
            qWarning() << "Could not open the process token:" << Windows::formatLastSystemError();
            return false;
        }

        token = Windows::ScopedHandle{ rawToken };
    }

    TOKEN_ELEVATION elevation{};
    DWORD returnedSize = 0;

    if (!GetTokenInformation(token.get(), TokenElevation, &elevation, sizeof(elevation), &returnedSize))
    {
        qWarning() << "Could not read the elevation state of the process token:"
                   << Windows::formatLastSystemError();
        return false;
    }

    return elevation.TokenIsElevated != 0;
}

bool Privileges::canElevate()
{
    return !isElevated();
}

bool Privileges::relaunchElevated()
{
    const QStringList arguments = QCoreApplication::arguments();

    QStringList forwardedArguments;
    for (qsizetype index = 1; index < arguments.size(); ++index)
    {
        forwardedArguments.push_back(quoteArgument(arguments.at(index)));
    }

    const std::wstring executable =
        Windows::toWideString(QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
    const std::wstring parameters = Windows::toWideString(forwardedArguments.join(u' '));
    const std::wstring workingDirectory =
        Windows::toWideString(QDir::toNativeSeparators(QCoreApplication::applicationDirPath()));

    SHELLEXECUTEINFOW executeInfo{};
    executeInfo.cbSize = sizeof(executeInfo);
    executeInfo.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    executeInfo.lpVerb = L"runas";
    executeInfo.lpFile = executable.c_str();
    executeInfo.lpParameters = parameters.empty() ? nullptr : parameters.c_str();
    executeInfo.lpDirectory = workingDirectory.c_str();
    executeInfo.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&executeInfo))
    {
        const DWORD errorCode = GetLastError();

        // Declining the UAC prompt is a decision, not a failure worth shouting about.
        if (errorCode == ERROR_CANCELLED)
        {
            qInfo() << "The user declined the elevation prompt";
        }
        else
        {
            qWarning() << "Could not start an elevated instance:" << Windows::formatSystemError(errorCode);
        }

        return false;
    }

    if (executeInfo.hProcess)
    {
        CloseHandle(executeInfo.hProcess);
    }

    return true;
}

QString Privileges::getElevationHint()
{
    return tr("Raw disk access needs administrator rights. Restart the app as administrator, or right-click "
              "its shortcut and choose “Run as administrator”.");
}
} // namespace UDI
