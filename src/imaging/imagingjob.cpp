#include <imaging/imagingjob.hpp>

#include <algorithm>
#include <array>
#include <cstring>

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <disk/filesystemresizer.hpp>
#include <disk/volumecontrol.hpp>
#include <imaging/hasher.hpp>
#include <utils/formatting.hpp>


namespace UDI
{
namespace
{
// A trimmed image never ends up smaller than this: partition tables, bootloaders and the odd tool that
// expects a megabyte of slack all live in the first sectors.
constexpr quint64 MinimumTrimmedImageBytes = 1024u * 1024u;

constexpr double SpeedSmoothingFactor = 0.3;

// Real removable media report one of these two as their logical sector size. GPT's checksum lets each
// be tried in turn and trusted only once it actually validates; MBR has no such check, so the fallback
// below just assumes the near-universal majority case.
constexpr std::array<quint32, 2> CandidateSectorSizesBytes{ 512u, 4096u };

// The ext2/3/4 superblock starts 1024 bytes into the filesystem and its magic number sits 0x38 bytes
// into that, regardless of block size.
constexpr quint64 ExtSuperblockOffsetBytes = 1024;
constexpr quint64 ExtMagicOffsetBytes = 0x38;
constexpr quint16 ExtMagicNumber = 0xEF53;

quint64 roundUpTo(quint64 value, quint64 granularity)
{
    return ((value + granularity - 1) / granularity) * granularity;
}

std::size_t bufferAlignment(quint32 sectorSizeBytes)
{
    return std::max<std::size_t>(sectorSizeBytes, 4096);
}
} // namespace

ImagingJob::ImagingJob(ImagingRequest request,
    ImagingJobSettings settings,
    ProgressCallback progressCallback,
    const std::atomic_bool& cancelRequested)
    : m_request(std::move(request))
    , m_settings(std::move(settings))
    , m_progressCallback(std::move(progressCallback))
    , m_cancelRequested(cancelRequested)
{}

ImagingResult ImagingJob::run()
{
    m_runTimer.start();
    reportProgress(ImagingStage::Preparing, 0, 0, true);

    switch (m_request.operation)
    {
    case ImagingOperation::Read:   return runRead();
    case ImagingOperation::Write:  return runWrite();
    case ImagingOperation::Verify: return runVerify();
    case ImagingOperation::None:   break;
    }

    return makeResult(ImagingStage::Failed, tr("No operation was requested."));
}

ImagingResult ImagingJob::runRead()
{
    QString errorMessage;
    const auto device = openDevice(RawDevice::AccessMode::Read, errorMessage);
    if (!device)
    {
        return makeResult(ImagingStage::Failed, errorMessage);
    }

    const quint32 sectorSizeBytes = device->getSectorSizeBytes();
    quint64 totalBytes = device->getSizeBytes();

    const auto trimPlan = resolveTrimPlan(*device, totalBytes);

    if (isCancelled())
    {
        return makeResult(ImagingStage::Cancelled, QString{});
    }

    const quint64 dataSizeBytes = trimPlan ? trimPlan->dataSizeBytes : device->getSizeBytes();

    // QSaveFile writes to a sibling temporary file and renames on commit, so a cancelled or failed read
    // leaves no half-written image where a valid one used to be.
    QSaveFile imageFile{ m_request.imageFilePath };
    if (!imageFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return makeResult(ImagingStage::Failed, tr("Could not open “%1” for writing: %2")
            .arg(m_request.imageFilePath, imageFile.errorString()));
    }

    if (!device->seek(0))
    {
        imageFile.cancelWriting();
        return makeResult(ImagingStage::Failed, tr("Could not rewind %1: %2")
            .arg(QString::fromUtf8(m_request.device.path), device->getLastError()));
    }

    Hasher hasher{ m_request.computeSha256 };

    const quint64 chunkSizeBytes = alignedChunkSize(sectorSizeBytes);
    AlignedBuffer buffer{ static_cast<std::size_t>(chunkSizeBytes), bufferAlignment(sectorSizeBytes) };

    qInfo() << "Reading" << dataSizeBytes << "bytes from" << m_request.device.path
            << "into" << m_request.imageFilePath << "(trim mode" << trimModeToken(m_request.trimMode) << ')';

    quint64 offsetBytes = 0;

    while (offsetBytes < dataSizeBytes)
    {
        if (isCancelled())
        {
            imageFile.cancelWriting();
            return makeResult(ImagingStage::Cancelled, QString{});
        }

        const qint64 chunkBytes = static_cast<qint64>(std::min(chunkSizeBytes, dataSizeBytes - offsetBytes));

        if (device->read(buffer.data(), chunkBytes) != chunkBytes)
        {
            imageFile.cancelWriting();
            return makeResult(ImagingStage::Failed, tr("Reading %1 failed at offset %2: %3")
                .arg(QString::fromUtf8(m_request.device.path),
                    formatByteSize(offsetBytes),
                    device->getLastError()));
        }

        // The GPT primary header has to describe the shorter medium. Patching it here, before the sector
        // is hashed and written, keeps the digest a digest of the file that was actually produced.
        if (offsetBytes == 0 && trimPlan && trimPlan->gptTrailer)
        {
            const GptTrimTrailer& trailer = *trimPlan->gptTrailer;
            const std::size_t headerOffset =
                static_cast<std::size_t>(trailer.primaryHeaderSectorIndex) * sectorSizeBytes;

            if (headerOffset + trailer.primaryHeaderSector.size() <= static_cast<std::size_t>(chunkBytes))
            {
                std::copy(trailer.primaryHeaderSector.cbegin(),
                    trailer.primaryHeaderSector.cend(),
                    buffer.data() + headerOffset);
            }
        }

        hasher.update(buffer.data(), static_cast<std::size_t>(chunkBytes));

        if (imageFile.write(reinterpret_cast<const char*>(buffer.data()), chunkBytes) != chunkBytes)
        {
            imageFile.cancelWriting();
            return makeResult(ImagingStage::Failed, tr("Writing to “%1” failed: %2")
                .arg(m_request.imageFilePath, imageFile.errorString()));
        }

        offsetBytes += static_cast<quint64>(chunkBytes);
        m_processedBytes = offsetBytes;

        reportProgress(ImagingStage::Transferring, offsetBytes, totalBytes, false);
    }

    if (trimPlan && trimPlan->gptTrailer)
    {
        const std::vector<std::uint8_t>& trailerSectors = trimPlan->gptTrailer->trailerSectors;
        const qint64 trailerBytes = static_cast<qint64>(trailerSectors.size());

        hasher.update(trailerSectors.data(), trailerSectors.size());

        if (imageFile.write(reinterpret_cast<const char*>(trailerSectors.data()), trailerBytes) != trailerBytes)
        {
            imageFile.cancelWriting();
            return makeResult(ImagingStage::Failed, tr("Writing the rebuilt GPT metadata to “%1” failed: %2")
                .arg(m_request.imageFilePath, imageFile.errorString()));
        }

        m_processedBytes += static_cast<quint64>(trailerBytes);
    }

    reportProgress(ImagingStage::Finalizing, m_processedBytes, totalBytes, true);

    if (!imageFile.commit())
    {
        return makeResult(ImagingStage::Failed, tr("Could not finish writing “%1”: %2")
            .arg(m_request.imageFilePath, imageFile.errorString()));
    }

    m_fastDigestHex = hasher.getFastDigestHex();
    m_sha256Hex = hasher.getSha256Hex();

    if (m_request.shrinkFilesystemAfterRead)
    {
        reportProgress(ImagingStage::ShrinkingFilesystem, 0, 0, true);

        QString shrinkErrorMessage;

        if (!shrinkImageFilesystem(shrinkErrorMessage))
        {
            return makeResult(ImagingStage::Failed, tr("“%1” was read successfully, but its filesystem "
                "could not be shrunk: %2").arg(m_request.imageFilePath, shrinkErrorMessage));
        }
    }

    return makeResult(ImagingStage::Complete, QString{});
}

ImagingResult ImagingJob::runWrite()
{
    const QFileInfo imageInfo{ m_request.imageFilePath };

    if (!imageInfo.exists() || !imageInfo.isFile())
    {
        return makeResult(ImagingStage::Failed, tr("Image file “%1” does not exist.").arg(m_request.imageFilePath));
    }

    // Not const: growFilesystemToFillDevice below can change the image's size on disk before the main
    // copy loop starts.
    quint64 imageSizeBytes = static_cast<quint64>(imageInfo.size());

    if (imageSizeBytes == 0)
    {
        return makeResult(ImagingStage::Failed, tr("Image file “%1” is empty.").arg(m_request.imageFilePath));
    }

    if (m_request.device.systemDevice)
    {
        return makeResult(ImagingStage::Failed,
            tr("%1 holds the running operating system and will not be written to.")
                .arg(QString::fromUtf8(m_request.device.path)));
    }

    if (m_request.device.writeProtected)
    {
        return makeResult(ImagingStage::Failed, tr("%1 is write protected.")
            .arg(QString::fromUtf8(m_request.device.path)));
    }

    if (imageSizeBytes > m_request.device.sizeBytes)
    {
        return makeResult(ImagingStage::Failed, tr("The image is %1 but the device only holds %2.")
            .arg(formatByteSize(imageSizeBytes), formatByteSize(m_request.device.sizeBytes)));
    }

    QFile imageFile{ m_request.imageFilePath };
    if (!imageFile.open(QIODevice::ReadOnly))
    {
        return makeResult(ImagingStage::Failed, tr("Could not open “%1” for reading: %2")
            .arg(m_request.imageFilePath, imageFile.errorString()));
    }

    reportProgress(ImagingStage::Unmounting, 0, imageSizeBytes, true);

    VolumeControl volumeControl{ m_request.device };
    QString volumeErrorMessage;
    if (!volumeControl.acquire(volumeErrorMessage))
    {
        return makeResult(ImagingStage::Failed, volumeErrorMessage);
    }

    QString errorMessage;
    const auto device = openDevice(RawDevice::AccessMode::Write, errorMessage);
    if (!device)
    {
        return makeResult(ImagingStage::Failed, errorMessage);
    }

    if (!device->seek(0))
    {
        return makeResult(ImagingStage::Failed, tr("Could not rewind %1: %2")
            .arg(QString::fromUtf8(m_request.device.path), device->getLastError()));
    }

    if (m_request.growFilesystemToFillDevice)
    {
        reportProgress(ImagingStage::GrowingFilesystem, 0, 0, true);

        // Grown here, on the image file itself, rather than on the device after writing: the same
        // file-based machinery the shrink feature already uses, instead of a second, riskier path that
        // has to fight this job's own exclusive hold on the device to attach a loop device against it.
        imageFile.close();

        QString growErrorMessage;

        if (!growImageFilesystem(device->getSizeBytes(), growErrorMessage))
        {
            return makeResult(ImagingStage::Failed, tr("Could not grow the filesystem in “%1” before "
                "writing it: %2").arg(m_request.imageFilePath, growErrorMessage));
        }

        if (!imageFile.open(QIODevice::ReadOnly))
        {
            return makeResult(ImagingStage::Failed, tr("Could not reopen “%1” after growing it: %2")
                .arg(m_request.imageFilePath, imageFile.errorString()));
        }

        imageSizeBytes = static_cast<quint64>(QFileInfo{ m_request.imageFilePath }.size());
    }

    const quint32 sectorSizeBytes = device->getSectorSizeBytes();
    const quint64 chunkSizeBytes = alignedChunkSize(sectorSizeBytes);
    AlignedBuffer buffer{ static_cast<std::size_t>(chunkSizeBytes), bufferAlignment(sectorSizeBytes) };
    Hasher hasher{ m_request.computeSha256 };

    // The whole image is still read and hashed below regardless, so the reported digest always matches
    // the image file exactly; only the device write for a trailing run of zeros is skipped.
    quint64 writeSizeBytes = imageSizeBytes;

    if (m_request.skipTrailingZerosOnWrite)
    {
        reportProgress(ImagingStage::Analyzing, 0, imageSizeBytes, true);

        const auto lastNonZeroByte = findLastNonZeroByteInImage(imageFile, imageSizeBytes);

        if (isCancelled())
        {
            return makeResult(ImagingStage::Cancelled, QString{});
        }

        writeSizeBytes = lastNonZeroByte
            ? std::min(roundUpTo(*lastNonZeroByte + 1, sectorSizeBytes), imageSizeBytes)
            : 0;

        if (writeSizeBytes < imageSizeBytes)
        {
            qInfo() << "Skipping" << (imageSizeBytes - writeSizeBytes) << "trailing zero bytes of"
                    << m_request.imageFilePath << "- the corresponding tail of"
                    << m_request.device.path << "is left untouched";
        }

        if (!imageFile.seek(0))
        {
            return makeResult(ImagingStage::Failed, tr("Could not rewind “%1”: %2")
                .arg(m_request.imageFilePath, imageFile.errorString()));
        }
    }

    qInfo() << "Writing" << imageSizeBytes << "bytes from" << m_request.imageFilePath
            << "to" << m_request.device.path;

    quint64 offsetBytes = 0;

    while (offsetBytes < imageSizeBytes)
    {
        if (isCancelled())
        {
            return makeResult(ImagingStage::Cancelled,
                tr("The write was cancelled after %1, so %2 now holds an incomplete image.")
                    .arg(formatByteSize(offsetBytes), QString::fromUtf8(m_request.device.path)));
        }

        const qint64 chunkBytes = static_cast<qint64>(std::min(chunkSizeBytes, imageSizeBytes - offsetBytes));

        if (imageFile.read(reinterpret_cast<char*>(buffer.data()), chunkBytes) != chunkBytes)
        {
            return makeResult(ImagingStage::Failed, tr("Reading “%1” failed at offset %2: %3")
                .arg(m_request.imageFilePath, formatByteSize(offsetBytes), imageFile.errorString()));
        }

        hasher.update(buffer.data(), static_cast<std::size_t>(chunkBytes));

        const qint64 bytesToWrite = offsetBytes < writeSizeBytes
            ? static_cast<qint64>(std::min(static_cast<quint64>(chunkBytes), writeSizeBytes - offsetBytes))
            : 0;

        if (bytesToWrite > 0)
        {
            // A device only accepts whole sectors, so a write whose length is not a multiple of the
            // sector size gets its final sector padded with zeros.
            const qint64 sectorAlignedBytes =
                static_cast<qint64>(roundUpTo(static_cast<quint64>(bytesToWrite), sectorSizeBytes));

            if (sectorAlignedBytes > bytesToWrite)
            {
                std::fill(buffer.data() + bytesToWrite, buffer.data() + sectorAlignedBytes, std::uint8_t{ 0 });
            }

            if (device->write(buffer.data(), sectorAlignedBytes) != sectorAlignedBytes)
            {
                return makeResult(ImagingStage::Failed, tr("Writing to %1 failed at offset %2: %3")
                    .arg(QString::fromUtf8(m_request.device.path),
                        formatByteSize(offsetBytes),
                        device->getLastError()));
            }
        }

        offsetBytes += static_cast<quint64>(chunkBytes);
        m_processedBytes = offsetBytes;

        reportProgress(ImagingStage::Transferring, offsetBytes, imageSizeBytes, false);
    }

    reportProgress(ImagingStage::Finalizing, imageSizeBytes, imageSizeBytes, true);

    if (!device->sync())
    {
        return makeResult(ImagingStage::Failed, tr("Flushing %1 failed: %2")
            .arg(QString::fromUtf8(m_request.device.path), device->getLastError()));
    }

    m_fastDigestHex = hasher.getFastDigestHex();
    m_sha256Hex = hasher.getSha256Hex();
    // Reported as the amount written rather than the (possibly larger) amount read and hashed above.
    m_processedBytes = writeSizeBytes;

    if (m_request.verifyAfterWrite)
    {
        if (!imageFile.seek(0))
        {
            return makeResult(ImagingStage::Failed, tr("Could not rewind “%1”: %2")
                .arg(m_request.imageFilePath, imageFile.errorString()));
        }

        // A skipped tail was never written, so verifying it would only ever compare the image's zeros
        // against whatever the device already had there.
        if (!compareDeviceWithImage(*device, imageFile, writeSizeBytes,
                ImagingStage::Verifying, 0, writeSizeBytes, errorMessage))
        {
            if (isCancelled())
            {
                return makeResult(ImagingStage::Cancelled, QString{});
            }

            return makeResult(ImagingStage::Failed, errorMessage);
        }

        m_verificationMatched = true;
    }

    return makeResult(ImagingStage::Complete, QString{});
}

ImagingResult ImagingJob::runVerify()
{
    const QFileInfo imageInfo{ m_request.imageFilePath };

    if (!imageInfo.exists() || !imageInfo.isFile())
    {
        return makeResult(ImagingStage::Failed, tr("Image file “%1” does not exist.").arg(m_request.imageFilePath));
    }

    const quint64 imageSizeBytes = static_cast<quint64>(imageInfo.size());

    if (imageSizeBytes == 0)
    {
        return makeResult(ImagingStage::Failed, tr("Image file “%1” is empty.").arg(m_request.imageFilePath));
    }

    QString errorMessage;
    const auto device = openDevice(RawDevice::AccessMode::Read, errorMessage);
    if (!device)
    {
        return makeResult(ImagingStage::Failed, errorMessage);
    }

    if (imageSizeBytes > device->getSizeBytes())
    {
        return makeResult(ImagingStage::Failed, tr("The image is %1 but the device only holds %2.")
            .arg(formatByteSize(imageSizeBytes), formatByteSize(device->getSizeBytes())));
    }

    QFile imageFile{ m_request.imageFilePath };
    if (!imageFile.open(QIODevice::ReadOnly))
    {
        return makeResult(ImagingStage::Failed, tr("Could not open “%1” for reading: %2")
            .arg(m_request.imageFilePath, imageFile.errorString()));
    }

    qInfo() << "Verifying" << m_request.device.path << "against" << m_request.imageFilePath;

    if (!compareDeviceWithImage(*device, imageFile, imageSizeBytes,
            ImagingStage::Verifying, 0, imageSizeBytes, errorMessage))
    {
        if (isCancelled())
        {
            return makeResult(ImagingStage::Cancelled, QString{});
        }

        return makeResult(ImagingStage::Failed, errorMessage);
    }

    m_verificationMatched = true;

    return makeResult(ImagingStage::Complete, QString{});
}

std::unique_ptr<RawDevice> ImagingJob::openDevice(RawDevice::AccessMode mode, QString& errorMessage) const
{
    auto device = RawDevice::open(m_request.device, mode, errorMessage);

    if (!device)
    {
        return nullptr;
    }

    if (device->getSizeBytes() == 0)
    {
        errorMessage = tr("%1 reports a capacity of zero bytes — is the card still inserted?")
            .arg(QString::fromUtf8(m_request.device.path));
        return nullptr;
    }

    return device;
}

std::optional<TrimPlan> ImagingJob::resolveTrimPlan(RawDevice& device, quint64& totalBytes)
{
    if (m_request.trimMode == TrimMode::None)
    {
        return std::nullopt;
    }

    const quint32 sectorSizeBytes = device.getSectorSizeBytes();
    const quint64 deviceSizeBytes = device.getSizeBytes();

    reportProgress(ImagingStage::Analyzing, 0, deviceSizeBytes, true);

    if (m_request.trimMode == TrimMode::Partitions)
    {
        const quint64 scanSizeBytes = std::min(
            roundUpTo(m_settings.partitionScanSizeBytes, sectorSizeBytes), deviceSizeBytes);

        AlignedBuffer head{ static_cast<std::size_t>(scanSizeBytes), bufferAlignment(sectorSizeBytes) };

        if (!device.seek(0) || device.read(head.data(), static_cast<qint64>(scanSizeBytes)) != static_cast<qint64>(scanSizeBytes))
        {
            qWarning() << "Could not read the partition table of" << m_request.device.path
                       << ':' << device.getLastError() << "- copying the whole device instead";
            return std::nullopt;
        }

        const std::vector<std::uint8_t> headBytes(head.data(), head.data() + scanSizeBytes);
        const PartitionTable table = PartitionTable::parse(headBytes, sectorSizeBytes, deviceSizeBytes);

        auto plan = table.planTrim(m_settings.trimAlignmentBytes);

        if (!plan)
        {
            qInfo() << "No partition table to trim to on" << m_request.device.path
                    << "- copying the whole device";
            return std::nullopt;
        }

        // A GPT plan's sizes are load-bearing — the rebuilt backup header records where the trailer sits
        // — so only a plain MBR trim is rounded up to the minimum.
        if (!plan->gptTrailer)
        {
            plan->dataSizeBytes = std::min(std::max(plan->dataSizeBytes, MinimumTrimmedImageBytes), deviceSizeBytes);
            plan->imageSizeBytes = plan->dataSizeBytes;
        }

        totalBytes = plan->imageSizeBytes;

        qInfo() << "Trimming to the last partition:" << plan->imageSizeBytes << "of"
                << deviceSizeBytes << "bytes, GPT rebuilt:" << plan->gptTrailer.has_value();

        return plan;
    }

    const auto lastNonZeroByte = findLastNonZeroByte(device);

    if (!lastNonZeroByte)
    {
        return std::nullopt;
    }

    const quint64 trimmedSizeBytes = std::min(
        std::max(roundUpTo(*lastNonZeroByte + 1, sectorSizeBytes), MinimumTrimmedImageBytes),
        deviceSizeBytes);

    if (trimmedSizeBytes >= deviceSizeBytes)
    {
        qInfo() << "Data reaches the end of" << m_request.device.path << "- copying the whole device";
        return std::nullopt;
    }

    qInfo() << "Trimming trailing zeros:" << trimmedSizeBytes << "of" << deviceSizeBytes << "bytes";

    TrimPlan plan;
    plan.dataSizeBytes = trimmedSizeBytes;
    plan.imageSizeBytes = trimmedSizeBytes;
    totalBytes = trimmedSizeBytes;

    return plan;
}

std::optional<quint64> ImagingJob::findLastNonZeroByte(RawDevice& device)
{
    const quint32 sectorSizeBytes = device.getSectorSizeBytes();
    const quint64 deviceSizeBytes = device.getSizeBytes();
    const quint64 chunkSizeBytes = alignedChunkSize(sectorSizeBytes);

    AlignedBuffer buffer{ static_cast<std::size_t>(chunkSizeBytes), bufferAlignment(sectorSizeBytes) };

    quint64 scanEndBytes = deviceSizeBytes;

    while (scanEndBytes > 0)
    {
        if (isCancelled())
        {
            return std::nullopt;
        }

        const quint64 chunkBytes = std::min(chunkSizeBytes, scanEndBytes);
        const quint64 chunkStartBytes = scanEndBytes - chunkBytes;

        if (!device.seek(chunkStartBytes)
            || device.read(buffer.data(), static_cast<qint64>(chunkBytes)) != static_cast<qint64>(chunkBytes))
        {
            qWarning() << "Trailing-zero scan of" << m_request.device.path << "failed at offset"
                       << chunkStartBytes << ':' << device.getLastError() << "- copying the whole device";
            return std::nullopt;
        }

        for (quint64 index = chunkBytes; index > 0; --index)
        {
            if (buffer.data()[index - 1] != 0)
            {
                return chunkStartBytes + index - 1;
            }
        }

        scanEndBytes = chunkStartBytes;

        reportProgress(ImagingStage::Analyzing, deviceSizeBytes - scanEndBytes, deviceSizeBytes, false);
    }

    return 0;
}

std::optional<quint64> ImagingJob::findLastNonZeroByteInImage(QIODevice& imageFile, quint64 imageSizeBytes)
{
    const quint64 chunkSizeBytes = std::max<quint64>(m_settings.chunkSizeBytes, 1u);
    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(std::min(chunkSizeBytes, imageSizeBytes)));

