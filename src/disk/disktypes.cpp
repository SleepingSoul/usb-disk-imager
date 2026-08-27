#include <disk/disktypes.hpp>


namespace UDI
{
using namespace Qt::Literals::StringLiterals;

QString DeviceInfo::getProductName() const
{
    if (!vendor.isEmpty() && !model.isEmpty())
    {
        // Plenty of controllers report the vendor twice, once on its own and once as a prefix of the
        // model, which would otherwise read as "SanDisk SanDisk Ultra".
        if (model.startsWith(vendor, Qt::CaseInsensitive))
        {
            return model;
        }

        return vendor + u' ' + model;
    }

    if (!model.isEmpty())
    {
        return model;
    }

    if (!vendor.isEmpty())
    {
        return vendor;
    }

    return QString::fromUtf8(shortIdentifier);
}

bool DeviceInfo::operator==(const DeviceInfo& other) const
{
    return path == other.path
        && serialNumber == other.serialNumber
        && vendor == other.vendor
        && model == other.model
        && mountPoints == other.mountPoints
        && volumeLabels == other.volumeLabels
        && mountedDevicePaths == other.mountedDevicePaths
        && sizeBytes == other.sizeBytes
        && logicalSectorSizeBytes == other.logicalSectorSizeBytes
        && bus == other.bus
        && removableMedia == other.removableMedia
        && writeProtected == other.writeProtected
        && systemDevice == other.systemDevice;
}

QLatin1StringView busTypeName(BusType bus)
{
    switch (bus)
    {
    case BusType::Usb:     return "USB"_L1;
    case BusType::Sd:      return "SD"_L1;
    case BusType::Mmc:     return "eMMC"_L1;
    case BusType::Nvme:    return "NVMe"_L1;
    case BusType::Sata:    return "SATA"_L1;
    case BusType::Scsi:    return "SCSI"_L1;
    case BusType::Ide:     return "IDE"_L1;
    case BusType::Virtual: return "Virtual"_L1;
    case BusType::Unknown: break;
    }

    return "Unknown"_L1;
}
} // namespace UDI
