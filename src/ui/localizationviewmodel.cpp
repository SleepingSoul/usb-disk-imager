#include <ui/localizationviewmodel.hpp>

#include <QVariantMap>

#include <managers/localizationmanager.hpp>


namespace UDI
{
LocalizationViewModel::LocalizationViewModel() = default;

void LocalizationViewModel::resetToDefault()
{
    Q_EMIT languageChanged();
}

QString LocalizationViewModel::getCurrentLanguage() const
{
    return getManager<LocalizationManager>().getCurrentLanguageCode();
}

QVariantList LocalizationViewModel::getAvailableLanguages() const
{
    QVariantList languages;

    for (const QString& languageCode : getManager<LocalizationManager>().getAvailableLanguageCodes())
    {
        QVariantMap language;
        language[QStringLiteral("code")] = languageCode;
        language[QStringLiteral("name")] = LocalizationManager::getLanguageDisplayName(languageCode);

        languages.push_back(language);
    }

    return languages;
}

void LocalizationViewModel::selectLanguage(const QString& languageCode)
{
    getManager<LocalizationManager>().setLanguage(languageCode);
}

void LocalizationViewModel::onLanguageChanged()
{
    Q_EMIT languageChanged();
}
} // namespace UDI