    quint64 scanEndBytes = imageSizeBytes;

    while (scanEndBytes > 0)
    {
        if (isCancelled())
        {
            return std::nullopt;
        }

        const quint64 chunkBytes = std::min(chunkSizeBytes, scanEndBytes);
        const quint64 chunkStartBytes = scanEndBytes - chunkBytes;

        if (!imageFile.seek(static_cast<qint64>(chunkStartBytes))
            || imageFile.read(reinterpret_cast<char*>(buffer.data()), static_cast<qint64>(chunkBytes))
                != static_cast<qint64>(chunkBytes))
        {
            qWarning() << "Trailing-zero scan of" << m_request.imageFilePath << "failed at offset"
                       << chunkStartBytes << ':' << imageFile.errorString() << "- writing the whole image";
            return std::nullopt;
        }

        for (quint64 index = chunkBytes; index > 0; --index)
        {
            if (buffer[index - 1] != 0)
            {
                return chunkStartBytes + index - 1;
            }
        }

        scanEndBytes = chunkStartBytes;

        reportProgress(ImagingStage::Analyzing, imageSizeBytes - scanEndBytes, imageSizeBytes, false);
    }

    return 0;
}

PartitionTable ImagingJob::parseImagePartitionTable(const std::vector<std::uint8_t>& head, quint64 imageSizeBytes) const
{
    for (const quint32 candidateSectorSizeBytes : CandidateSectorSizesBytes)
    {
        PartitionTable table = PartitionTable::parse(head, candidateSectorSizeBytes, imageSizeBytes);

        if (table.getScheme() == PartitionScheme::Gpt)
        {
            return table;
        }
    }

    return PartitionTable::parse(head, CandidateSectorSizesBytes.front(), imageSizeBytes);
}

