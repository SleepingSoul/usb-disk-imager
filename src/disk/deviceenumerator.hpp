#pragma once

#include <QStringList>

#include <disk/disktypes.hpp>


namespace UDI
{
/*
 * Discovery of attached physical storage devices, implemented once per platform.
 *
 * Two rules hold for every backend, because breaking them is how imaging tools end up hanging on
 * machines that run VeraCrypt or any other virtual-volume driver:
 *
 *  - A volume is never opened before it has been positively identified as a partition of a physical
 *    disk. On Windows that means resolving the drive letter with QueryDosDevice and matching
 *    \Device\HarddiskVolume* first; a VeraCrypt volume resolves to \Device\VeraCryptVolumeX and is
 *    skipped without a handle ever being created for it.
 *  - Enumeration asks for metadata only, never for read access to the medium. Property queries need no
 *    administrator rights and cannot spin up, unlock or block on a device.
 *
 * enumerate() still does blocking file-system calls, so it runs on DeviceManager's own thread.
 */
class DeviceEnumerator
{
public:
    struct Result
    {
        DeviceList devices;
        // Already translated, shown in the UI as-is. A QStringList because it crosses a queued signal on
        // its way to a QML property.
        QStringList warnings;
    };

    // Internal disks are always enumerated so the system disk can be recognised, but they are filtered
    // out of the result unless \a includeFixedDisks is set.
    static Result enumerate(bool includeFixedDisks);
};
} // namespace UDI
