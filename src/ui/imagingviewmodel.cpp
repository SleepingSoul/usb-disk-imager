#include <ui/imagingviewmodel.hpp>

#include <algorithm>

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QUrl>

#include <managers/imagingmanager.hpp>
#include <ui/devicelistmodel.hpp>
#include <utils/formatting.hpp>
#include <utils/privileges.hpp>
#include <utils/usersettings.hpp>


namespace UDI
{
using namespace Qt::Literals::StringLiterals;

namespace
{
const QLatin1StringView ImageFileExtension{ ".img" };
const QLatin1StringView FileUrlScheme{ "file:" };
// A QString rather than a QLatin1StringView: an em dash is multi-byte UTF-8.
const QString EmptyValuePlaceholder = QStringLiteral("—");

QString sanitizeForFileName(const QString& text)
{
    QString sanitized;
    sanitized.reserve(text.size());

    for (const QChar character : text)
    {
        sanitized.append(character.isLetterOrNumber() ? character : u'-');
    }

    while (sanitized.contains("--"_L1))
    {
        sanitized.replace("--"_L1, "-"_L1);
    }

    return sanitized.isEmpty() ? QStringLiteral("device") : sanitized;
}
} // namespace

ImagingViewModel::ImagingViewModel(DeviceListModel* deviceListModel)
    : m_deviceListModel(deviceListModel)
{}

void ImagingViewModel::resetToDefault()
{
    const QByteArray storedTrimMode = UserSettings::getTrimModeToken();

    m_trimMode = QString{ trimModeToken(trimModeFromToken(storedTrimMode)) };
    m_verifyAfterWrite = UserSettings::getVerifyAfterWrite();
    m_computeSha256 = UserSettings::getComputeSha256();

    m_busy = false;
    m_cancelling = false;
    m_operation = ImagingOperation::None;
    m_progress = ImagingProgress{};
    m_result = ImagingResult{};
    m_hasResult = false;

    Q_EMIT optionsChanged();
    Q_EMIT imageFileChanged();
    Q_EMIT stateChanged();
    Q_EMIT progressChanged();
    Q_EMIT resultChanged();
}

void ImagingViewModel::setImageFilePath(const QString& imageFilePath)
{
    // A QML FileDialog hands over a URL while a dropped or typed path is already local.
    const QString localPath = imageFilePath.startsWith(FileUrlScheme)
        ? QUrl{ imageFilePath }.toLocalFile()
        : imageFilePath;

    if (localPath == m_imageFilePath)
    {
        return;
    }

    m_imageFilePath = localPath;

    if (!localPath.isEmpty())
    {
        UserSettings::setLastImageDirectory(QFileInfo{ localPath }.absolutePath());
    }

    Q_EMIT imageFileChanged();
}

QString ImagingViewModel::getImageFileName() const
{
    return m_imageFilePath.isEmpty() ? QString{} : QFileInfo{ m_imageFilePath }.fileName();
}

QString ImagingViewModel::getImageSizeText() const
{
    const QFileInfo imageInfo{ m_imageFilePath };

    if (m_imageFilePath.isEmpty() || !imageInfo.exists() || !imageInfo.isFile())
    {
        return QString{};
    }

    return formatByteSize(static_cast<quint64>(imageInfo.size()));
}

bool ImagingViewModel::getImageFileExists() const
{
    const QFileInfo imageInfo{ m_imageFilePath };

    return !m_imageFilePath.isEmpty() && imageInfo.exists() && imageInfo.isFile() && imageInfo.size() > 0;
}

void ImagingViewModel::setTrimMode(const QString& trimMode)
{
    if (trimMode == m_trimMode)
    {
        return;
    }

    m_trimMode = trimMode;
    UserSettings::setTrimModeToken(trimMode.toUtf8());

    Q_EMIT optionsChanged();
}

void ImagingViewModel::setVerifyAfterWrite(bool verifyAfterWrite)
{
    if (verifyAfterWrite == m_verifyAfterWrite)
    {
        return;
    }

    m_verifyAfterWrite = verifyAfterWrite;
    UserSettings::setVerifyAfterWrite(verifyAfterWrite);

    Q_EMIT optionsChanged();
}

void ImagingViewModel::setComputeSha256(bool computeSha256)
{
    if (computeSha256 == m_computeSha256)
    {
        return;
    }

    m_computeSha256 = computeSha256;
    UserSettings::setComputeSha256(computeSha256);

    Q_EMIT optionsChanged();
}

QString ImagingViewModel::getOperationToken() const
{
    switch (m_operation)
    {
    case ImagingOperation::Read:   return QStringLiteral("read");
    case ImagingOperation::Write:  return QStringLiteral("write");
    case ImagingOperation::Verify: return QStringLiteral("verify");
    case ImagingOperation::None:   break;
    }

    return QStringLiteral("none");
}

double ImagingViewModel::getProgressFraction() const
{
    if (m_progress.totalBytes == 0)
    {
        return 0.0;
    }

    const double fraction =
        static_cast<double>(m_progress.processedBytes) / static_cast<double>(m_progress.totalBytes);

    return std::clamp(fraction, 0.0, 1.0);
}

QString ImagingViewModel::getProgressPercentText() const
{
    return QStringLiteral("%1%").arg(QString::number(getProgressFraction() * 100.0, 'f', 1));
}

QString ImagingViewModel::getStageText() const
{
    switch (m_progress.stage)
    {
    case ImagingStage::Preparing:    return tr("Preparing");
    case ImagingStage::Unmounting:   return tr("Taking the device offline");
    case ImagingStage::Analyzing:    return tr("Looking for the end of the data");
    case ImagingStage::Transferring: return m_operation == ImagingOperation::Write ? tr("Writing") : tr("Reading");
    case ImagingStage::Verifying:    return tr("Verifying");
    case ImagingStage::Finalizing:   return tr("Flushing to the medium");
    case ImagingStage::Complete:     return tr("Done");
    case ImagingStage::Failed:       return tr("Failed");
    case ImagingStage::Cancelled:    return tr("Cancelled");
    case ImagingStage::Idle:         break;
    }

    return tr("Idle");
}

QString ImagingViewModel::getProcessedSizeText() const
{
    return formatByteSize(m_progress.processedBytes);
}

QString ImagingViewModel::getTotalSizeText() const
{
    return m_progress.totalBytes > 0 ? formatByteSize(m_progress.totalBytes) : EmptyValuePlaceholder;
}

QString ImagingViewModel::getTransferRateText() const
{
    return formatTransferRate(m_progress.bytesPerSecond);
}

QString ImagingViewModel::getRemainingTimeText() const
{
    return m_progress.remainingMilliseconds >= 0
        ? formatDuration(m_progress.remainingMilliseconds)
        : EmptyValuePlaceholder;
}

QString ImagingViewModel::getElapsedTimeText() const
{
    return formatDuration(m_progress.elapsedMilliseconds);
}

QString ImagingViewModel::getResultTitle() const
{
    if (!m_hasResult)
    {
        return QString{};
    }

    if (m_result.cancelled)
    {
        return tr("Cancelled");
    }

    if (!m_result.succeeded)
    {
        return tr("Something went wrong");
    }

    switch (m_result.operation)
    {
    case ImagingOperation::Read:   return tr("Image saved");
    case ImagingOperation::Write:  return tr("Device written");
    case ImagingOperation::Verify: return tr("Device matches the image");
    case ImagingOperation::None:   break;
    }

    return tr("Done");
}

QString ImagingViewModel::getResultMessage() const
{
    if (!m_hasResult)
    {
        return QString{};
    }

    if (!m_result.errorMessage.isEmpty())
    {
        return m_result.errorMessage;
    }

    if (!m_result.succeeded)
    {
        return QString{};
    }

    const QString amount = formatByteSize(m_result.processedBytes);
    const QString duration = formatDuration(m_result.elapsedMilliseconds);

    switch (m_result.operation)
    {
    case ImagingOperation::Read:
        return tr("%1 read in %2 to “%3”.").arg(amount, duration, m_result.imageFilePath);
    case ImagingOperation::Write:
        return m_result.verificationMatched
            ? tr("%1 written and verified in %2.").arg(amount, duration)
            : tr("%1 written in %2.").arg(amount, duration);
    case ImagingOperation::Verify:
        return tr("%1 compared in %2 — every byte matches.").arg(amount, duration);
    case ImagingOperation::None:
        break;
    }

    return QString{};
}

QString ImagingViewModel::getResultFastDigest() const
{
    return QString::fromLatin1(m_result.fastDigestHex);
}

QString ImagingViewModel::getResultSha256Digest() const
{
    return QString::fromLatin1(m_result.sha256Hex);
}

void ImagingViewModel::startRead()
{
    startOperation(ImagingOperation::Read);
}

void ImagingViewModel::startWrite()
{
    startOperation(ImagingOperation::Write);
}

void ImagingViewModel::startVerify()
{
    startOperation(ImagingOperation::Verify);
}

void ImagingViewModel::cancel()
{
    if (!m_busy)
    {
        return;
    }

    m_cancelling = true;

    Q_EMIT stateChanged();

    getManager<ImagingManager>().requestCancel();
}

void ImagingViewModel::clearResult()
{
    if (!m_hasResult)
    {
        return;
    }

    m_hasResult = false;
    m_result = ImagingResult{};
    m_progress = ImagingProgress{};

    Q_EMIT resultChanged();
    Q_EMIT progressChanged();
}

QUrl ImagingViewModel::suggestImageFileUrl() const
{
    const DeviceInfo* const device = m_deviceListModel ? m_deviceListModel->getSelectedDevice() : nullptr;

    const QString deviceName = device
        ? sanitizeForFileName(device->getProductName())
        : QStringLiteral("device");
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmm"));
    const QString suggestedPath = QDir{ UserSettings::getLastImageDirectory() }
        .filePath(deviceName + u'-' + timestamp + ImageFileExtension);

    return QUrl::fromLocalFile(suggestedPath);
}

QUrl ImagingViewModel::getImageDirectoryUrl() const
{
    const QString directoryPath = m_imageFilePath.isEmpty()
        ? UserSettings::getLastImageDirectory()
        : QFileInfo{ m_imageFilePath }.absolutePath();

    return QUrl::fromLocalFile(directoryPath);
}

void ImagingViewModel::showImageInFileManager() const
{
    if (m_imageFilePath.isEmpty())
    {
        return;
    }

    const QString directoryPath = QFileInfo{ m_imageFilePath }.absolutePath();

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(directoryPath)))
    {
        qWarning() << "Could not open" << directoryPath << "in the file manager";
    }
}

