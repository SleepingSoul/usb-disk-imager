#pragma once

#include <app/managerconfig.hpp>


namespace UDI
{
using namespace Qt::Literals::StringLiterals;

class UiManagerConfig
{
public:
    int defaultWindowWidth;
    int defaultWindowHeight;
    int minimumWindowWidth;
    int minimumWindowHeight;

    bool parse(const ManagerConfigMap& config)
    {
        bool result = true;

        result = config["default_window_width"_L1].get(defaultWindowWidth) && result;
        result = config["default_window_height"_L1].get(defaultWindowHeight) && result;
        result = config["minimum_window_width"_L1].get(minimumWindowWidth) && result;
        result = config["minimum_window_height"_L1].get(minimumWindowHeight) && result;

        return result;
    }

    QString getFileName() const
    {
        return QStringLiteral("uimanagerconfig.json");
    }
};
} // namespace UDI
