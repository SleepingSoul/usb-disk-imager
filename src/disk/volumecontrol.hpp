#pragma once

#include <vector>

#include <QString>

#include <disk/disktypes.hpp>


namespace UDI
{
/*
 * Takes a device out of the operating system's hands for the duration of a write and hands it back
 * afterwards.
 *
 * Windows locks and dismounts every volume of the target disk, then asks the disk driver to re-read the
 * partition table; Linux unmounts the partitions so the subsequent O_EXCL open is what keeps the kernel
 * from writing behind our back; macOS lets `diskutil unmountDisk` detach the whole disk at once.
 *
 * Only volumes that were positively identified as partitions of the selected device are touched. A
 * virtual volume that merely happens to be mounted — a VeraCrypt container, for instance — is never
 * enumerated, so it is never locked, dismounted or waited on.
 */
class VolumeControl
{
    Q_DISABLE_COPY_MOVE(VolumeControl)
public:
    explicit VolumeControl(DeviceInfo device);

    // Releases the locks and then refreshes the layout, so every exit path out of a write — including a
    // failed or cancelled one — leaves the OS looking at what is actually on the device now.
    ~VolumeControl();

    // Leaves the device untouched and explains which volume refused when it returns false.
    bool acquire(QString& errorMessage);

    void release();

    // Tells the OS the on-disk layout changed, so it re-reads the partition table and re-mounts what it
    // finds instead of showing the pre-write filesystems.
    void refreshLayout();

private:
    DeviceInfo m_device;

    // Windows holds one locked handle per dismounted volume until the write finishes; the Unix backends
    // unmount instead and have nothing to hold.
    std::vector<quintptr> m_lockedVolumeHandles;
};
} // namespace UDI
