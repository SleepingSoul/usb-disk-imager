#pragma once

#include <atomic>
#include <functional>
#include <memory>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>

#include <disk/partitiontable.hpp>
#include <disk/rawdevice.hpp>
#include <imaging/imagingtypes.hpp>


namespace UDI
{
struct ImagingJobSettings
{
    quint64 chunkSizeBytes{ 4u * 1024u * 1024u };
    // How much of the device head is inspected for a partition table.
    quint64 partitionScanSizeBytes{ 1024u * 1024u };
    // Trimmed MBR images are rounded up to this, so a following partition tool has room to align.
    quint64 trimAlignmentBytes{ 1024u * 1024u };
    int progressIntervalMilliseconds{ 100 };
};

/*
 * One read, write or verify run, start to finish, on the caller's thread.
 *
 * The transfer loop is a plain blocking loop rather than an event-driven pipeline: device I/O at this
 * size is bound by the medium, and a loop that checks a flag between chunks cancels within one chunk
 * without needing its thread's event loop to be free.
 */
class ImagingJob
{
    Q_DISABLE_COPY_MOVE(ImagingJob)
    // Error messages reach the user unchanged, so they are translated here rather than mapped from
    // codes in the view model.
    Q_DECLARE_TR_FUNCTIONS(ImagingJob)
public:
    using ProgressCallback = std::function<void(const ImagingProgress&)>;

    ImagingJob(ImagingRequest request,
        ImagingJobSettings settings,
        ProgressCallback progressCallback,
        const std::atomic_bool& cancelRequested);

    ImagingResult run();

private:
    ImagingResult runRead();
    ImagingResult runWrite();
    ImagingResult runVerify();

    std::unique_ptr<RawDevice> openDevice(RawDevice::AccessMode mode, QString& errorMessage) const;

    // Decides how many bytes to copy, and for a trimmed GPT device what has to be appended and patched
    // so the shorter image still describes itself correctly.
    std::optional<TrimPlan> resolveTrimPlan(RawDevice& device, quint64& totalBytes);
    std::optional<quint64> findLastNonZeroByte(RawDevice& device);
    std::optional<quint64> findLastNonZeroByteInImage(QIODevice& imageFile, quint64 imageSizeBytes);

    // Where the image's last partition ends, which is the earliest a write may stop without leaving
    // stale device bytes inside a partition. The whole image size when that cannot be established.
    quint64 findLastPartitionEndInImage(quint64 imageSizeBytes) const;

    // Parses whatever of MBR and GPT is in \a head, trying the sector sizes real removable media
    // actually use until one produces a validated GPT or, failing that, an MBR at the default of 512 —
    // there is no live device here to just ask, the way resolveTrimPlan() can.
    PartitionTable parseImagePartitionTable(const std::vector<std::uint8_t>& head, quint64 imageSizeBytes) const;
    bool readImageHead(quint64 imageSizeBytes, std::vector<std::uint8_t>& head, QString& errorMessage) const;
    bool looksLikeExtFilesystem(quint64 partitionOffsetBytes, QString& errorMessage) const;

    // Writes a resize plan's patched header/trailer bytes into an already-open image file and resizes it
    // to match — the one piece shrink and grow apply identically once each has its own plan.
    bool applyPartitionResizePlan(QFile& imageFile, const TrimPlan& plan, quint32 sectorSizeBytes) const;

    // Shrinks the ext2/3/4 filesystem in the image's last partition and truncates the image to match,
    // once a read has finished. Recomputes the reported digest, since the file it describes just changed.
    bool shrinkImageFilesystem(QString& errorMessage);

    // Grows the ext2/3/4 filesystem in the image's last partition to fill \a targetSizeBytes, before a
    // write starts, so the write loop that follows sees nothing more unusual than a larger image file.
    bool growImageFilesystem(quint64 targetSizeBytes, QString& errorMessage);

    bool computeFileDigests(QString& errorMessage);

    bool compareDeviceWithImage(RawDevice& device,
        QIODevice& imageFile,
        quint64 sizeToCompare,
        ImagingStage stage,
        quint64 processedBytesBefore,
        quint64 totalProgressBytes,
        QString& errorMessage);

    void reportProgress(ImagingStage stage, quint64 processedBytes, quint64 totalBytes, bool force);
    bool isCancelled() const { return m_cancelRequested.load(std::memory_order_relaxed); }

    ImagingResult makeResult(ImagingStage stage, QString errorMessage) const;

    quint64 alignedChunkSize(quint32 sectorSizeBytes) const;

    ImagingRequest m_request;
    ImagingJobSettings m_settings;
    ProgressCallback m_progressCallback;
    const std::atomic_bool& m_cancelRequested;

    QElapsedTimer m_runTimer;
    qint64 m_lastProgressReportMs{ 0 };
    quint64 m_lastProgressProcessedBytes{ 0 };
    double m_smoothedBytesPerSecond{ 0.0 };

    quint64 m_processedBytes{ 0 };
    QByteArray m_fastDigestHex;
    QByteArray m_sha256Hex;
    bool m_verificationMatched{ false };
};
} // namespace UDI
