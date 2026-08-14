#include <disk/deviceenumerator.hpp>

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

#include <QCoreApplication>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOMedia.h>

#include <sys/mount.h>


namespace UDI
{
using namespace Qt::Literals::StringLiterals;

namespace
{

const QLatin1StringView RawDevicePrefix{ "/dev/r" };
const QLatin1StringView BlockDevicePrefix{ "/dev/" };

constexpr int MaximumParentSearchDepth = 16;

const std::array SystemMountPoints{ "/"_L1, "/System/Volumes/Data"_L1, "/private/var"_L1 };

// Retains the entry it is given so the parent walk can release uniformly.
class ScopedIoObject
{
    Q_DISABLE_COPY(ScopedIoObject)
public:
    explicit ScopedIoObject(io_object_t object = IO_OBJECT_NULL)
        : m_object(object)
    {}

    ScopedIoObject(ScopedIoObject&& other) noexcept
        : m_object(std::exchange(other.m_object, IO_OBJECT_NULL))
    {}

    ScopedIoObject& operator=(ScopedIoObject&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            m_object = std::exchange(other.m_object, IO_OBJECT_NULL);
        }

        return *this;
    }

    ~ScopedIoObject() { reset(); }

    io_object_t get() const { return m_object; }
    bool isValid() const { return m_object != IO_OBJECT_NULL; }