void ImagingViewModel::startOperation(ImagingOperation operation)
{
    if (m_busy)
    {
        return;
    }

    const DeviceInfo* const device = m_deviceListModel ? m_deviceListModel->getSelectedDevice() : nullptr;

    if (!device)
    {
        reportRefusal(operation, tr("Select a device first."));
        return;
    }

    if (m_imageFilePath.isEmpty())
    {
        reportRefusal(operation, operation == ImagingOperation::Read
            ? tr("Choose where the image should be saved.")
            : tr("Choose an image file first."));
        return;
    }

    if (operation != ImagingOperation::Read && !getImageFileExists())
    {
        reportRefusal(operation, tr("“%1” does not exist or is empty.").arg(m_imageFilePath));
        return;
    }

    if (operation == ImagingOperation::Write && device->systemDevice)
    {
        reportRefusal(operation, tr("%1 holds the running operating system and will not be written to.")
            .arg(QString::fromUtf8(device->path)));
        return;
    }

    if (!Privileges::isElevated())
    {
        reportRefusal(operation, Privileges::getElevationHint());
        return;
    }

    ImagingRequest request;
    request.operation = operation;
    request.device = *device;
    request.imageFilePath = m_imageFilePath;
    request.trimMode = trimModeFromToken(m_trimMode.toUtf8());
    request.verifyAfterWrite = m_verifyAfterWrite;
    request.computeSha256 = m_computeSha256;

    m_hasResult = false;
    m_result = ImagingResult{};

    Q_EMIT resultChanged();
    Q_EMIT imagingRequested(request);
}

