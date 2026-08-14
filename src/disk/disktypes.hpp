#pragma once

#include <cstdint>
#include <vector>

#include <QByteArray>
#include <QLatin1StringView>
#include <QMetaType>
#include <QString>


namespace UDI
{
enum class BusType
{
    Unknown,
    Usb,
    Sd,
    Mmc,
    Nvme,
    Sata,
    Scsi,
    Ide,
    Virtual
};

// A whole physical storage device, never a partition. Pure data, so it can be copied across the
// enumeration, imaging and GUI threads.
struct DeviceInfo
{
    // What the raw device is opened by: \\.\PhysicalDrive2, /dev/sdb, /dev/rdisk3.
    QByteArray path;
    QByteArray shortIdentifier;
    QByteArray serialNumber;

    QString vendor;
    QString model;

    std::vector<QString> mountPoints;
    std::vector<QString> volumeLabels;

    quint64 sizeBytes{ 0 };
    quint32 logicalSectorSizeBytes{ 512 };

    BusType bus{ BusType::Unknown };

    bool removableMedia{ false };
    bool writeProtected{ false };
    // Hosts the running operating system, so it is never offered as a write target.
    bool systemDevice{ false };

    bool isValid() const { return !path.isEmpty() && sizeBytes > 0; }

    QString getProductName() const;

    bool operator==(const DeviceInfo& other) const;
    bool operator!=(const DeviceInfo& other) const { return !(*this == other); }
};

using DeviceList = std::vector<DeviceInfo>;

QLatin1StringView busTypeName(BusType bus);
} // namespace UDI

Q_DECLARE_METATYPE(UDI::DeviceInfo)
Q_DECLARE_METATYPE(UDI::DeviceList)