bool ImagingJob::readImageHead(quint64 imageSizeBytes, std::vector<std::uint8_t>& head, QString& errorMessage) const
{
    QFile imageFile{ m_request.imageFilePath };

    if (!imageFile.open(QIODevice::ReadOnly))
    {
        errorMessage = tr("Could not open “%1” for reading: %2")
            .arg(m_request.imageFilePath, imageFile.errorString());
        return false;
    }

    const quint64 scanSizeBytes = std::min(m_settings.partitionScanSizeBytes, imageSizeBytes);
    head.assign(static_cast<std::size_t>(scanSizeBytes), std::uint8_t{ 0 });

    if (imageFile.read(reinterpret_cast<char*>(head.data()), static_cast<qint64>(scanSizeBytes))
        != static_cast<qint64>(scanSizeBytes))
    {
        errorMessage = tr("Could not read the partition table of “%1”: %2")
            .arg(m_request.imageFilePath, imageFile.errorString());
        return false;
    }

    return true;
}

bool ImagingJob::looksLikeExtFilesystem(quint64 partitionOffsetBytes, QString& errorMessage) const
{
    QFile imageFile{ m_request.imageFilePath };

    if (!imageFile.open(QIODevice::ReadOnly)
        || !imageFile.seek(static_cast<qint64>(partitionOffsetBytes + ExtSuperblockOffsetBytes + ExtMagicOffsetBytes)))
    {
        errorMessage = tr("Could not inspect the last partition of “%1”: %2")
            .arg(m_request.imageFilePath, imageFile.errorString());
        return false;
    }

    std::array<std::uint8_t, 2> magic{};

    if (imageFile.read(reinterpret_cast<char*>(magic.data()), static_cast<qint64>(magic.size()))
        != static_cast<qint64>(magic.size()))
    {
        errorMessage = tr("Could not inspect the last partition of “%1”: %2")
            .arg(m_request.imageFilePath, imageFile.errorString());
        return false;
    }

    if ((static_cast<quint16>(magic[0]) | (static_cast<quint16>(magic[1]) << 8)) != ExtMagicNumber)
    {
        errorMessage = tr("The last partition of “%1” does not look like an ext2/3/4 filesystem.")
            .arg(m_request.imageFilePath);
        return false;
    }

    return true;
}

