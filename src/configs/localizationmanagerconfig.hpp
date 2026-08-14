#pragma once

#include <vector>

#include <app/managerconfig.hpp>


namespace UDI
{
using namespace Qt::Literals::StringLiterals;

class LocalizationManagerConfig
{
public:
    // Language codes in the order they are offered in the UI. Each needs a matching
    // usb-disk-imager_<code>.qm in the binary's resources.
    std::vector<QString> availableLanguages;
    QString fallbackLanguage;

    bool parse(const ManagerConfigMap& config)
    {
        bool result = true;

        result = config["available_languages"_L1].get(availableLanguages) && result;
        result = config["fallback_language"_L1].get(fallbackLanguage) && result;

        return result;
    }

    QString getFileName() const
    {
        return QStringLiteral("localizationmanagerconfig.json");
    }
};
} // namespace UDI
