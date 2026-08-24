#include <disk/filesystemresizer.hpp>

#include <algorithm>
#include <vector>

#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>


namespace UDI
{
using namespace Qt::Literals::StringLiterals;

namespace
{
/*
 * Windows has no e2fsprogs and no way to expose a byte range of a file as its own device node, so the
 * resize runs inside WSL instead, against a copy of the partition rather than the image.
 *
 * The loop-device trick the Linux backend uses is unavailable here even through WSL: a file under
 * /mnt/c is reached over the 9p protocol, which losetup cannot map. What does work is that e2fsck and
 * resize2fs operate happily on a plain file whose first byte is the filesystem's first byte — no loop
 * device involved — so the partition is copied out to a temporary file, resized there, and copied back.
 * That costs a pass over the partition in each direction, which is the price of Windows lacking the
 * primitive the other two platforms have.
 *
 * Growing is deliberately not offered. It would need a temporary file the size of the destination
 * device and a full copy back, since resize2fs writes group descriptors across the whole new extent —
 * a much worse trade than the shrink case, where the copy back is only as large as the shrunk result.
 *
 * Docker could host the tools just as well, but it earns very little here: Docker Desktop runs on WSL2
 * by default, so a machine with Docker almost always has WSL already, and Docker additionally needs a
 * running daemon and an image that contains e2fsprogs — a network pull on first use. WSL's default
 * distribution ships e2fsprogs outright.
 */

constexpr int ProbeTimeoutMilliseconds = 30000;
constexpr int SetupToolTimeoutMilliseconds = 60000;

// e2fsck and resize2fs rewrite the filesystem in place, and killing one part-way can leave it half
// migrated — far worse than any wait.
constexpr int NoTimeoutMilliseconds = -1;

constexpr qint64 CopyChunkBytes = 4ll * 1024ll * 1024ll;

const QLatin1StringView WslExecutable{ "wsl" };

QString findWsl()
{
    return QStandardPaths::findExecutable(WslExecutable);
}

// wsl.exe reports its own failures — no distribution installed, WSL not enabled — as UTF-16, while
// anything a Linux program printed arrives as UTF-8. Interleaved NULs are what tells the two apart.
QString decodeWslOutput(const QByteArray& output)
{
    const bool looksUtf16 = output.size() >= 2 && output.contains('\0');

    return (looksUtf16 ? QString::fromUtf16(reinterpret_cast<const char16_t*>(output.constData()),
                             output.size() / 2)
                       : QString::fromUtf8(output))
        .trimmed();
}

// Runs one command inside the default WSL distribution. \a maxAcceptableExitCode carries e2fsck's
// "errors were corrected" exit code of 1 where the caller tolerates it.
bool runInWsl(const QStringList& command, int timeoutMilliseconds, int maxAcceptableExitCode, QString& output)
{
    const QString wslPath = findWsl();

    if (wslPath.isEmpty())
    {
        output = QCoreApplication::translate("FilesystemResizer", "wsl.exe could not be found.");
        return false;
    }

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(wslPath, QStringList{ QStringLiteral("-e") } + command);

    if (!process.waitForFinished(timeoutMilliseconds))
    {
        process.kill();
        output = QCoreApplication::translate("FilesystemResizer", "%1 did not finish in time.").arg(command.value(0));
        return false;
    }

    output = decodeWslOutput(process.readAll());

    return process.exitStatus() == QProcess::NormalExit && process.exitCode() <= maxAcceptableExitCode;
}

struct WslAvailability
{
    bool usable{ false };
    QString reason;
};

// One command answers every question worth asking — WSL present, a distribution installed and able to
// start, and e2fsprogs inside it — so the probe costs a single process launch.
WslAvailability probeWsl()
{
    WslAvailability availability;

    if (findWsl().isEmpty())
    {
        availability.reason = QCoreApplication::translate("FilesystemResizer",
            "Needs WSL, which is not installed. Install it from an administrator terminal with "
            "“wsl --install”, then restart this app.");
        return availability;
    }

    QString output;

    if (!runInWsl(QStringList{ QStringLiteral("sh"), QStringLiteral("-c"),
            QStringLiteral("command -v e2fsck && command -v resize2fs && command -v dumpe2fs") },
            ProbeTimeoutMilliseconds, 0, output))
    {
        availability.reason = QCoreApplication::translate("FilesystemResizer",
            "WSL is installed but cannot run e2fsprogs (%1). Install a Linux distribution and its "
            "e2fsprogs package, then restart this app.").arg(output);
        return availability;
    }

    availability.usable = true;

    return availability;
}

// Probing costs a process launch, and neither WSL nor its distributions appear while the app runs.
const WslAvailability& wslAvailability()
{
    static const WslAvailability availability = probeWsl();

    return availability;
}

// Windows paths mean nothing inside the distribution, and hand-assembling /mnt/<drive>/... guesses at a
// mapping the user can reconfigure, so wslpath is asked instead.
bool toWslPath(const QString& windowsPath, QString& wslPath, QString& errorMessage)
{
    QString output;

    if (!runInWsl(QStringList{ QStringLiteral("wslpath"), QStringLiteral("-a"), windowsPath },
            SetupToolTimeoutMilliseconds, 0, output)
        || output.isEmpty())
    {
        errorMessage = QCoreApplication::translate("FilesystemResizer",
            "Could not translate “%1” into a path WSL understands: %2").arg(windowsPath, output);
        return false;
    }

    wslPath = output;

    return true;
}

bool copyRange(QFile& source,
    quint64 sourceOffsetBytes,
    QFile& destination,
    quint64 destinationOffsetBytes,
    quint64 sizeBytes,
    QString& errorMessage)
{
    if (!source.seek(static_cast<qint64>(sourceOffsetBytes))
        || !destination.seek(static_cast<qint64>(destinationOffsetBytes)))
    {
        errorMessage = QCoreApplication::translate("FilesystemResizer", "Could not seek “%1”: %2")
            .arg(source.fileName(), source.errorString());
        return false;
    }

    std::vector<char> buffer(static_cast<std::size_t>(std::min<quint64>(CopyChunkBytes, std::max<quint64>(sizeBytes, 1))));
    quint64 copiedBytes = 0;

    while (copiedBytes < sizeBytes)
    {
        const qint64 chunkBytes =
            static_cast<qint64>(std::min<quint64>(buffer.size(), sizeBytes - copiedBytes));

        if (source.read(buffer.data(), chunkBytes) != chunkBytes)
        {
            errorMessage = QCoreApplication::translate("FilesystemResizer", "Could not read “%1”: %2")
                .arg(source.fileName(), source.errorString());
            return false;
        }

        if (destination.write(buffer.data(), chunkBytes) != chunkBytes)
        {
            errorMessage = QCoreApplication::translate("FilesystemResizer", "Could not write “%1”: %2")
                .arg(destination.fileName(), destination.errorString());
            return false;
        }

        copiedBytes += static_cast<quint64>(chunkBytes);
    }

    return true;
}

std::optional<quint64> readFilesystemSizeBytes(const QString& wslFilesystemPath, QString& errorMessage)
{
    QString listing;

    if (!runInWsl(QStringList{ QStringLiteral("dumpe2fs"), QStringLiteral("-h"), wslFilesystemPath },
            SetupToolTimeoutMilliseconds, 0, listing))
    {
        errorMessage = QCoreApplication::translate("FilesystemResizer",
            "Could not read the resized filesystem's size: %1").arg(listing);
        return std::nullopt;
    }

    std::optional<quint64> blockCount;
    quint64 blockSizeBytes = 0;

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

bool FilesystemResizer::isSupported(Operation operation)
{
    return operation == Operation::Shrink && wslAvailability().usable;
}

QString FilesystemResizer::getUnsupportedReason(Operation operation)
{
    if (operation == Operation::Grow)
    {
        return tr("Growing a filesystem is not available on Windows — it would need a temporary copy "
                  "the size of the whole device.");
    }

    return wslAvailability().usable ? QString{} : wslAvailability().reason;
}

std::optional<FilesystemResizer::ShrinkResult> FilesystemResizer::shrinkFilesystem(const QString& path,
    quint64 partitionStartSector,
    quint64 partitionSectorCount,
    quint32 sectorSizeBytes,
    QString& errorMessage)
{
    if (!isSupported(Operation::Shrink))
    {
        errorMessage = getUnsupportedReason(Operation::Shrink);
        return std::nullopt;
    }

    const quint64 partitionOffsetBytes = partitionStartSector * sectorSizeBytes;
    const quint64 partitionSizeBytes = partitionSectorCount * sectorSizeBytes;

    QTemporaryDir temporaryDirectory;

    if (!temporaryDirectory.isValid())
    {
        errorMessage = tr("Could not create a temporary directory to resize the filesystem in: %1")
            .arg(temporaryDirectory.errorString());
        return std::nullopt;
    }

    const QString temporaryFilesystemPath = temporaryDirectory.filePath(QStringLiteral("partition.img"));

    QFile image{ path };
    QFile temporaryFilesystem{ temporaryFilesystemPath };

    if (!image.open(QIODevice::ReadWrite) || !temporaryFilesystem.open(QIODevice::ReadWrite))
    {
        errorMessage = tr("Could not open “%1” to resize its filesystem: %2").arg(path, image.errorString());
        return std::nullopt;
    }

    qInfo() << "Copying" << partitionSizeBytes << "bytes of" << path
            << "out to" << temporaryFilesystemPath << "- Windows cannot resize it in place";

    if (!copyRange(image, partitionOffsetBytes, temporaryFilesystem, 0, partitionSizeBytes, errorMessage))
    {
        return std::nullopt;
    }

    // Flushed before WSL opens the same bytes through its own view of the filesystem.
    temporaryFilesystem.close();

    QString wslFilesystemPath;

    if (!toWslPath(temporaryFilesystemPath, wslFilesystemPath, errorMessage))
    {
        return std::nullopt;
    }

    QString output;

    if (!runInWsl(QStringList{ QStringLiteral("e2fsck"), QStringLiteral("-f"), QStringLiteral("-y"),
            wslFilesystemPath }, NoTimeoutMilliseconds, 1, output))
    {
        errorMessage = tr("The filesystem check before resizing failed: %1").arg(output);
        return std::nullopt;
    }

    if (!runInWsl(QStringList{ QStringLiteral("resize2fs"), QStringLiteral("-M"), wslFilesystemPath },
            NoTimeoutMilliseconds, 0, output))
    {
        errorMessage = tr("resize2fs could not shrink the filesystem: %1").arg(output);
        return std::nullopt;
    }

    const auto newFilesystemSizeBytes = readFilesystemSizeBytes(wslFilesystemPath, errorMessage);

    if (!newFilesystemSizeBytes)
    {
        return std::nullopt;
    }

    if (*newFilesystemSizeBytes > partitionSizeBytes)
    {
        errorMessage = tr("The resized filesystem is larger than the partition it came from.");
        return std::nullopt;
    }

    if (!temporaryFilesystem.open(QIODevice::ReadOnly))
    {
        errorMessage = tr("Could not reopen “%1” after resizing it: %2")
            .arg(temporaryFilesystemPath, temporaryFilesystem.errorString());
        return std::nullopt;
    }

    // Only the shrunk filesystem goes back. Whatever of the old partition lies past it is left for the
    // caller's truncation to drop.
    if (!copyRange(temporaryFilesystem, 0, image, partitionOffsetBytes, *newFilesystemSizeBytes, errorMessage))
    {
        return std::nullopt;
    }

    if (!image.flush())
    {
        errorMessage = tr("Could not write the resized filesystem back into “%1”: %2")
            .arg(path, image.errorString());
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
    Q_UNUSED(path)
    Q_UNUSED(partitionStartSector)
    Q_UNUSED(partitionSectorCount)
    Q_UNUSED(sectorSizeBytes)

    errorMessage = getUnsupportedReason(Operation::Grow);

    return false;
}
} // namespace UDI