bool ImagingJob::applyPartitionResizePlan(QFile& imageFile, const TrimPlan& plan, quint32 sectorSizeBytes) const
{
    if (plan.patchedFirstSector)
    {
        const auto& sector = *plan.patchedFirstSector;

        if (!imageFile.seek(0)
            || imageFile.write(reinterpret_cast<const char*>(sector.data()), static_cast<qint64>(sector.size()))
                != static_cast<qint64>(sector.size()))
        {
            return false;
        }
    }

    if (plan.gptTrailer)
    {
        const GptTrimTrailer& trailer = *plan.gptTrailer;
        const qint64 headerOffset = static_cast<qint64>(trailer.primaryHeaderSectorIndex) * sectorSizeBytes;

        if (!imageFile.seek(headerOffset)
            || imageFile.write(reinterpret_cast<const char*>(trailer.primaryHeaderSector.data()),
                   static_cast<qint64>(trailer.primaryHeaderSector.size()))
                != static_cast<qint64>(trailer.primaryHeaderSector.size()))
        {
            return false;
        }

        const qint64 trailerOffset =
            static_cast<qint64>(plan.imageSizeBytes) - static_cast<qint64>(trailer.trailerSectors.size());

        if (!imageFile.seek(trailerOffset)
            || imageFile.write(reinterpret_cast<const char*>(trailer.trailerSectors.data()),
                   static_cast<qint64>(trailer.trailerSectors.size()))
                != static_cast<qint64>(trailer.trailerSectors.size()))
        {
            return false;
        }

        if (trailer.patchedPrimaryEntryArray)
        {
            const auto& primaryEntryArray = *trailer.patchedPrimaryEntryArray;
            const qint64 primaryEntryArrayOffset =
                static_cast<qint64>(trailer.primaryEntryArrayFirstSector) * sectorSizeBytes;

            if (!imageFile.seek(primaryEntryArrayOffset)
                || imageFile.write(reinterpret_cast<const char*>(primaryEntryArray.data()),
                       static_cast<qint64>(primaryEntryArray.size()))
                    != static_cast<qint64>(primaryEntryArray.size()))
            {
                return false;
            }
        }
    }

    return imageFile.resize(static_cast<qint64>(plan.imageSizeBytes));
}