    void reset(io_object_t object = IO_OBJECT_NULL)
    {
        if (m_object != IO_OBJECT_NULL)
        {
            IOObjectRelease(m_object);
        }

        m_object = object;
    }

private:
    io_object_t m_object{ IO_OBJECT_NULL };
};

QString toQString(CFStringRef value)
{
    if (!value)
    {
        return QString{};
    }

    const CFIndex length = CFStringGetLength(value);

    if (length <= 0)
    {
        return QString{};
    }

    std::vector<UniChar> characters(static_cast<std::size_t>(length));
    CFStringGetCharacters(value, CFRangeMake(0, length), characters.data());

    return QString{ reinterpret_cast<const QChar*>(characters.data()), static_cast<qsizetype>(length) };
}

QString copyStringProperty(io_registry_entry_t entry, CFStringRef key)
{
    // MACH_PORT_NULL stands for the default IOKit port on every macOS release, which avoids having to
    // pick between the deprecated master-port name and its replacement.
    const CFTypeRef value = IORegistryEntryCreateCFProperty(entry, key, kCFAllocatorDefault, 0);

    if (!value)
    {
        return QString{};
    }

    QString result;

    if (CFGetTypeID(value) == CFStringGetTypeID())
    {
        result = toQString(static_cast<CFStringRef>(value));
    }

    CFRelease(value);

    return result;
}

quint64 copyNumberProperty(io_registry_entry_t entry, CFStringRef key, quint64 fallbackValue)
{
    const CFTypeRef value = IORegistryEntryCreateCFProperty(entry, key, kCFAllocatorDefault, 0);

    if (!value)
    {
        return fallbackValue;
    }

    quint64 result = fallbackValue;

    if (CFGetTypeID(value) == CFNumberGetTypeID())
    {
        long long number = 0;

        if (CFNumberGetValue(static_cast<CFNumberRef>(value), kCFNumberLongLongType, &number) && number >= 0)
        {
            result = static_cast<quint64>(number);
        }
    }

    CFRelease(value);

    return result;
}

bool copyBoolProperty(io_registry_entry_t entry, CFStringRef key, bool fallbackValue)
{
    const CFTypeRef value = IORegistryEntryCreateCFProperty(entry, key, kCFAllocatorDefault, 0);

    if (!value)
    {
        return fallbackValue;
    }

    bool result = fallbackValue;

    if (CFGetTypeID(value) == CFBooleanGetTypeID())
    {
        result = CFBooleanGetValue(static_cast<CFBooleanRef>(value));
    }

    CFRelease(value);

    return result;
}

// Vendor, model and interconnect live in dictionaries published by the hardware a few levels up the
// service plane, not on the media object itself.
QString copyInheritedDictionaryString(io_registry_entry_t entry, CFStringRef dictionaryKey, CFStringRef valueKey)
{
    const CFTypeRef dictionary = IORegistryEntrySearchCFProperty(entry,
        kIOServicePlane,
        dictionaryKey,
        kCFAllocatorDefault,
        kIORegistryIterateParents | kIORegistryIterateRecursively);

    if (!dictionary)
    {
        return QString{};
    }

    QString result;

    if (CFGetTypeID(dictionary) == CFDictionaryGetTypeID())
    {
        const void* value = nullptr;

        if (CFDictionaryGetValueIfPresent(static_cast<CFDictionaryRef>(dictionary), valueKey, &value)
            && value && CFGetTypeID(value) == CFStringGetTypeID())
        {
            result = toQString(static_cast<CFStringRef>(value));
        }
    }

    CFRelease(dictionary);

    return result;
}

// A synthesized APFS volume is also whole media, but it has no block-storage driver behind it and is not
// something that can be imaged.
bool hasBlockStorageDriver(io_registry_entry_t entry)
{
    ScopedIoObject current;

    {
        IOObjectRetain(entry);
        current.reset(entry);
    }

    for (int depth = 0; depth < MaximumParentSearchDepth; ++depth)
    {
        if (IOObjectConformsTo(current.get(), "IOBlockStorageDriver"))
        {
            return true;
        }

        io_registry_entry_t parent = IO_OBJECT_NULL;

        if (IORegistryEntryGetParentEntry(current.get(), kIOServicePlane, &parent) != KERN_SUCCESS)
        {
            break;
        }

        current.reset(parent);
    }

    return false;
}

BusType mapPhysicalInterconnect(const QString& interconnect)
{
    if (interconnect.compare("USB"_L1, Qt::CaseInsensitive) == 0)
    {
        return BusType::Usb;
    }

    if (interconnect.contains("Secure Digital"_L1, Qt::CaseInsensitive))
    {
        return BusType::Sd;
    }

    if (interconnect.contains("PCI"_L1, Qt::CaseInsensitive) || interconnect.contains("Apple Fabric"_L1, Qt::CaseInsensitive))
    {
        return BusType::Nvme;
    }

    if (interconnect.contains("SATA"_L1, Qt::CaseInsensitive) || interconnect.contains("ATA"_L1, Qt::CaseInsensitive))
    {
        return BusType::Sata;
    }

    if (interconnect.contains("Virtual"_L1, Qt::CaseInsensitive) || interconnect.contains("Disk Image"_L1, Qt::CaseInsensitive))
    {
        return BusType::Virtual;
    }

    return BusType::Unknown;
}

struct MountEntry
{
    QString devicePath;
    QString mountPoint;
};

std::vector<MountEntry> readMountTable()
{
    std::vector<MountEntry> entries;

    struct statfs* mounts = nullptr;
    const int mountCount = getmntinfo(&mounts, MNT_NOWAIT);

    for (int index = 0; index < mountCount; ++index)
    {
        MountEntry entry;
        entry.devicePath = QString::fromLocal8Bit(mounts[index].f_mntfromname);
        entry.mountPoint = QString::fromLocal8Bit(mounts[index].f_mntonname);

        if (entry.devicePath.startsWith(BlockDevicePrefix))
        {
            entries.push_back(std::move(entry));
        }
    }

    return entries;
}

// /dev/disk3s2 belongs to disk3, but /dev/disk30 does not.
bool mountBelongsToDevice(const QString& mountDevicePath, const QString& blockDevicePath)
{
    if (!mountDevicePath.startsWith(blockDevicePath))
    {
        return false;
    }

    if (mountDevicePath.size() == blockDevicePath.size())
    {
        return true;
    }

    return mountDevicePath.at(blockDevicePath.size()) == u's';
}
} // namespace

