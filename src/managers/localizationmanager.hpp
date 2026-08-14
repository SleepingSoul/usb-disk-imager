#pragma once

#include <memory>
#include <vector>

#include <QTranslator>

#include <app/manager.hpp>
#include <configs/localizationmanagerconfig.hpp>


namespace UDI
{
class LocalizationManager : public Manager<LocalizationManager, LocalizationManagerConfig>
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(LocalizationManager)
public:
    LocalizationManager();

    // Uninstalls the catalogues before they are freed: QCoreApplication keeps raw pointers to installed
    // translators and would be left holding dangling ones.
    ~LocalizationManager() override;

    const QString& getCurrentLanguageCode() const { return m_currentLanguageCode; }
    const std::vector<QString>& getAvailableLanguageCodes() const;

    // Endonym, so the entry a user is looking for reads in their own language.
    static QString getLanguageDisplayName(const QString& languageCode);

    bool setLanguage(const QString& languageCode);

Q_SIGNALS:
    void languageChanged(const QString& languageCode);

private:
    // The language to start in: the stored preference, else the system's, else the configured fallback.
    QString resolveInitialLanguageCode() const;
    bool installTranslators(const QString& languageCode);
    void removeTranslators();
    bool isLanguageAvailable(const QString& languageCode) const;

    std::unique_ptr<QTranslator> m_applicationTranslator;
    std::unique_ptr<QTranslator> m_qtTranslator;
    QString m_currentLanguageCode;
};
} // namespace UDI