bool ImagingJob::computeFileDigests(QString& errorMessage)
{
    QFile imageFile{ m_request.imageFilePath };

    if (!imageFile.open(QIODevice::ReadOnly))
    {
        errorMessage = tr("Could not reopen “%1” to recompute its digest: %2")
            .arg(m_request.imageFilePath, imageFile.errorString());
        return false;
    }

    const quint64 imageSizeBytes = static_cast<quint64>(imageFile.size());
    const quint64 chunkSizeBytes = std::max<quint64>(m_settings.chunkSizeBytes, 1u);
    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(std::min(chunkSizeBytes, std::max<quint64>(imageSizeBytes, 1u))));

    Hasher hasher{ m_request.computeSha256 };
    quint64 offsetBytes = 0;

    while (offsetBytes < imageSizeBytes)
    {
        const qint64 chunkBytes = static_cast<qint64>(std::min(chunkSizeBytes, imageSizeBytes - offsetBytes));

        if (imageFile.read(reinterpret_cast<char*>(buffer.data()), chunkBytes) != chunkBytes)
        {
            errorMessage = tr("Could not reopen “%1” to recompute its digest: %2")
                .arg(m_request.imageFilePath, imageFile.errorString());
            return false;
        }

        hasher.update(buffer.data(), static_cast<std::size_t>(chunkBytes));
        offsetBytes += static_cast<quint64>(chunkBytes);
    }

    m_fastDigestHex = hasher.getFastDigestHex();
    m_sha256Hex = hasher.getSha256Hex();

    return true;
}

