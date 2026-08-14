#include <disk/volumecontrol.hpp>

#include <QCoreApplication>
#include <QThread>

#include <disk/windows/windowsutils.hpp>

#include <winioctl.h>


namespace UDI
{
using Windows::ScopedHandle;

namespace
{

// A volume that was in use a moment ago usually frees up as soon as Explorer notices; a few short
// retries turn "the drive is busy" into a successful write far more often than failing straight away.
constexpr int LockAttemptCount = 6;
constexpr int LockRetryDelayMilliseconds = 250;

bool lockAndDismountVolume(HANDLE volumeHandle)
{
    DWORD bytesReturned = 0;

    for (int attempt = 0; attempt < LockAttemptCount; ++attempt)
    {
        if (DeviceIoControl(volumeHandle, FSCTL_LOCK_VOLUME, nullptr, 0, nullptr, 0, &bytesReturned, nullptr))
        {
            return DeviceIoControl(volumeHandle, FSCTL_DISMOUNT_VOLUME, nullptr, 0, nullptr, 0, &bytesReturned, nullptr);
        }

        QThread::msleep(LockRetryDelayMilliseconds);
    }

    return false;
}
} // namespace

VolumeControl::VolumeControl(DeviceInfo device)
    : m_device(std::move(device))
{}

VolumeControl::~VolumeControl()
{
    const bool hadLocks = !m_lockedVolumeHandles.empty();

    release();

    if (hadLocks)
    {
        refreshLayout();
    }
}

bool VolumeControl::acquire(QString& errorMessage)
{
    for (const QString& mountPoint : m_device.mountPoints)
    {
        const std::wstring volumePath = L"\\\\.\\" + Windows::toWideString(mountPoint);

        ScopedHandle volumeHandle{ CreateFileW(volumePath.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr) };

        if (!volumeHandle.isValid())
        {
            const DWORD errorCode = GetLastError();

            release();

            if (errorCode == ERROR_ACCESS_DENIED)
            {
                errorMessage = QCoreApplication::translate("VolumeControl",
                    "Access to volume %1 was denied. Writing to a disk needs administrator rights.")
                    .arg(mountPoint);
            }
            else
            {
                errorMessage = QCoreApplication::translate("VolumeControl",
                    "Could not open volume %1: %2").arg(mountPoint, Windows::formatSystemError(errorCode));
            }

            return false;
        }

        if (!lockAndDismountVolume(volumeHandle.get()))
        {
            const QString systemError = Windows::formatLastSystemError();

            release();

            errorMessage = QCoreApplication::translate("VolumeControl",
                "Volume %1 is in use and could not be taken offline (%2). Close any program using it and "
                "try again.").arg(mountPoint, systemError);

            return false;
        }

        qInfo() << "Locked and dismounted volume" << mountPoint << "of" << m_device.path;

        m_lockedVolumeHandles.push_back(reinterpret_cast<quintptr>(volumeHandle.release()));
    }

    return true;
}

void VolumeControl::release()
{
    for (const quintptr handleValue : m_lockedVolumeHandles)
    {
        CloseHandle(reinterpret_cast<HANDLE>(handleValue));
    }

    m_lockedVolumeHandles.clear();
}

void VolumeControl::refreshLayout()
{
    const std::wstring widePath = Windows::toWideString(m_device.path);

    const ScopedHandle handle{ CreateFileW(widePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr) };

    if (!handle.isValid())
    {
        qWarning() << "Could not reopen" << m_device.path << "to refresh its partition table:"
                   << Windows::formatLastSystemError();
        return;
    }

    DWORD bytesReturned = 0;

    if (!DeviceIoControl(handle.get(), IOCTL_DISK_UPDATE_PROPERTIES, nullptr, 0, nullptr, 0, &bytesReturned, nullptr))
    {
        qWarning() << "Windows refused to re-read the partition table of" << m_device.path << ':'
                   << Windows::formatLastSystemError();
    }
}
} // namespace UDI
