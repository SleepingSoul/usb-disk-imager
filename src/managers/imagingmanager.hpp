#pragma once

#include <atomic>

#include <app/manager.hpp>
#include <configs/imagingmanagerconfig.hpp>
#include <imaging/imagingtypes.hpp>


namespace UDI
{
/*
 * Owns the read, write and verify runs and lives on imaging_thread.
 *
 * start() blocks its thread for the whole transfer — minutes, for a full card — which is precisely why
 * the manager has a thread of its own. Progress arrives on the GUI thread as a throttled queued signal.
 */
class ImagingManager : public Manager<ImagingManager, ImagingManagerConfig>
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ImagingManager)
public:
    ImagingManager();

    bool isBusy() const { return m_busy.load(std::memory_order_relaxed); }

    /*
     * Deliberately a plain method rather than a slot, and the one place where calling into a manager from
     * another thread is right: a queued call could not be delivered while start() is inside its transfer
     * loop, and the loop only needs to observe an atomic flag between chunks.
     */
    void requestCancel();

public Q_SLOTS:
    void start(const UDI::ImagingRequest& request);

Q_SIGNALS:
    void operationStarted(const UDI::ImagingRequest& request);
    void progressUpdated(const UDI::ImagingProgress& progress);
    void operationFinished(const UDI::ImagingResult& result);

private:
    std::atomic_bool m_cancelRequested{ false };
    std::atomic_bool m_busy{ false };
};
} // namespace UDI