bool ImagingJob::shrinkImageFilesystem(QString& errorMessage)
{
    const quint64 imageSizeBytes = static_cast<quint64>(QFileInfo{ m_request.imageFilePath }.size());

    std::vector<std::uint8_t> head;

    if (!readImageHead(imageSizeBytes, head, errorMessage))
    {
        return false;
    }

    const PartitionTable table = parseImagePartitionTable(head, imageSizeBytes);
    const PartitionEntry* const lastPartition = table.getLastPartition();

    if (table.getScheme() == PartitionScheme::None || !lastPartition)
    {
        errorMessage = tr("No partition table was found in the image, so there is no filesystem to shrink.");
        return false;
    }

    const quint32 sectorSizeBytes = table.getSectorSizeBytes();
    const quint64 partitionOffsetBytes = lastPartition->firstSector * sectorSizeBytes;

    if (!looksLikeExtFilesystem(partitionOffsetBytes, errorMessage))
    {
        return false;
    }

    const auto shrinkResult = FilesystemResizer::shrinkFilesystem(m_request.imageFilePath,
        lastPartition->firstSector, lastPartition->sectorCount, sectorSizeBytes, errorMessage);

    if (!shrinkResult)
    {
        return false;
    }

    if (shrinkResult->newPartitionSectorCount >= lastPartition->sectorCount)
    {
        qInfo() << "resize2fs could not shrink the filesystem in" << m_request.imageFilePath << "any further";
        return true;
    }

    const auto plan = table.planShrinkLastPartition(shrinkResult->newPartitionSectorCount);

    if (!plan)
    {
        errorMessage = tr("Could not rewrite the partition table for the shrunk filesystem.");
        return false;
    }

    QFile imageFile{ m_request.imageFilePath };

    if (!imageFile.open(QIODevice::ReadWrite) || !applyPartitionResizePlan(imageFile, *plan, sectorSizeBytes))
    {
        errorMessage = tr("Could not write the shrunk partition table to “%1”: %2")
            .arg(m_request.imageFilePath, imageFile.errorString());
        return false;
    }

    imageFile.close();

    qInfo() << "Shrank the filesystem in" << m_request.imageFilePath << "to" << plan->imageSizeBytes << "bytes";

    return computeFileDigests(errorMessage);
}

