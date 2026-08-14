#pragma once

#include <atomic>
#include <functional>
#include <memory>

#include <QCoreApplication>
#include <QElapsedTimer>

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
