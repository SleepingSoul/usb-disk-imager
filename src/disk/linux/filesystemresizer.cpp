#include <disk/filesystemresizer.hpp>

#include <QProcess>
#include <QStandardPaths>


namespace UDI
{
using namespace Qt::Literals::StringLiterals;

namespace
{
// losetup and dumpe2fs only attach or inspect, so killing one cannot damage a filesystem; this only has
// to be long enough to tell a stalled tool from a slow one.
constexpr int SetupToolTimeoutMilliseconds = 60000;

// e2fsck and resize2fs rewrite the filesystem in place, and killing one part-way can leave it half
// migrated — far worse than any wait. They are waited on without a deadline, however long a large or
// slow medium takes, which is also why a resize is not cancellable once started.
constexpr int NoTimeoutMilliseconds = -1;

const QLatin1StringView LosetupExecutable{ "losetup" };
const QLatin1StringView E2fsckExecutable{ "e2fsck" };
const QLatin1StringView Resize2fsExecutable{ "resize2fs" };
const QLatin1StringView Dumpe2fsExecutable{ "dumpe2fs" };

bool allToolsFound()
{
    return !QStandardPaths::findExecutable(LosetupExecutable).isEmpty()
        && !QStandardPaths::findExecutable(E2fsckExecutable).isEmpty()
        && !QStandardPaths::findExecutable(Resize2fsExecutable).isEmpty()
        && !QStandardPaths::findExecutable(Dumpe2fsExecutable).isEmpty();
}

// Runs \a executable to completion and reports whether it finished within \a maxAcceptableExitCode,
// merging stdout and stderr into \a output for use in an error message. e2fsck's "errors were corrected"
// exit code of 1 is the only place a caller passes anything other than 0. \a timeoutMilliseconds is
// NoTimeoutMilliseconds for the tools that must never be killed.
bool runTool(const QLatin1StringView& executable,
    const QStringList& arguments,
    int timeoutMilliseconds,
    int maxAcceptableExitCode,
    QString& output)
{
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(executable, arguments);

    if (!process.waitForFinished(timeoutMilliseconds))
    {
        process.kill();
        output = QCoreApplication::translate("FilesystemResizer", "%1 did not finish in time.").arg(executable);
        return false;
    }

    output = QString::fromLocal8Bit(process.readAll()).trimmed();

    return process.exitStatus() == QProcess::NormalExit && process.exitCode() <= maxAcceptableExitCode;
}

// Attaches a loop device covering exactly one partition's byte range and detaches it again once this
// goes out of scope, so every early return in the functions below still frees the loop device.
class LoopDeviceGuard
{
    Q_DISABLE_COPY_MOVE(LoopDeviceGuard)
public:
    LoopDeviceGuard() = default;

    ~LoopDeviceGuard()
    {
        QString ignoredErrorMessage;
        detach(ignoredErrorMessage);
    }

    bool attach(const QString& path, quint64 offsetBytes, quint64 sizeBytes, QString& errorMessage)
    {
        QProcess process;
        process.start(LosetupExecutable, QStringList{
            QStringLiteral("--find"), QStringLiteral("--show"),
            QStringLiteral("--offset"), QString::number(offsetBytes),
            QStringLiteral("--sizelimit"), QString::number(sizeBytes),
            path });

        if (!process.waitForFinished(SetupToolTimeoutMilliseconds))
        {
            process.kill();
            errorMessage = QCoreApplication::translate("FilesystemResizer", "losetup did not finish in time.");
            return false;
        }

        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        {
            errorMessage = QCoreApplication::translate("FilesystemResizer", "Could not attach a loop device: %1")
                .arg(QString::fromLocal8Bit(process.readAllStandardError()).trimmed());
            return false;
        }

        m_devicePath = QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();

        if (m_devicePath.isEmpty())
        {
            errorMessage = QCoreApplication::translate("FilesystemResizer", "losetup did not report a loop device.");
            return false;
        }

        return true;
    }

    // A checked step rather than only a destructor side effect: the caller rewrites and truncates the
    // backing file once this returns, which must not happen while the mapping onto it is still live.
    bool detach(QString& errorMessage)
    {
        if (m_devicePath.isEmpty())
        {
            return true;
        }

        QString output;

        if (!runTool(LosetupExecutable, QStringList{ QStringLiteral("-d"), m_devicePath },
                SetupToolTimeoutMilliseconds, 0, output))
        {
            errorMessage = QCoreApplication::translate("FilesystemResizer",
                "Could not detach the loop device %1: %2").arg(m_devicePath, output);
            return false;
        }

        m_devicePath.clear();

        return true;
    }