bool ImagingJob::growImageFilesystem(quint64 targetSizeBytes, QString& errorMessage)
{
    const quint64 imageSizeBytes = static_cast<quint64>(QFileInfo{ m_request.imageFilePath }.size());

    std::vector<std::uint8_t> head;

    if (!readImageHead(imageSizeBytes, head, errorMessage))
    {
        return false;
    }

    const PartitionTable table = parseImagePartitionTable(head, imageSizeBytes);
    const PartitionEntry* const lastPartition = table.getLastPartition();

    if (table.getScheme() == PartitionScheme::None || !lastPartition)
    {
        errorMessage = tr("No partition table was found in the image, so there is no filesystem to grow.");
        return false;
    }

    const quint32 sectorSizeBytes = table.getSectorSizeBytes();
    const quint64 partitionOffsetBytes = lastPartition->firstSector * sectorSizeBytes;

    if (!looksLikeExtFilesystem(partitionOffsetBytes, errorMessage))
    {
        return false;
    }

    const auto plan = table.planGrowLastPartition(targetSizeBytes, m_settings.trimAlignmentBytes);

    if (!plan)
    {
        qInfo() << "The image already fills the device - nothing to grow";
        return true;
    }

    QFile imageFile{ m_request.imageFilePath };

    if (!imageFile.open(QIODevice::ReadWrite)
        || !imageFile.resize(static_cast<qint64>(plan->imageSizeBytes))
        || !applyPartitionResizePlan(imageFile, *plan, sectorSizeBytes))
    {
        errorMessage = tr("Could not grow the partition table in “%1”: %2")
            .arg(m_request.imageFilePath, imageFile.errorString());
        return false;
    }

    imageFile.close();

    const quint64 newPartitionSectorCount = plan->dataSizeBytes / sectorSizeBytes - lastPartition->firstSector;

    if (!FilesystemResizer::growFilesystem(m_request.imageFilePath,
            lastPartition->firstSector, newPartitionSectorCount, sectorSizeBytes, errorMessage))
    {
        return false;
    }

    qInfo() << "Grew the filesystem in" << m_request.imageFilePath << "to" << plan->imageSizeBytes << "bytes";

    return true;
}