void ImagingViewModel::reportRefusal(ImagingOperation operation, const QString& message)
{
    m_result = ImagingResult{};
    m_result.operation = operation;
    m_result.finalStage = ImagingStage::Failed;
    m_result.errorMessage = message;
    m_hasResult = true;

    // The page showing the result panel picks itself by operation, so a refusal has to name the operation
    // that was refused even though nothing started.
    m_operation = operation;

    qWarning() << "Refusing to start:" << message;

    Q_EMIT stateChanged();
    Q_EMIT resultChanged();
}

void ImagingViewModel::onOperationStarted(const UDI::ImagingRequest& request)
{
    m_busy = true;
    m_cancelling = false;
    m_operation = request.operation;
    m_progress = ImagingProgress{};
    m_progress.stage = ImagingStage::Preparing;

    Q_EMIT stateChanged();
    Q_EMIT progressChanged();
}

void ImagingViewModel::onProgressUpdated(const UDI::ImagingProgress& progress)
{
    m_progress = progress;

    Q_EMIT progressChanged();
}

void ImagingViewModel::onOperationFinished(const UDI::ImagingResult& result)
{
    m_busy = false;
    m_cancelling = false;
    m_result = result;
    m_hasResult = true;
    m_progress.stage = result.finalStage;

    Q_EMIT stateChanged();
    Q_EMIT progressChanged();
    Q_EMIT resultChanged();

    // A finished write leaves a different filesystem behind, so the device list has to be re-read.
    if (m_deviceListModel)
    {
        m_deviceListModel->refresh();
    }
}

void ImagingViewModel::onLanguageChanged()
{
    // The stage and result texts were translated when they were built, so they have to be rebuilt.
    Q_EMIT progressChanged();
    Q_EMIT resultChanged();
}
} // namespace UDI
