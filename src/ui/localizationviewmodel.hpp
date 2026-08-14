#pragma once

#include <QString>
#include <QVariantList>

#include <ui/viewmodel.hpp>


namespace UDI
{
class LocalizationViewModel : public IViewModel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(LocalizationViewModel)
    Q_PROPERTY(QString currentLanguage READ getCurrentLanguage NOTIFY languageChanged FINAL)
    // One entry per language: { "code": "uk", "name": "Українська" }.
    Q_PROPERTY(QVariantList availableLanguages READ getAvailableLanguages CONSTANT FINAL)

public:
    LocalizationViewModel();

    void resetToDefault() override;
    void onLanguageChanged() override;

    QString getCurrentLanguage() const;
    QVariantList getAvailableLanguages() const;

    Q_INVOKABLE void selectLanguage(const QString& languageCode);

Q_SIGNALS:
    void languageChanged();
};
} // namespace UDI
