#include <managers/imagingmanager.hpp>

#include <imaging/imagingjob.hpp>
#include <utils/formatting.hpp>


namespace UDI
{
ImagingManager::ImagingManager()
    : Manager("ImagingManager")
{
    qRegisterMetaType<ImagingRequest>();
    qRegisterMetaType<ImagingProgress>();
    qRegisterMetaType<ImagingResult>();
}

void ImagingManager::requestCancel()
{
    if (!isBusy())
    {
        return;
    }

    qInfo() << "Cancellation requested";

    m_cancelRequested.store(true, std::memory_order_relaxed);
}

void ImagingManager::start(const UDI::ImagingRequest& request)
{
    if (m_busy.exchange(true, std::memory_order_relaxed))
    {
        qWarning() << "An imaging operation is already running; ignoring the new request";
        return;
    }

    m_cancelRequested.store(false, std::memory_order_relaxed);

    Q_EMIT operationStarted(request);

    ImagingJobSettings settings;
    settings.chunkSizeBytes = getConfig().chunkSizeBytes;
    settings.partitionScanSizeBytes = getConfig().partitionScanSizeBytes;
    settings.trimAlignmentBytes = getConfig().trimAlignmentBytes;
    settings.progressIntervalMilliseconds = getConfig().progressIntervalMilliseconds;

    ImagingJob job{ request, settings, [this](const ImagingProgress& progress)
        {
            Q_EMIT progressUpdated(progress);
        },
        m_cancelRequested };

    const ImagingResult result = job.run();

    if (result.succeeded)
    {
        qInfo() << "Operation finished:" << result.processedBytes << "bytes in"
                << formatDuration(result.elapsedMilliseconds) << "digest" << result.fastDigestHex;
    }
    else if (result.cancelled)
    {
        qInfo() << "Operation cancelled after" << result.processedBytes << "bytes";
    }
    else
    {
        qCritical() << "Operation failed:" << result.errorMessage;
    }

    m_busy.store(false, std::memory_order_relaxed);

    Q_EMIT operationFinished(result);
}
} // namespace UDI
