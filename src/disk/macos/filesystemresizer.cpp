#include <disk/filesystemresizer.hpp>

#include <array>

#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QXmlStreamReader>

#include <fcntl.h>
#include <sys/disk.h>
#include <sys/ioctl.h>
#include <unistd.h>


namespace UDI
{
using namespace Qt::Literals::StringLiterals;

namespace
{
// hdiutil and dumpe2fs only attach or inspect, so killing one cannot damage a filesystem; this only has
// to be long enough to tell a stalled tool from a slow one.
constexpr int SetupToolTimeoutMilliseconds = 60000;

// e2fsck and resize2fs rewrite the filesystem in place, and killing one part-way can leave it half
// migrated — far worse than any wait. They are waited on without a deadline, however long a large or
// slow medium takes, which is also why a resize is not cancellable once started.
constexpr int NoTimeoutMilliseconds = -1;

const QLatin1StringView HdiutilExecutable{ "hdiutil" };

// Homebrew's e2fsprogs is keg-only, since macOS ships its own (much older) fsck under the same names, so
// its binaries never land on the PATH a GUI app inherits from launchd — every plausible install location
// is checked directly rather than relying on PATH alone.
QString findTool(const QLatin1StringView& name)
{
    const QString onPath = QStandardPaths::findExecutable(QString(name));

    if (!onPath.isEmpty())
    {
        return onPath;
    }

    const std::array<QLatin1StringView, 4> prefixes{
        "/opt/homebrew/opt/e2fsprogs/sbin/"_L1,
        "/opt/homebrew/opt/e2fsprogs/bin/"_L1,
        "/usr/local/opt/e2fsprogs/sbin/"_L1,
        "/usr/local/opt/e2fsprogs/bin/"_L1
    };

    for (const auto& prefix : prefixes)
    {
        const QString candidate = QString(prefix) + QString(name);

        if (QFile::exists(candidate))
        {
            return candidate;
        }
    }

    return QString{};
}

bool allToolsFound()
{
    return !findTool("e2fsck"_L1).isEmpty() && !findTool("resize2fs"_L1).isEmpty()
        && !findTool("dumpe2fs"_L1).isEmpty();
}

// \a timeoutMilliseconds is NoTimeoutMilliseconds for the tools that must never be killed.
bool runTool(const QString& executable,
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

// Reads the block count directly from the device node via the same ioctls disk/unix/rawdevice.cpp uses,
// rather than parsing a CLI tool's formatted text — this is the safety check that lets AttachedImage::
// attach() refuse to proceed instead of guessing when hdiutil's partition order does not line up as
// expected.
std::optional<quint64> readBlockDeviceSizeBytes(const QString& devicePath)
{
    const int fileDescriptor = ::open(devicePath.toLocal8Bit().constData(), O_RDONLY);

    if (fileDescriptor < 0)
    {
        return std::nullopt;
    }

    quint64 blockCount = 0;
    quint32 blockSizeBytes = 0;

    const bool ok = ::ioctl(fileDescriptor, DKIOCGETBLOCKCOUNT, &blockCount) == 0
        && ::ioctl(fileDescriptor, DKIOCGETBLOCKSIZE, &blockSizeBytes) == 0
        && blockCount > 0 && blockSizeBytes > 0;

    ::close(fileDescriptor);

    return ok ? std::make_optional(blockCount * blockSizeBytes) : std::nullopt;
}

// hdiutil attach's plist lists the whole-disk scheme first, then one entry per partition in table order.
// Collects every "dev-entry" string in that order.
QStringList parseAttachedDeviceNodes(const QByteArray& plistXml)
{
    QStringList deviceNodes;
    QXmlStreamReader reader(plistXml);
    bool expectDevEntryValue = false;

    while (!reader.atEnd())
    {
        reader.readNext();

        if (reader.tokenType() == QXmlStreamReader::StartElement && reader.name() == "key"_L1)
        {
            expectDevEntryValue = reader.readElementText() == "dev-entry"_L1;
        }
        else if (expectDevEntryValue
            && reader.tokenType() == QXmlStreamReader::StartElement && reader.name() == "string"_L1)
        {
            deviceNodes.append(reader.readElementText());
            expectDevEntryValue = false;
        }
    }

    return deviceNodes;
}

// Attaches the whole image, then picks out whichever exposed partition device node actually reports the
// expected size — the one genuinely safe way to identify "the last partition" here, since hdiutil's own
// partition ordering is not something this app controls or can verify offline.
class AttachedImage
{
    Q_DISABLE_COPY_MOVE(AttachedImage)
public:
    AttachedImage() = default;

    ~AttachedImage()
    {
        QString ignoredErrorMessage;
        detach(ignoredErrorMessage);
    }

    // A checked step rather than only a destructor side effect: the caller rewrites and truncates the
    // backing file once this returns, which must not happen while the image is still attached.
    bool detach(QString& errorMessage)
    {
        if (m_wholeDiskNode.isEmpty())
        {
            return true;
        }

        QString output;

        if (!runTool(HdiutilExecutable, QStringList{ QStringLiteral("detach"), m_wholeDiskNode },
                SetupToolTimeoutMilliseconds, 0, output))
        {
            errorMessage = QCoreApplication::translate("FilesystemResizer",
                "Could not detach %1: %2").arg(m_wholeDiskNode, output);
            return false;
        }

        m_wholeDiskNode.clear();
        m_partitionNode.clear();

        return true;
    }

    bool attach(const QString& path, quint64 expectedPartitionSizeBytes, QString& errorMessage)
    {
        QProcess process;
        process.start(HdiutilExecutable,
            QStringList{ QStringLiteral("attach"), QStringLiteral("-nomount"), QStringLiteral("-plist"), path });

        if (!process.waitForFinished(SetupToolTimeoutMilliseconds))
        {
            process.kill();
            errorMessage = QCoreApplication::translate("FilesystemResizer", "hdiutil did not finish in time.");
            return false;
        }

        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        {
            errorMessage = QCoreApplication::translate("FilesystemResizer", "Could not attach the image: %1")
                .arg(QString::fromLocal8Bit(process.readAllStandardError()).trimmed());
            return false;
        }

        const QStringList deviceNodes = parseAttachedDeviceNodes(process.readAllStandardOutput());

        if (deviceNodes.isEmpty())
        {
            errorMessage = QCoreApplication::translate("FilesystemResizer", "hdiutil did not report any device nodes.");
            return false;
        }

        // The first node is the whole-disk scheme entry; every attached node has to be released even
        // when a matching partition is never found below.
        m_wholeDiskNode = deviceNodes.first();

        QStringList matchingNodes;

        for (const QString& deviceNode : deviceNodes)
        {
            const auto sizeBytes = readBlockDeviceSizeBytes(deviceNode);

            if (sizeBytes && *sizeBytes == expectedPartitionSizeBytes)
            {
                matchingNodes.append(deviceNode);
            }
        }

        // More than one candidate means the size alone cannot tell them apart — guessing which one is
        // the target partition is worse than refusing outright.
        if (matchingNodes.size() != 1)
        {
            errorMessage = QCoreApplication::translate("FilesystemResizer",
                "Could not uniquely identify the target partition among the devices hdiutil attached "
                "(%1 candidates matched its size).").arg(matchingNodes.size());
            return false;
        }

        m_partitionNode = matchingNodes.first();

        return true;
    }

    const QString& partitionNode() const { return m_partitionNode; }

private:
    QString m_wholeDiskNode;
    QString m_partitionNode;
};

bool checkFilesystem(const QString& devicePath, QString& errorMessage)
{
    QString output;

    if (!runTool(findTool("e2fsck"_L1), QStringList{ QStringLiteral("-f"), QStringLiteral("-y"), devicePath },
            NoTimeoutMilliseconds, 1, output))
    {
        errorMessage = QCoreApplication::translate("FilesystemResizer",
            "The filesystem check before resizing failed: %1").arg(output);
        return false;
    }

    return true;
}

// The partition device node's own size never changes — resize2fs only changes what the filesystem
// claims to use inside it — so the new size after a shrink has to come from the filesystem itself.
std::optional<quint64> readFilesystemSizeBytes(const QString& devicePath, QString& errorMessage)
{
    QProcess process;
    process.start(findTool("dumpe2fs"_L1), QStringList{ QStringLiteral("-h"), devicePath });

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
    quint64 blockSizeBytes = 0;

    const QString listing = QString::fromLocal8Bit(process.readAllStandardOutput());

    for (const QString& line : listing.split(u'\n'))
    {
        if (line.startsWith("Block count:"_L1))
        {
            blockCount = line.section(u':', 1).trimmed().toULongLong();
        }
        else if (line.startsWith("Block size:"_L1))
        {
            blockSizeBytes = line.section(u':', 1).trimmed().toULongLong();
        }
    }

    if (!blockCount || blockSizeBytes == 0)
    {
        errorMessage = QCoreApplication::translate("FilesystemResizer",
            "Could not parse the resized filesystem's block count from dumpe2fs.");
        return std::nullopt;
    }

    return *blockCount * blockSizeBytes;
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
        : tr("Needs e2fsprogs — install it with Homebrew: brew install e2fsprogs");
}

std::optional<FilesystemResizer::ShrinkResult> FilesystemResizer::shrinkFilesystem(const QString& path,
    quint64 partitionStartSector,
    quint64 partitionSectorCount,
    quint32 sectorSizeBytes,
    QString& errorMessage)
{
    Q_UNUSED(partitionStartSector)

    if (!allToolsFound())
    {
        errorMessage = getUnsupportedReason();
        return std::nullopt;
    }

    AttachedImage attached;

    if (!attached.attach(path, partitionSectorCount * sectorSizeBytes, errorMessage))
    {
        return std::nullopt;
    }

    if (!checkFilesystem(attached.partitionNode(), errorMessage))
    {
        return std::nullopt;
    }

    QString resizeOutput;

    if (!runTool(findTool("resize2fs"_L1), QStringList{ QStringLiteral("-M"), attached.partitionNode() },
            NoTimeoutMilliseconds, 0, resizeOutput))
    {
        errorMessage = tr("resize2fs could not shrink the filesystem: %1").arg(resizeOutput);
        return std::nullopt;
    }

    const auto newFilesystemSizeBytes = readFilesystemSizeBytes(attached.partitionNode(), errorMessage);

    if (!newFilesystemSizeBytes)
    {
        return std::nullopt;
    }

    if (!attached.detach(errorMessage))
    {
        return std::nullopt;
    }

    return ShrinkResult{ (*newFilesystemSizeBytes + sectorSizeBytes - 1) / sectorSizeBytes };
}

bool FilesystemResizer::growFilesystem(const QString& path,
    quint64 partitionStartSector,
    quint64 partitionSectorCount,
    quint32 sectorSizeBytes,
    QString& errorMessage)
{
    Q_UNUSED(partitionStartSector)

    if (!allToolsFound())
    {
        errorMessage = getUnsupportedReason();
        return false;
    }

    AttachedImage attached;

    if (!attached.attach(path, partitionSectorCount * sectorSizeBytes, errorMessage))
    {
        return false;
    }

    if (!checkFilesystem(attached.partitionNode(), errorMessage))
    {
        return false;
    }

    QString resizeOutput;

    if (!runTool(findTool("resize2fs"_L1), QStringList{ attached.partitionNode() },
            NoTimeoutMilliseconds, 0, resizeOutput))
    {
        errorMessage = tr("resize2fs could not grow the filesystem: %1").arg(resizeOutput);
        return false;
    }

    return attached.detach(errorMessage);
}
} // namespace UDI