DeviceEnumerator::Result DeviceEnumerator::enumerate(bool includeFixedDisks)
{
    Result result;

    CFMutableDictionaryRef matching = IOServiceMatching(kIOMediaClass);

    if (!matching)
    {
        result.warnings.push_back(QCoreApplication::translate("DeviceEnumerator",
            "The IOKit registry could not be queried, so no devices could be found."));

        return result;
    }

    CFDictionarySetValue(matching, CFSTR(kIOMediaWholeKey), kCFBooleanTrue);

    io_iterator_t mediaIterator = IO_OBJECT_NULL;

    if (IOServiceGetMatchingServices(MACH_PORT_NULL, matching, &mediaIterator) != KERN_SUCCESS)
    {
        result.warnings.push_back(QCoreApplication::translate("DeviceEnumerator",
            "The IOKit registry could not be queried, so no devices could be found."));

        return result;
    }

    const ScopedIoObject iterator{ mediaIterator };
    const std::vector<MountEntry> mountTable = readMountTable();

    while (true)
    {
        const ScopedIoObject media{ IOIteratorNext(iterator.get()) };

        if (!media.isValid())
        {
            break;
        }

        const QString bsdName = copyStringProperty(media.get(), CFSTR(kIOBSDNameKey));

        if (bsdName.isEmpty() || !hasBlockStorageDriver(media.get()))
        {
            continue;
        }

        DeviceInfo device;
        device.shortIdentifier = bsdName.toUtf8();
        // The raw character device skips the buffer cache, which roughly doubles throughput compared with
        // /dev/diskN.
        device.path = (RawDevicePrefix + bsdName).toUtf8();
        device.sizeBytes = copyNumberProperty(media.get(), CFSTR(kIOMediaSizeKey), 0);

        if (device.sizeBytes == 0)
        {
            continue;
        }

        device.logicalSectorSizeBytes = static_cast<quint32>(
            copyNumberProperty(media.get(), CFSTR(kIOMediaPreferredBlockSizeKey), 512));
        device.writeProtected = !copyBoolProperty(media.get(), CFSTR(kIOMediaWritableKey), true);

        device.vendor = copyInheritedDictionaryString(media.get(),
            CFSTR("Device Characteristics"), CFSTR("Vendor Name"));
        device.model = copyInheritedDictionaryString(media.get(),
            CFSTR("Device Characteristics"), CFSTR("Product Name"));
        device.serialNumber = copyInheritedDictionaryString(media.get(),
            CFSTR("Device Characteristics"), CFSTR("Serial Number")).toLatin1();

        const QString interconnect = copyInheritedDictionaryString(media.get(),
            CFSTR("Protocol Characteristics"), CFSTR("Physical Interconnect"));
        const QString interconnectLocation = copyInheritedDictionaryString(media.get(),
            CFSTR("Protocol Characteristics"), CFSTR("Physical Interconnect Location"));

        device.bus = mapPhysicalInterconnect(interconnect);

        const bool externalDevice = interconnectLocation.compare("External"_L1, Qt::CaseInsensitive) == 0;
        device.removableMedia = copyBoolProperty(media.get(), CFSTR(kIOMediaRemovableKey), false)
            || copyBoolProperty(media.get(), CFSTR(kIOMediaEjectableKey), false)
            || externalDevice;

        const QString blockDevicePath = BlockDevicePrefix + bsdName;

        for (const MountEntry& mountEntry : mountTable)
        {
            if (!mountBelongsToDevice(mountEntry.devicePath, blockDevicePath))
            {
                continue;
            }

            device.mountPoints.push_back(mountEntry.mountPoint);
            device.volumeLabels.push_back(QString{});

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

    std::sort(result.devices.begin(), result.devices.end(), [](const DeviceInfo& first, const DeviceInfo& second)
    {
        return first.shortIdentifier < second.shortIdentifier;
    });

    return result;
}
} // namespace UDI
