#pragma once

#include <QCoreApplication>
#include <QString>


namespace UDI
{
class Privileges
{
    Q_DECLARE_TR_FUNCTIONS(Privileges)
public:
    // True when this process may open a physical device for writing: an elevated Administrators token
    // on Windows, effective uid 0 elsewhere.
    static bool isElevated();

    // True when the platform can restart the app with more rights (UAC, pkexec, AppleScript) and it is
    // not already elevated.
    static bool canElevate();

    // Starts a second, elevated instance. Returns true once that process is running, in which case the
    // caller must quit so the two do not fight over the same device.
    static bool relaunchElevated();

    static QString getElevationHint();
};
} // namespace UDI
