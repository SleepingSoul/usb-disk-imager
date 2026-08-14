#pragma once

#include <atomic>

#include <QStringList>
#include <QTimer>

#include <app/manager.hpp>
#include <configs/devicemanagerconfig.hpp>
#include <disk/disktypes.hpp>


namespace UDI
{
/*
 * Owns device discovery and lives on device_scan_thread.
 *
 * Enumeration is off the GUI thread because it is the one part of the app that talks to whatever storage
 * drivers happen to be loaded: a card reader with no card, a device that has just been yanked out or a
 * third-party volume driver can all take their time answering, and none of that may reach the UI.
 */
class DeviceManager : public Manager<DeviceManager, DeviceManagerConfig>
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(DeviceManager)
public:
    DeviceManager();

    void postInit() override;

    // Only writes an atomic, so it is safe to call from the GUI thread; the next scan picks it up.
    void setIncludeFixedDisks(bool includeFixedDisks);

public Q_SLOTS:
    void refresh();

Q_SIGNALS:
    void devicesUpdated(const UDI::DeviceList& devices, const QStringList& warnings);

private Q_SLOTS:
    void initAll();

private:
    QTimer* m_refreshTimer{ nullptr };
    std::atomic_bool m_includeFixedDisks{ false };
    DeviceList m_lastDevices;
    QStringList m_lastWarnings;
};
} // namespace UDI