    const QString& devicePath() const { return m_devicePath; }

private:
    QString m_devicePath;
};

// A resize refuses to run against a filesystem it has not just checked cleanly.
bool checkFilesystem(const QString& devicePath, QString& errorMessage)
{
    QString output;

    if (!runTool(E2fsckExecutable, QStringList{ QStringLiteral("-f"), QStringLiteral("-y"), devicePath },
            NoTimeoutMilliseconds, 1, output))
    {
        errorMessage = QCoreApplication::translate("FilesystemResizer",
            "The filesystem check before resizing failed: %1").arg(output);
        return false;
    }

    return true;
}

std::optional<quint64> readBlockCount(const QString& devicePath, quint32& blockSizeBytes, QString& errorMessage)
{
    QProcess process;
    process.start(Dumpe2fsExecutable, QStringList{ QStringLiteral("-h"), devicePath });

    if (!process.waitForFinished(SetupToolTimeoutMilliseconds))
    {
        process.kill();
        errorMessage = QCoreApplication::translate("FilesystemResizer", "dumpe2fs did not finish in time.");
        return std::nullopt;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
    {
        errorMessage = QCoreApplication::translate("FilesystemResizer",
            "Could not read the resized filesystem's size: %1")
            .arg(QString::fromLocal8Bit(process.readAllStandardError()).trimmed());
        return std::nullopt;
    }

    std::optional<quint64> blockCount;
    blockSizeBytes = 0;

    const QString listing = QString::fromLocal8Bit(process.readAllStandardOutput());

    for (const QString& line : listing.split(u'\n'))
    {
        if (line.startsWith("Block count:"_L1))
        {
            blockCount = line.section(u':', 1).trimmed().toULongLong();
        }
        else if (line.startsWith("Block size:"_L1))
        {
            blockSizeBytes = line.section(u':', 1).trimmed().toUInt();
        }
    }

    if (!blockCount || blockSizeBytes == 0)
    {
        errorMessage = QCoreApplication::translate("FilesystemResizer",
            "Could not parse the resized filesystem's block count from dumpe2fs.");
        return std::nullopt;
    }

    return blockCount;
}
} // namespace

bool FilesystemResizer::isSupported()
{
    return allToolsFound();
}

QString FilesystemResizer::getUnsupportedReason()
{
    return allToolsFound()
        ? QString{}
        : tr("Needs e2fsprogs (e2fsck, resize2fs, dumpe2fs) — install it with your package manager.");
}

std::optional<FilesystemResizer::ShrinkResult> FilesystemResizer::shrinkFilesystem(const QString& path,
    quint64 partitionStartSector,
    quint64 partitionSectorCount,
    quint32 sectorSizeBytes,
    QString& errorMessage)
{
    if (!allToolsFound())
    {
        errorMessage = getUnsupportedReason();
        return std::nullopt;
    }

    LoopDeviceGuard loopDevice;

    if (!loopDevice.attach(path,
            partitionStartSector * sectorSizeBytes,
            partitionSectorCount * sectorSizeBytes,
            errorMessage))
    {
        return std::nullopt;
    }

    if (!checkFilesystem(loopDevice.devicePath(), errorMessage))
    {
        return std::nullopt;
    }

    QString resizeOutput;

    if (!runTool(Resize2fsExecutable, QStringList{ QStringLiteral("-M"), loopDevice.devicePath() },
            NoTimeoutMilliseconds, 0, resizeOutput))
    {
        errorMessage = tr("resize2fs could not shrink the filesystem: %1").arg(resizeOutput);
        return std::nullopt;
    }

    quint32 blockSizeBytes = 0;
    const auto blockCount = readBlockCount(loopDevice.devicePath(), blockSizeBytes, errorMessage);

    if (!blockCount)
    {
        return std::nullopt;
    }

    if (!loopDevice.detach(errorMessage))
    {
        return std::nullopt;
    }

    const quint64 newFilesystemSizeBytes = *blockCount * static_cast<quint64>(blockSizeBytes);
    const quint64 newPartitionSectorCount = (newFilesystemSizeBytes + sectorSizeBytes - 1) / sectorSizeBytes;

    return ShrinkResult{ newPartitionSectorCount };
}

bool FilesystemResizer::growFilesystem(const QString& path,
    quint64 partitionStartSector,
    quint64 partitionSectorCount,
    quint32 sectorSizeBytes,
    QString& errorMessage)
{
    if (!allToolsFound())
    {
        errorMessage = getUnsupportedReason();
        return false;
    }

    LoopDeviceGuard loopDevice;

    if (!loopDevice.attach(path,
            partitionStartSector * sectorSizeBytes,
            partitionSectorCount * sectorSizeBytes,
            errorMessage))
    {
        return false;
    }

    if (!checkFilesystem(loopDevice.devicePath(), errorMessage))
    {
        return false;
    }

    QString resizeOutput;

    // No target size: resize2fs fills whatever the loop device's --sizelimit already made available.
    if (!runTool(Resize2fsExecutable, QStringList{ loopDevice.devicePath() },
            NoTimeoutMilliseconds, 0, resizeOutput))
    {
        errorMessage = tr("resize2fs could not grow the filesystem: %1").arg(resizeOutput);
        return false;
    }

    return loopDevice.detach(errorMessage);
}
} // namespace UDI
