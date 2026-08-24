#include <utils/usersettings.hpp>

#include <QSettings>
#include <QStandardPaths>


namespace UDI
{
namespace
{
const QLatin1StringView LanguageKey{ "language" };
const QLatin1StringView IncludeFixedDisksKey{ "devices/includeFixedDisks" };
const QLatin1StringView TrimModeKey{ "imaging/trimMode" };
const QLatin1StringView VerifyAfterWriteKey{ "imaging/verifyAfterWrite" };
const QLatin1StringView ComputeSha256Key{ "imaging/computeSha256" };
const QLatin1StringView SkipTrailingZerosOnWriteKey{ "imaging/skipTrailingZerosOnWrite" };
const QLatin1StringView ShrinkFilesystemAfterReadKey{ "imaging/shrinkFilesystemAfterRead" };
const QLatin1StringView GrowFilesystemToFillDeviceKey{ "imaging/growFilesystemToFillDevice" };
const QLatin1StringView LastImageDirectoryKey{ "imaging/lastImageDirectory" };

/*
 * Deliberately a fresh instance per call rather than one long-lived static.
 *
 * A QSettings that outlives QCoreApplication is destroyed during static destruction, after the
 * application object is gone, and its final sync() then runs without the organisation and application
 * names it was built from — which takes the process down on the way out. Settings are only touched when
 * the user changes something, so constructing one per access costs nothing worth keeping a static for.
 */
QSettings openSettings()
{
    return QSettings{};
}
} // namespace

QString UserSettings::getLanguageCode()
{
    return openSettings().value(LanguageKey).toString();
}

void UserSettings::setLanguageCode(const QString& languageCode)
{
    openSettings().setValue(LanguageKey, languageCode);
}

bool UserSettings::getIncludeFixedDisks()
{
    return openSettings().value(IncludeFixedDisksKey, false).toBool();
}

void UserSettings::setIncludeFixedDisks(bool includeFixedDisks)
{
    openSettings().setValue(IncludeFixedDisksKey, includeFixedDisks);
}

QByteArray UserSettings::getTrimModeToken()
{
    return openSettings().value(TrimModeKey).toByteArray();
}

void UserSettings::setTrimModeToken(const QByteArray& trimModeToken)
{
    openSettings().setValue(TrimModeKey, trimModeToken);
}

bool UserSettings::getVerifyAfterWrite()
{
    return openSettings().value(VerifyAfterWriteKey, true).toBool();
}

void UserSettings::setVerifyAfterWrite(bool verifyAfterWrite)
{
    openSettings().setValue(VerifyAfterWriteKey, verifyAfterWrite);
}

bool UserSettings::getComputeSha256()
{
    return openSettings().value(ComputeSha256Key, false).toBool();
}

void UserSettings::setComputeSha256(bool computeSha256)
{
    openSettings().setValue(ComputeSha256Key, computeSha256);
}

bool UserSettings::getSkipTrailingZerosOnWrite()
{
    return openSettings().value(SkipTrailingZerosOnWriteKey, false).toBool();
}

void UserSettings::setSkipTrailingZerosOnWrite(bool skipTrailingZerosOnWrite)
{
    openSettings().setValue(SkipTrailingZerosOnWriteKey, skipTrailingZerosOnWrite);
}

bool UserSettings::getShrinkFilesystemAfterRead()
{
    return openSettings().value(ShrinkFilesystemAfterReadKey, false).toBool();
}

void UserSettings::setShrinkFilesystemAfterRead(bool shrinkFilesystemAfterRead)
{
    openSettings().setValue(ShrinkFilesystemAfterReadKey, shrinkFilesystemAfterRead);
}

bool UserSettings::getGrowFilesystemToFillDevice()
{
    return openSettings().value(GrowFilesystemToFillDeviceKey, false).toBool();
}

void UserSettings::setGrowFilesystemToFillDevice(bool growFilesystemToFillDevice)
{
    openSettings().setValue(GrowFilesystemToFillDeviceKey, growFilesystemToFillDevice);
}

QString UserSettings::getLastImageDirectory()
{
    const QString stored = openSettings().value(LastImageDirectoryKey).toString();

    if (!stored.isEmpty())
    {
        return stored;
    }

    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

void UserSettings::setLastImageDirectory(const QString& directoryPath)
{
    openSettings().setValue(LastImageDirectoryKey, directoryPath);
}
} // namespace UDI
