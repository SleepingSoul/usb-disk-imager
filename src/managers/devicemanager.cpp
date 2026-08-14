#include <managers/devicemanager.hpp>

#include <utility>

#include <disk/deviceenumerator.hpp>
#include <utils/usersettings.hpp>


namespace UDI
{
DeviceManager::DeviceManager()
    : Manager("DeviceManager")
{
    qRegisterMetaType<DeviceInfo>();
    qRegisterMetaType<DeviceList>();

    m_includeFixedDisks.store(UserSettings::getIncludeFixedDisks()
        || getConfig().includeFixedDisksByDefault, std::memory_order_relaxed);
}

void DeviceManager::postInit()
{
    // Queued onto our own event loop so the timer is created and owned by device_scan_thread.
    QMetaObject::invokeMethod(this, &DeviceManager::initAll, Qt::QueuedConnection);
}

void DeviceManager::setIncludeFixedDisks(bool includeFixedDisks)
{
    m_includeFixedDisks.store(includeFixedDisks, std::memory_order_relaxed);
}

void DeviceManager::initAll()
{
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(getConfig().refreshIntervalMilliseconds);

    connect(m_refreshTimer, &QTimer::timeout, this, &DeviceManager::refresh);

    refresh();

    m_refreshTimer->start();
}

void DeviceManager::refresh()
{
    DeviceEnumerator::Result result =
        DeviceEnumerator::enumerate(m_includeFixedDisks.load(std::memory_order_relaxed));

    // The scan runs a few times a second; re-emitting an identical list would rebuild the QML delegates
    // and fight the user's selection for nothing.
    if (result.devices == m_lastDevices && result.warnings == m_lastWarnings)
    {
        return;
    }

    m_lastDevices = std::move(result.devices);
    m_lastWarnings = std::move(result.warnings);

    qInfo() << "Device list changed:" << m_lastDevices.size() << "device(s)";

    Q_EMIT devicesUpdated(m_lastDevices, m_lastWarnings);
}
} // namespace UDI
