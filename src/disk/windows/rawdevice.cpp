#include <disk/rawdevice.hpp>

#include <algorithm>
#include <array>

#include <QCoreApplication>

#include <disk/windows/windowsutils.hpp>

#include <winioctl.h>


namespace UDI
{
using Windows::ScopedHandle;

namespace
{

// ReadFile and WriteFile take a DWORD length; capping well below that also keeps a single stalled
// transfer short enough that cancelling stays responsive.
constexpr qint64 MaximumSingleTransferBytes = 32ll * 1024ll * 1024ll;

class WindowsRawDevice : public RawDevice
{
public:
    WindowsRawDevice(ScopedHandle handle, quint64 sizeBytes, quint32 sectorSizeBytes)
        : m_handle(std::move(handle))
        , m_sizeBytes(sizeBytes)
        , m_sectorSizeBytes(sectorSizeBytes)
    {}

    qint64 read(std::uint8_t* data, qint64 sizeBytes) override
    {
        qint64 transferred = 0;

        while (transferred < sizeBytes)
        {
            const DWORD requested =
                static_cast<DWORD>(std::min(sizeBytes - transferred, MaximumSingleTransferBytes));
            DWORD received = 0;

            if (!ReadFile(m_handle.get(), data + transferred, requested, &received, nullptr))
            {
                setLastError(Windows::formatLastSystemError());
                return -1;
            }

            if (received == 0)
            {
                break;
            }

            transferred += received;
        }

        return transferred;
    }

    qint64 write(const std::uint8_t* data, qint64 sizeBytes) override
    {
        qint64 transferred = 0;

        while (transferred < sizeBytes)
        {
            const DWORD requested =
                static_cast<DWORD>(std::min(sizeBytes - transferred, MaximumSingleTransferBytes));
            DWORD written = 0;

            if (!WriteFile(m_handle.get(), data + transferred, requested, &written, nullptr))
            {
                setLastError(Windows::formatLastSystemError());
                return -1;
            }

            if (written == 0)
            {
                setLastError(QCoreApplication::translate("RawDevice",
                    "The device accepted no data — it may have been removed."));
                return -1;
            }

            transferred += written;
        }

        return transferred;
    }

    bool seek(quint64 offsetBytes) override
    {
        LARGE_INTEGER offset{};
        offset.QuadPart = static_cast<LONGLONG>(offsetBytes);

        if (!SetFilePointerEx(m_handle.get(), offset, nullptr, FILE_BEGIN))
        {
            setLastError(Windows::formatLastSystemError());
            return false;
        }

        return true;
    }

    bool sync() override
    {
        if (!FlushFileBuffers(m_handle.get()))
        {
            setLastError(Windows::formatLastSystemError());
            return false;
        }

        return true;
    }

    quint64 getSizeBytes() const override { return m_sizeBytes; }
    quint32 getSectorSizeBytes() const override { return m_sectorSizeBytes; }

private:
    ScopedHandle m_handle;
    quint64 m_sizeBytes{ 0 };
    quint32 m_sectorSizeBytes{ 512 };
};

quint64 queryDeviceSize(HANDLE handle, quint64 fallbackSizeBytes)
{
    GET_LENGTH_INFORMATION lengthInformation{};
    DWORD bytesReturned = 0;

    if (DeviceIoControl(handle,
            IOCTL_DISK_GET_LENGTH_INFO,
            nullptr,
            0,
            &lengthInformation,
            sizeof(lengthInformation),
            &bytesReturned,
            nullptr))
    {
        return static_cast<quint64>(lengthInformation.Length.QuadPart);
    }

    return fallbackSizeBytes;
}

quint32 querySectorSize(HANDLE handle, quint32 fallbackSectorSizeBytes)
{
    std::array<std::uint8_t, sizeof(DISK_GEOMETRY_EX) + 512> geometryBuffer{};
    DWORD bytesReturned = 0;

    if (DeviceIoControl(handle,
            IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
            nullptr,
            0,
            geometryBuffer.data(),
            static_cast<DWORD>(geometryBuffer.size()),
            &bytesReturned,
            nullptr))
    {
        const auto* const geometry = reinterpret_cast<const DISK_GEOMETRY_EX*>(geometryBuffer.data());

        if (geometry->Geometry.BytesPerSector > 0)
        {
            return geometry->Geometry.BytesPerSector;
        }
    }

    return fallbackSectorSizeBytes;
}
} // namespace

std::unique_ptr<RawDevice> RawDevice::open(const DeviceInfo& device, AccessMode mode, QString& errorMessage)
{
    const std::wstring widePath = Windows::toWideString(device.path);

    const DWORD access = mode == AccessMode::Read ? GENERIC_READ : (GENERIC_READ | GENERIC_WRITE);

    // No buffering keeps a multi-gigabyte transfer from evicting the user's page cache and makes every
    // completed write a write to the medium rather than to RAM; write-through additionally defeats the
    // device's own cache being reported as done.
    const DWORD flags = mode == AccessMode::Read
        ? FILE_FLAG_NO_BUFFERING
        : (FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH);

    ScopedHandle handle{ CreateFileW(widePath.c_str(),
        access,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        flags,
        nullptr) };

    if (!handle.isValid())
    {
        const DWORD errorCode = GetLastError();

        if (errorCode == ERROR_ACCESS_DENIED)
        {
            errorMessage = QCoreApplication::translate("RawDevice",
                "Access to %1 was denied. Raw disk access needs administrator rights.")
                .arg(QString::fromUtf8(device.path));
        }
        else
        {
            errorMessage = QCoreApplication::translate("RawDevice", "Could not open %1: %2")
                .arg(QString::fromUtf8(device.path), Windows::formatSystemError(errorCode));
        }

        return nullptr;
    }

    const quint64 sizeBytes = queryDeviceSize(handle.get(), device.sizeBytes);
    const quint32 sectorSizeBytes = querySectorSize(handle.get(), device.logicalSectorSizeBytes);

    return std::make_unique<WindowsRawDevice>(std::move(handle), sizeBytes, sectorSizeBytes);
}
} // namespace UDI
