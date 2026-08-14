#include <disk/deviceenumerator.hpp>

#include <algorithm>
#include <array>
#include <optional>

#include <QCoreApplication>

#include <disk/windows/windowsutils.hpp>

#include <winioctl.h>


namespace UDI
{
using namespace Qt::Literals::StringLiterals;
using Windows::ScopedHandle;

namespace
{

constexpr int MaximumPhysicalDriveIndex = 63;
constexpr int DriveLetterCount = 26;
constexpr DWORD DescriptorBufferSize = 1024;
constexpr DWORD ExtentsBufferSize = sizeof(VOLUME_DISK_EXTENTS) + 63 * sizeof(DISK_EXTENT);

// A drive letter belonging to a partition of a physical disk always resolves to this. VeraCrypt volumes
// resolve to \Device\VeraCryptVolumeX, network drives to \Device\LanmanRedirector, and subst'ed
// directories to \??\C:\..., all of which must stay untouched.
const QLatin1StringView PhysicalVolumeTargetPrefix{ "\\Device\\HarddiskVolume" };

struct VolumeOnDisk
{
    DWORD diskNumber{ 0 };
    QString mountPoint;
    QString label;
};

BusType mapBusType(STORAGE_BUS_TYPE busType)
{
    switch (busType)
    {
    case BusTypeUsb:                return BusType::Usb;
    case BusTypeSd:                 return BusType::Sd;
    case BusTypeMmc:                return BusType::Mmc;
    case BusTypeNvme:               return BusType::Nvme;
    case BusTypeSata:               return BusType::Sata;
    case BusTypeScsi:
    case BusTypeSas:
    case BusTypeiScsi:
    case BusTypeRAID:               return BusType::Scsi;
    case BusTypeAta:                return BusType::Ide;
    case BusTypeVirtual:
    case BusTypeFileBackedVirtual:  return BusType::Virtual;
    default:                        break;
    }

    return BusType::Unknown;
}

std::optional<DeviceInfo> queryPhysicalDrive(int driveIndex)
{
    DeviceInfo device;
    device.shortIdentifier = QByteArrayLiteral("PhysicalDrive") + QByteArray::number(driveIndex);
    device.path = QByteArrayLiteral("\\\\.\\") + device.shortIdentifier;

    // Access mask 0 asks for metadata only. It needs no administrator rights and, unlike a read handle,
    // cannot spin up or block on the medium.
    const std::wstring widePath = Windows::toWideString(device.path);
    const ScopedHandle handle{ CreateFileW(widePath.c_str(),
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr) };

    if (!handle.isValid())
    {
        return std::nullopt;
    }

    STORAGE_PROPERTY_QUERY query{};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;

    std::array<std::uint8_t, DescriptorBufferSize> descriptorBuffer{};
    DWORD bytesReturned = 0;

    if (DeviceIoControl(handle.get(),
            IOCTL_STORAGE_QUERY_PROPERTY,
            &query,
            sizeof(query),
            descriptorBuffer.data(),
            static_cast<DWORD>(descriptorBuffer.size()),
            &bytesReturned,
            nullptr))
    {
        const auto* const descriptor =
            reinterpret_cast<const STORAGE_DEVICE_DESCRIPTOR*>(descriptorBuffer.data());

        device.bus = mapBusType(descriptor->BusType);
        device.vendor = Windows::readDescriptorString(descriptorBuffer.data(), descriptor->VendorIdOffset, bytesReturned);
        device.model = Windows::readDescriptorString(descriptorBuffer.data(), descriptor->ProductIdOffset, bytesReturned);
        device.serialNumber =
            Windows::readDescriptorString(descriptorBuffer.data(), descriptor->SerialNumberOffset, bytesReturned).toLatin1();

        // A USB SSD reports RemovableMedia = FALSE, yet it is exactly the kind of device this app is
        // for, so the hot-pluggable buses count as removable too.
        const bool hotPluggableBus = device.bus == BusType::Usb
            || device.bus == BusType::Sd
            || device.bus == BusType::Mmc;

        device.removableMedia = descriptor->RemovableMedia != FALSE || hotPluggableBus;
    }
    else
    {
        qDebug() << "Could not query storage properties of" << device.path << ':' << Windows::formatLastSystemError();
    }

    std::array<std::uint8_t, sizeof(DISK_GEOMETRY_EX) + 512> geometryBuffer{};

    if (!DeviceIoControl(handle.get(),
            IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
            nullptr,
            0,
            geometryBuffer.data(),
            static_cast<DWORD>(geometryBuffer.size()),
            &bytesReturned,
            nullptr))
    {
        // No media in a card reader lands here, which is why this is not worth a warning.
        qDebug() << "No geometry for" << device.path << ':' << Windows::formatLastSystemError();
        return std::nullopt;
    }

    const auto* const geometry = reinterpret_cast<const DISK_GEOMETRY_EX*>(geometryBuffer.data());
    device.sizeBytes = static_cast<quint64>(geometry->DiskSize.QuadPart);
    device.logicalSectorSizeBytes = geometry->Geometry.BytesPerSector > 0
        ? geometry->Geometry.BytesPerSector
        : 512u;

    if (device.sizeBytes == 0)
    {
        return std::nullopt;
    }

    if (!DeviceIoControl(handle.get(), IOCTL_DISK_IS_WRITABLE, nullptr, 0, nullptr, 0, &bytesReturned, nullptr))
    {
        device.writeProtected = GetLastError() == ERROR_WRITE_PROTECT;
    }

    return device;
}

QString readVolumeLabel(const std::wstring& volumeRoot)
{
    std::array<wchar_t, MAX_PATH + 1> label{};

    if (!GetVolumeInformationW(volumeRoot.c_str(),
            label.data(),
            static_cast<DWORD>(label.size()),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            0))
    {
        return QString{};
    }

    return QString::fromWCharArray(label.data()).trimmed();
}

std::vector<VolumeOnDisk> enumerateVolumes()
{
    std::vector<VolumeOnDisk> volumes;

    const DWORD driveMask = GetLogicalDrives();

    for (int letterIndex = 0; letterIndex < DriveLetterCount; ++letterIndex)
    {
        if ((driveMask & (1u << letterIndex)) == 0)
        {
            continue;
        }

        const wchar_t driveLetter = static_cast<wchar_t>(L'A' + letterIndex);
        const std::wstring dosDeviceName{ driveLetter, L':' };

        // QueryDosDevice resolves the letter without opening anything. This is the check that keeps
        // virtual volumes out of the enumeration: they are recognised and skipped before any handle
        // exists, so their driver is never asked to do work it might not come back from.
        std::array<wchar_t, 512> deviceTarget{};
        if (QueryDosDeviceW(dosDeviceName.c_str(), deviceTarget.data(), static_cast<DWORD>(deviceTarget.size())) == 0)
        {
            continue;
        }

        const QString target = QString::fromWCharArray(deviceTarget.data());

        if (!target.startsWith(PhysicalVolumeTargetPrefix))
        {
            qDebug() << "Skipping drive" << QChar{ driveLetter } << "- it resolves to" << target
                     << "rather than a partition of a physical disk";
            continue;
        }

        const std::wstring volumeRoot = dosDeviceName + L'\\';
        const UINT driveType = GetDriveTypeW(volumeRoot.c_str());

        if (driveType != DRIVE_REMOVABLE && driveType != DRIVE_FIXED)
        {
            continue;
        }

        const std::wstring volumePath = L"\\\\.\\" + dosDeviceName;
        const ScopedHandle volumeHandle{ CreateFileW(volumePath.c_str(),
            0,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr) };

        if (!volumeHandle.isValid())
        {
            continue;
        }

        std::array<std::uint8_t, ExtentsBufferSize> extentsBuffer{};
        DWORD bytesReturned = 0;

        if (!DeviceIoControl(volumeHandle.get(),
                IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                nullptr,
                0,
                extentsBuffer.data(),
                static_cast<DWORD>(extentsBuffer.size()),
                &bytesReturned,
                nullptr))
        {
            continue;
        }

        const auto* const extents = reinterpret_cast<const VOLUME_DISK_EXTENTS*>(extentsBuffer.data());
        const QString mountPoint = QString{ QChar{ driveLetter } } + ":"_L1;
        const QString label = readVolumeLabel(volumeRoot);

        for (DWORD extentIndex = 0; extentIndex < extents->NumberOfDiskExtents; ++extentIndex)
        {
            volumes.push_back(VolumeOnDisk{ extents->Extents[extentIndex].DiskNumber, mountPoint, label });
        }
    }

    return volumes;
}

std::optional<DWORD> findSystemDiskNumber(const std::vector<VolumeOnDisk>& volumes)
{
    std::array<wchar_t, MAX_PATH + 1> windowsDirectory{};

    if (GetWindowsDirectoryW(windowsDirectory.data(), static_cast<UINT>(windowsDirectory.size())) == 0)
    {
        return std::nullopt;
    }

    const QString systemMountPoint = QString::fromWCharArray(windowsDirectory.data()).left(2);

    const auto match = std::find_if(volumes.cbegin(), volumes.cend(), [&systemMountPoint](const VolumeOnDisk& volume)
    {
        return volume.mountPoint.compare(systemMountPoint, Qt::CaseInsensitive) == 0;
    });

    return match != volumes.cend() ? std::optional<DWORD>{ match->diskNumber } : std::nullopt;
}
} // namespace

DeviceEnumerator::Result DeviceEnumerator::enumerate(bool includeFixedDisks)
{
    Result result;

    const std::vector<VolumeOnDisk> volumes = enumerateVolumes();
    const std::optional<DWORD> systemDiskNumber = findSystemDiskNumber(volumes);

    if (!systemDiskNumber)
    {
        result.warnings.push_back(QCoreApplication::translate("DeviceEnumerator",
            "The disk holding Windows could not be identified, so no disk is marked as the system disk."));
    }

    for (int driveIndex = 0; driveIndex <= MaximumPhysicalDriveIndex; ++driveIndex)
    {
        auto device = queryPhysicalDrive(driveIndex);

        if (!device)
        {
            continue;
        }

        const DWORD diskNumber = static_cast<DWORD>(driveIndex);

        device->systemDevice = systemDiskNumber && *systemDiskNumber == diskNumber;

        for (const VolumeOnDisk& volume : volumes)
        {
            if (volume.diskNumber != diskNumber)
            {
                continue;
            }

            device->mountPoints.push_back(volume.mountPoint);
            device->volumeLabels.push_back(volume.label);
        }

        if (!device->removableMedia && !includeFixedDisks)
        {
            continue;
        }

        result.devices.push_back(std::move(*device));
    }

    return result;
}
} // namespace UDI
