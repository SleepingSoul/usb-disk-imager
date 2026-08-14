#include <managers/localizationmanager.hpp>

#include <algorithm>

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>

#include <utils/usersettings.hpp>


namespace UDI
{
using namespace Qt::Literals::StringLiterals;

namespace
{
const QLatin1StringView TranslationsResourcePrefix{ ":/i18n" };
const QLatin1StringView ApplicationTranslationPrefix{ "usb-disk-imager_" };
const QLatin1StringView QtTranslationPrefix{ "qtbase_" };
} // namespace

LocalizationManager::LocalizationManager()
    : Manager("LocalizationManager")
{
    const QString initialLanguageCode = resolveInitialLanguageCode();

    if (!setLanguage(initialLanguageCode))
    {
        qWarning() << "Could not load translations for" << initialLanguageCode
                   << "- falling back to the untranslated source strings";
        m_currentLanguageCode = getConfig().fallbackLanguage;
    }
}

LocalizationManager::~LocalizationManager()
{
    removeTranslators();
}

const std::vector<QString>& LocalizationManager::getAvailableLanguageCodes() const
{
    return getConfig().availableLanguages;
}

QString LocalizationManager::getLanguageDisplayName(const QString& languageCode)
{
    const QLocale locale{ languageCode };
    const QString endonym = locale.nativeLanguageName();

    if (endonym.isEmpty())
    {
        return languageCode.toUpper();
    }

    // A bare language code resolves to a territory, so English arrives as "American English". The
    // language itself is the last word, and that is what belongs on a language button.
    const QString languageName = endonym.section(u' ', -1);

    return languageName[0].toUpper() + languageName.mid(1);
}

bool LocalizationManager::setLanguage(const QString& languageCode)
{
    if (!isLanguageAvailable(languageCode))
    {
        qWarning() << "Language" << languageCode << "is not one of the available languages";
        return false;
    }

    removeTranslators();

    const bool installed = installTranslators(languageCode);

    m_currentLanguageCode = languageCode;
    UserSettings::setLanguageCode(languageCode);

    // The default locale drives number and date formatting, which has to follow the chosen language even
    // when the system is set to something else.
    QLocale::setDefault(QLocale{ languageCode });

    qInfo() << "Interface language set to" << languageCode;

    Q_EMIT languageChanged(languageCode);

    return installed;
}

QString LocalizationManager::resolveInitialLanguageCode() const
{
    const QString storedLanguageCode = UserSettings::getLanguageCode();

    if (isLanguageAvailable(storedLanguageCode))
    {
        return storedLanguageCode;
    }

    const QString systemLanguageCode = QLocale::system().name().section(u'_', 0, 0);

    if (isLanguageAvailable(systemLanguageCode))
    {
        return systemLanguageCode;
    }

    return getConfig().fallbackLanguage;
}

bool LocalizationManager::installTranslators(const QString& languageCode)
{
    m_applicationTranslator = std::make_unique<QTranslator>();

    if (m_applicationTranslator->load(ApplicationTranslationPrefix + languageCode,
            TranslationsResourcePrefix))
    {
        QCoreApplication::installTranslator(m_applicationTranslator.get());
    }
    else
    {
        m_applicationTranslator.reset();

        // The source language has no catalogue of its own, so a missing file is only worth reporting for
        // the other languages.
        if (languageCode != getConfig().fallbackLanguage)
        {
            return false;
        }
    }

    // Qt's own strings cover the standard dialog buttons. They are absent from a build that ships no Qt
    // catalogues, which costs nothing but the translations of those buttons.
    m_qtTranslator = std::make_unique<QTranslator>();

    if (m_qtTranslator->load(QtTranslationPrefix + languageCode,
            QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
    {
        QCoreApplication::installTranslator(m_qtTranslator.get());
    }
    else
    {
        m_qtTranslator.reset();
    }

    return true;
}

void LocalizationManager::removeTranslators()
{
    if (m_applicationTranslator)
    {
        QCoreApplication::removeTranslator(m_applicationTranslator.get());
        m_applicationTranslator.reset();
    }

    if (m_qtTranslator)
    {
        QCoreApplication::removeTranslator(m_qtTranslator.get());
        m_qtTranslator.reset();
    }
}

bool LocalizationManager::isLanguageAvailable(const QString& languageCode) const
{
    if (languageCode.isEmpty())
    {
        return false;
    }

    const auto& availableLanguages = getConfig().availableLanguages;

    return std::any_of(availableLanguages.cbegin(), availableLanguages.cend(),
        [&languageCode](const QString& available)
        {
            return available == languageCode;
        });
}
} // namespace UDI
