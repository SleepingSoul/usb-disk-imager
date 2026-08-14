#pragma once

#include <QByteArray>
#include <QString>


namespace UDI
{
// Choices that have to survive a restart. Everything here is a user preference; anything a developer
// tunes lives in the managers' JSON configs instead.
class UserSettings
{
public:
    static QString getLanguageCode();
    static void setLanguageCode(const QString& languageCode);

    static bool getIncludeFixedDisks();
    static void setIncludeFixedDisks(bool includeFixedDisks);

    static QByteArray getTrimModeToken();
    static void setTrimModeToken(const QByteArray& trimModeToken);

    static bool getVerifyAfterWrite();
    static void setVerifyAfterWrite(bool verifyAfterWrite);

    static bool getComputeSha256();
    static void setComputeSha256(bool computeSha256);

    static QString getLastImageDirectory();
    static void setLastImageDirectory(const QString& directoryPath);
};
} // namespace UDI
