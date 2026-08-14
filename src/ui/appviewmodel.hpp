#pragma once

#include <QString>
#include <QVariantList>

#include <ui/viewmodel.hpp>


namespace UDI
{
// Application-level state QML needs everywhere: who wrote this, what it is built on, how large the window
// starts, and whether the process may touch a raw device at all.
class AppViewModel : public IViewModel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AppViewModel)

    Q_PROPERTY(QString applicationName READ getApplicationName CONSTANT FINAL)
    Q_PROPERTY(QString version READ getVersion CONSTANT FINAL)
    Q_PROPERTY(QString authorName READ getAuthorName CONSTANT FINAL)
    Q_PROPERTY(QString authorEmail READ getAuthorEmail CONSTANT FINAL)
    Q_PROPERTY(QString homepage READ getHomepage CONSTANT FINAL)
    Q_PROPERTY(QString licenseName READ getLicenseName CONSTANT FINAL)
    Q_PROPERTY(QString qtVersion READ getQtVersion CONSTANT FINAL)
    Q_PROPERTY(QString platformName READ getPlatformName CONSTANT FINAL)
    Q_PROPERTY(QString logsDirectoryPath READ getLogsDirectoryPath CONSTANT FINAL)

    Q_PROPERTY(QString description READ getDescription NOTIFY languageChanged FINAL)
    Q_PROPERTY(QString copyright READ getCopyright NOTIFY languageChanged FINAL)
    Q_PROPERTY(QVariantList thirdPartyComponents READ getThirdPartyComponents NOTIFY languageChanged FINAL)

    Q_PROPERTY(bool elevated READ getElevated CONSTANT FINAL)
    Q_PROPERTY(bool canElevate READ getCanElevate CONSTANT FINAL)
    Q_PROPERTY(QString elevationHint READ getElevationHint NOTIFY languageChanged FINAL)

    Q_PROPERTY(int defaultWindowWidth READ getDefaultWindowWidth CONSTANT FINAL)
    Q_PROPERTY(int defaultWindowHeight READ getDefaultWindowHeight CONSTANT FINAL)
    Q_PROPERTY(int minimumWindowWidth READ getMinimumWindowWidth CONSTANT FINAL)
    Q_PROPERTY(int minimumWindowHeight READ getMinimumWindowHeight CONSTANT FINAL)

public:
    AppViewModel();

    void resetToDefault() override;
    void onLanguageChanged() override;

    QString getApplicationName() const;
    QString getVersion() const;
    QString getAuthorName() const;
    QString getAuthorEmail() const;
    QString getHomepage() const;
    QString getLicenseName() const;
    QString getQtVersion() const;
    QString getPlatformName() const;
    QString getLogsDirectoryPath() const;

    QString getDescription() const;
    QString getCopyright() const;
    QVariantList getThirdPartyComponents() const;

    bool getElevated() const;
    bool getCanElevate() const;
    QString getElevationHint() const;

    int getDefaultWindowWidth() const;
    int getDefaultWindowHeight() const;
    int getMinimumWindowWidth() const;
    int getMinimumWindowHeight() const;

    // Restarts the app with the rights raw device access needs. The window closes itself when this
    // returns true, so the two instances never share a device.
    Q_INVOKABLE bool requestElevation();

    Q_INVOKABLE void openUrl(const QString& url) const;
    Q_INVOKABLE void openLogsDirectory() const;
    Q_INVOKABLE void copyToClipboard(const QString& text) const;

Q_SIGNALS:
    void languageChanged();
};
} // namespace UDI
