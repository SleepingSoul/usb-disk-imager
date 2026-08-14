#include <ui/appviewmodel.hpp>

#include <QClipboard>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QSysInfo>
#include <QUrl>
#include <QVariantMap>

#include <managers/loggingmanager.hpp>
#include <managers/uimanager.hpp>
#include <utils/buildinfo.hpp>
#include <utils/privileges.hpp>


namespace UDI
{
using namespace Qt::Literals::StringLiterals;

namespace
{
const QLatin1StringView QtHomepage{ "https://www.qt.io" };
const QLatin1StringView XxHashHomepage{ "https://github.com/Cyan4973/xxHash" };
const QLatin1StringView SpdlogHomepage{ "https://github.com/gabime/spdlog" };

QVariantMap makeComponent(const QString& name,
    const QString& version,
    const QString& license,
    const QString& homepage,
    const QString& purpose)
{
    QVariantMap component;
    component[QStringLiteral("name")] = name;
    component[QStringLiteral("version")] = version;
    component[QStringLiteral("license")] = license;
    component[QStringLiteral("homepage")] = homepage;
    component[QStringLiteral("purpose")] = purpose;

    return component;
}
} // namespace

AppViewModel::AppViewModel() = default;

void AppViewModel::resetToDefault()
{
    Q_EMIT languageChanged();
}

QString AppViewModel::getApplicationName() const
{
    return QString{ BuildInfo::ApplicationName };
}

QString AppViewModel::getVersion() const
{
    return QString{ BuildInfo::Version };
}

QString AppViewModel::getAuthorName() const
{
    return QString{ BuildInfo::AuthorName };
}

QString AppViewModel::getAuthorEmail() const
{
    return QString{ BuildInfo::AuthorEmail };
}

QString AppViewModel::getHomepage() const
{
    return QString{ BuildInfo::Homepage };
}

QString AppViewModel::getLicenseName() const
{
    return QString{ BuildInfo::LicenseName };
}

QString AppViewModel::getQtVersion() const
{
    return QString::fromLatin1(qVersion());
}

QString AppViewModel::getPlatformName() const
{
    return QStringLiteral("%1 %2").arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture());
}

QString AppViewModel::getLogsDirectoryPath() const
{
    return getManager<LoggingManager>().getLogsDirectoryPath();
}

QString AppViewModel::getDescription() const
{
    return tr("Reads a USB drive or memory card into an image file and writes an image file back, "
              "byte for byte.");
}

QString AppViewModel::getCopyright() const
{
    return tr("© %1 %2").arg(QString{ BuildInfo::CopyrightYear }, QString{ BuildInfo::AuthorName });
}

QVariantList AppViewModel::getThirdPartyComponents() const
{
    QVariantList components;

    components.push_back(makeComponent(QStringLiteral("Qt"),
        getQtVersion(),
        QStringLiteral("LGPL v3"),
        QString{ QtHomepage },
        tr("Application framework, QML user interface and translations")));

    components.push_back(makeComponent(QStringLiteral("xxHash"),
        QString{ BuildInfo::XxHashVersion },
        QStringLiteral("BSD 2-Clause"),
        QString{ XxHashHomepage },
        tr("Fast integrity digests of images and devices")));

    components.push_back(makeComponent(QStringLiteral("spdlog"),
        QString{ BuildInfo::SpdlogVersion },
        QStringLiteral("MIT"),
        QString{ SpdlogHomepage },
        tr("Rotating log files behind Qt's message handler")));

    return components;
}

bool AppViewModel::getElevated() const
{
    return Privileges::isElevated();
}

bool AppViewModel::getCanElevate() const
{
    return Privileges::canElevate();
}

QString AppViewModel::getElevationHint() const
{
    return Privileges::getElevationHint();
}

int AppViewModel::getDefaultWindowWidth() const
{
    return getManager<UiManager>().getConfig().defaultWindowWidth;
}

int AppViewModel::getDefaultWindowHeight() const
{
    return getManager<UiManager>().getConfig().defaultWindowHeight;
}

int AppViewModel::getMinimumWindowWidth() const
{
    return getManager<UiManager>().getConfig().minimumWindowWidth;
}

int AppViewModel::getMinimumWindowHeight() const
{
    return getManager<UiManager>().getConfig().minimumWindowHeight;
}

bool AppViewModel::requestElevation()
{
    return Privileges::relaunchElevated();
}

void AppViewModel::openUrl(const QString& url) const
{
    if (!QDesktopServices::openUrl(QUrl{ url }))
    {
        qWarning() << "Could not open" << url;
    }
}

void AppViewModel::openLogsDirectory() const
{
    const QString logsDirectoryPath = getLogsDirectoryPath();

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(logsDirectoryPath)))
    {
        qWarning() << "Could not open the logs directory" << logsDirectoryPath;
    }
}

void AppViewModel::copyToClipboard(const QString& text) const
{
    if (QClipboard* const clipboard = QGuiApplication::clipboard())
    {
        clipboard->setText(text);
    }
}

void AppViewModel::onLanguageChanged()
{
    Q_EMIT languageChanged();
}
} // namespace UDI