bool ImagingJob::compareDeviceWithImage(RawDevice& device,
    QIODevice& imageFile,
    quint64 sizeToCompare,
    ImagingStage stage,
    quint64 processedBytesBefore,
    quint64 totalProgressBytes,
    QString& errorMessage)
{
    const quint32 sectorSizeBytes = device.getSectorSizeBytes();
    const quint64 chunkSizeBytes = alignedChunkSize(sectorSizeBytes);

    AlignedBuffer deviceBuffer{ static_cast<std::size_t>(chunkSizeBytes), bufferAlignment(sectorSizeBytes) };
    std::vector<std::uint8_t> imageBuffer(static_cast<std::size_t>(chunkSizeBytes));

    if (!device.seek(0))
    {
        errorMessage = tr("Could not rewind %1: %2")
            .arg(QString::fromUtf8(m_request.device.path), device.getLastError());
        return false;
    }

    quint64 offsetBytes = 0;

    while (offsetBytes < sizeToCompare)
    {
        if (isCancelled())
        {
            return false;
        }

        const qint64 compareBytes = static_cast<qint64>(std::min(chunkSizeBytes, sizeToCompare - offsetBytes));
        const qint64 deviceReadBytes =
            static_cast<qint64>(roundUpTo(static_cast<quint64>(compareBytes), sectorSizeBytes));

        if (device.read(deviceBuffer.data(), deviceReadBytes) != deviceReadBytes)
        {
            errorMessage = tr("Reading %1 failed at offset %2: %3")
                .arg(QString::fromUtf8(m_request.device.path),
                    formatByteSize(offsetBytes),
                    device.getLastError());
            return false;
        }

        if (imageFile.read(reinterpret_cast<char*>(imageBuffer.data()), compareBytes) != compareBytes)
        {
            errorMessage = tr("Reading “%1” failed at offset %2: %3")
                .arg(m_request.imageFilePath, formatByteSize(offsetBytes), imageFile.errorString());
            return false;
        }

        const auto mismatch = std::mismatch(imageBuffer.cbegin(),
            imageBuffer.cbegin() + static_cast<std::ptrdiff_t>(compareBytes),
            deviceBuffer.data());

        if (mismatch.first != imageBuffer.cbegin() + static_cast<std::ptrdiff_t>(compareBytes))
        {
            const quint64 mismatchOffset =
                offsetBytes + static_cast<quint64>(std::distance(imageBuffer.cbegin(), mismatch.first));

            errorMessage = tr("The device and the image differ at offset %1 (byte 0x%2 on the device, "
                              "0x%3 in the image).")
                .arg(formatByteSize(mismatchOffset))
                .arg(*mismatch.second, 2, 16, QLatin1Char('0'))
                .arg(*mismatch.first, 2, 16, QLatin1Char('0'));
            return false;
        }

        offsetBytes += static_cast<quint64>(compareBytes);
        m_processedBytes = processedBytesBefore + offsetBytes;

        reportProgress(stage, m_processedBytes, totalProgressBytes, false);
    }

    return true;
}

void ImagingJob::reportProgress(ImagingStage stage, quint64 processedBytes, quint64 totalBytes, bool force)
{
    const qint64 nowMilliseconds = m_runTimer.elapsed();

    if (!force && nowMilliseconds - m_lastProgressReportMs < m_settings.progressIntervalMilliseconds)
    {
        return;
    }

    const qint64 intervalMilliseconds = nowMilliseconds - m_lastProgressReportMs;

    if (intervalMilliseconds > 0 && processedBytes >= m_lastProgressProcessedBytes)
    {
        const double instantBytesPerSecond =
            static_cast<double>(processedBytes - m_lastProgressProcessedBytes) * 1000.0
            / static_cast<double>(intervalMilliseconds);

        // A raw per-interval rate swings wildly with the device's own write caching, which makes the
        // remaining-time estimate jump around; smoothing it keeps both readable.
        m_smoothedBytesPerSecond = m_smoothedBytesPerSecond > 0.0
            ? (1.0 - SpeedSmoothingFactor) * m_smoothedBytesPerSecond + SpeedSmoothingFactor * instantBytesPerSecond
            : instantBytesPerSecond;
    }

    m_lastProgressReportMs = nowMilliseconds;
    m_lastProgressProcessedBytes = processedBytes;

    ImagingProgress progress;
    progress.stage = stage;
    progress.processedBytes = processedBytes;
    progress.totalBytes = totalBytes;
    progress.bytesPerSecond = m_smoothedBytesPerSecond;
    progress.elapsedMilliseconds = nowMilliseconds;
    progress.remainingMilliseconds = (m_smoothedBytesPerSecond > 1.0 && totalBytes > processedBytes)
        ? static_cast<qint64>(static_cast<double>(totalBytes - processedBytes) / m_smoothedBytesPerSecond * 1000.0)
        : -1;

    if (m_progressCallback)
    {
        m_progressCallback(progress);
    }
}

ImagingResult ImagingJob::makeResult(ImagingStage stage, QString errorMessage) const
{
    ImagingResult result;
    result.operation = m_request.operation;
    result.finalStage = stage;
    result.succeeded = stage == ImagingStage::Complete;
    result.cancelled = stage == ImagingStage::Cancelled;
    result.verificationMatched = m_verificationMatched;
    result.errorMessage = std::move(errorMessage);
    result.imageFilePath = m_request.imageFilePath;
    result.processedBytes = m_processedBytes;
    result.elapsedMilliseconds = m_runTimer.elapsed();
    result.fastDigestHex = m_fastDigestHex;
    result.sha256Hex = m_sha256Hex;

    return result;
}

quint64 ImagingJob::alignedChunkSize(quint32 sectorSizeBytes) const
{
    const quint64 sectorSize = std::max<quint64>(sectorSizeBytes, 1u);
    const quint64 chunkSize = std::max(m_settings.chunkSizeBytes, sectorSize);

    return (chunkSize / sectorSize) * sectorSize;
}
} // namespace UDI
