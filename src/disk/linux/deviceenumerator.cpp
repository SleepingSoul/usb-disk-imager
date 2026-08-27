#include <disk/deviceenumerator.hpp>

#include <algorithm>
#include <array>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>


namespace UDI
{
using namespace Qt::Literals::StringLiterals;

namespace
{

const QLatin1StringView SysBlockDirectory{ "/sys/block" };
const QLatin1StringView MountsFilePath{ "/proc/self/mounts" };
const QLatin1StringView DevicesDirectory{ "/dev" };

// Sysfs reports capacities in 512-byte units regardless of the device's real sector size.
constexpr quint64 SysfsSizeUnitBytes = 512;

constexpr int MaximumSerialSearchDepth = 6;

// Loop, ramdisk and device-mapper nodes are either virtual or not something anyone images. VeraCrypt on
// Linux surfaces as a device-mapper node, so it never reaches the device list.
const std::array IgnoredNamePrefixes{
    "loop"_L1, "ram"_L1, "zram"_L1, "zd"_L1, "dm-"_L1, "md"_L1, "sr"_L1, "fd"_L1, "nbd"_L1
};

const std::array SystemMountPoints{ "/"_L1, "/boot"_L1, "/boot/efi"_L1, "/usr"_L1, "/var"_L1 };

struct MountEntry
{
    QString devicePath;
    QString mountPoint;
};

QByteArray readSysfsValue(const QString& filePath)
{
    QFile file{ filePath };

    if (!file.open(QIODevice::ReadOnly))
    {
        return QByteArray{};
    }

    return file.readLine().trimmed();
}

quint64 readSysfsUnsigned(const QString& filePath, quint64 fallbackValue)
{
    bool parsed = false;
    const quint64 value = readSysfsValue(filePath).toULongLong(&parsed);

    return parsed ? value : fallbackValue;
}

bool isIgnoredDeviceName(const QString& deviceName)
{
    return std::any_of(IgnoredNamePrefixes.cbegin(), IgnoredNamePrefixes.cend(), [&deviceName](QLatin1StringView prefix)
    {
        return deviceName.startsWith(prefix);
    });
}

// /proc/self/mounts escapes anything that would otherwise break its field separation.
QString unescapeMountField(const QString& field)
{
    QString result;
    result.reserve(field.size());

    for (qsizetype index = 0; index < field.size(); ++index)
    {
        if (field.at(index) == u'\\' && index + 3 < field.size())
        {
            bool parsed = false;
            const int code = QStringView{ field }.mid(index + 1, 3).toInt(&parsed, 8);

            if (parsed)
            {
                result.append(QChar{ static_cast<char16_t>(code) });
                index += 3;
                continue;
            }
        }

        result.append(field.at(index));
    }

    return result;
}

std::vector<MountEntry> readMountTable()
{
    std::vector<MountEntry> entries;

    QFile mounts{ MountsFilePath };

    if (!mounts.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "Could not read" << MountsFilePath << ':' << mounts.errorString();
        return entries;
    }

    while (!mounts.atEnd())
    {
        const QString line = QString::fromUtf8(mounts.readLine());
        const QStringList fields = line.split(u' ', Qt::SkipEmptyParts);

        if (fields.size() < 2 || !fields.at(0).startsWith(DevicesDirectory))
        {
            continue;
        }

        MountEntry entry;
        // A mount source can be a symlink such as /dev/disk/by-uuid/..., which has to be resolved before
        // it can be matched against the device nodes sysfs reports.
        entry.devicePath = QFileInfo{ unescapeMountField(fields.at(0)) }.canonicalFilePath();
        entry.mountPoint = unescapeMountField(fields.at(1));

        if (!entry.devicePath.isEmpty())
        {
            entries.push_back(std::move(entry));
        }
    }

    return entries;
}

std::vector<QString> readPartitionNodes(const QString& deviceName)
{
    std::vector<QString> nodes;

    const QDir deviceDirectory{ QDir{ SysBlockDirectory }.filePath(deviceName) };

    for (const QString& entryName : deviceDirectory.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
    {
        if (QFileInfo::exists(deviceDirectory.filePath(entryName + "/partition"_L1)))
        {
            nodes.push_back(QDir{ DevicesDirectory }.filePath(entryName));
        }
    }

    return nodes;
}

BusType detectBusType(const QString& deviceName, const QString& sysfsDevicePath, const QString& canonicalPath)
{
    if (deviceName.startsWith("nvme"_L1) || canonicalPath.contains("/nvme"_L1))
    {
        return BusType::Nvme;
    }

    if (canonicalPath.contains("/usb"_L1))
    {
        return BusType::Usb;
    }

    if (canonicalPath.contains("/mmc_host/"_L1))
    {
        // eMMC is soldered down while an SD card is not, and only the card should be offered by default.
        return readSysfsValue(sysfsDevicePath + "/device/type"_L1) == QByteArrayLiteral("SD")
            ? BusType::Sd
            : BusType::Mmc;
    }

    if (canonicalPath.contains("/virtio"_L1))
    {
        return BusType::Virtual;
    }

    if (canonicalPath.contains("/ata"_L1))
    {
        return BusType::Sata;
    }

    if (canonicalPath.contains("/host"_L1))
    {
        return BusType::Scsi;
    }

    return BusType::Unknown;
}

QByteArray findSerialNumber(const QString& canonicalDevicePath)
{
    // A USB stick's serial belongs to the USB device a few levels above the block device, so walk up
    // until a serial attribute shows up.
    QDir directory{ canonicalDevicePath };

    for (int depth = 0; depth < MaximumSerialSearchDepth; ++depth)
    {
        const QString candidate = directory.filePath("serial"_L1);

        if (QFileInfo::exists(candidate))
        {
            return readSysfsValue(candidate);
        }

        if (!directory.cdUp())
        {
            break;
        }
    }

    return QByteArray{};
}

QString readModelName(const QString& sysfsDevicePath)
{
    const std::array candidateFiles{ "/device/model"_L1, "/device/name"_L1 };

    for (const QLatin1StringView candidate : candidateFiles)
    {
        const QByteArray value = readSysfsValue(sysfsDevicePath + candidate);

        if (!value.isEmpty())
        {
            return QString::fromUtf8(value).trimmed();
        }
    }

    return QString{};
}
} // namespace

DeviceEnumerator::Result DeviceEnumerator::enumerate(bool includeFixedDisks)
{
    Result result;

    const QDir sysBlock{ SysBlockDirectory };

    if (!sysBlock.exists())
    {
        result.warnings.push_back(QCoreApplication::translate("DeviceEnumerator",
            "%1 is not available, so no devices could be found.").arg(SysBlockDirectory));

        return result;
    }

    const std::vector<MountEntry> mountTable = readMountTable();

    for (const QString& deviceName : sysBlock.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
    {
        if (isIgnoredDeviceName(deviceName))
        {
            continue;
        }

        const QString sysfsDevicePath = sysBlock.filePath(deviceName);
        const QString canonicalPath = QFileInfo{ sysfsDevicePath }.canonicalFilePath();

        DeviceInfo device;
        device.shortIdentifier = deviceName.toUtf8();
        device.path = QDir{ DevicesDirectory }.filePath(deviceName).toUtf8();
        device.sizeBytes = readSysfsUnsigned(sysfsDevicePath + "/size"_L1, 0) * SysfsSizeUnitBytes;

        if (device.sizeBytes == 0)
        {
            continue;
        }

        device.logicalSectorSizeBytes =
            static_cast<quint32>(readSysfsUnsigned(sysfsDevicePath + "/queue/logical_block_size"_L1, 512));
        device.writeProtected = readSysfsUnsigned(sysfsDevicePath + "/ro"_L1, 0) != 0;
        device.bus = detectBusType(deviceName, sysfsDevicePath, canonicalPath);
        device.vendor = QString::fromUtf8(readSysfsValue(sysfsDevicePath + "/device/vendor"_L1)).trimmed();
        device.model = readModelName(sysfsDevicePath);
        device.serialNumber = findSerialNumber(canonicalPath);

        const bool hotPluggableBus = device.bus == BusType::Usb || device.bus == BusType::Sd;
        device.removableMedia = readSysfsUnsigned(sysfsDevicePath + "/removable"_L1, 0) != 0 || hotPluggableBus;

        std::vector<QString> deviceNodes = readPartitionNodes(deviceName);
        deviceNodes.push_back(QString::fromUtf8(device.path));

        for (const MountEntry& mountEntry : mountTable)
        {
            const bool belongsToDevice = std::any_of(deviceNodes.cbegin(), deviceNodes.cend(),
                [&mountEntry](const QString& node)
                {
                    return node == mountEntry.devicePath;
                });

            if (!belongsToDevice)
            {
                continue;
            }

            device.mountPoints.push_back(mountEntry.mountPoint);
            device.volumeLabels.push_back(QString{});
            device.mountedDevicePaths.push_back(mountEntry.devicePath);

            const bool isSystemMountPoint = std::any_of(SystemMountPoints.cbegin(), SystemMountPoints.cend(),
                [&mountEntry](QLatin1StringView systemMountPoint)
                {
                    return mountEntry.mountPoint == systemMountPoint;
                });

            if (isSystemMountPoint)
            {
                device.systemDevice = true;
            }
        }

        if (!device.removableMedia && !includeFixedDisks)
        {
            continue;
        }

        result.devices.push_back(std::move(device));
    }

    return result;
}
} // namespace UDI
