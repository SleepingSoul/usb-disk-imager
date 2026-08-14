#pragma once

#include <app/managerconfig.hpp>


namespace UDI
{
using namespace Qt::Literals::StringLiterals;

class DeviceManagerConfig
{
public:
    // Devices are re-enumerated on this interval so a card inserted while the app is open shows up
    // without the user having to press anything.
    int refreshIntervalMilliseconds;
    bool includeFixedDisksByDefault;

    bool parse(const ManagerConfigMap& config)
    {
        bool result = true;

        result = config["refresh_interval_ms"_L1].get(refreshIntervalMilliseconds) && result;
        result = config["include_fixed_disks_by_default"_L1].get(includeFixedDisksByDefault) && result;

        return result;
    }

    QString getFileName() const
    {
        return QStringLiteral("devicemanagerconfig.json");
    }
};
} // namespace UDI
