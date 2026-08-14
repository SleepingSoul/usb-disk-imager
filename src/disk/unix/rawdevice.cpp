#include <disk/rawdevice.hpp>

#include <algorithm>
#include <cerrno>
#include <cstring>

#include <QCoreApplication>
#include <QtGlobal>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#if defined(Q_OS_LINUX)
#include <linux/fs.h>
#elif defined(Q_OS_MACOS)
#include <sys/disk.h>
#endif


namespace UDI
{
namespace
{

// Keeps a single stalled transfer short enough for cancellation to stay responsive.
constexpr qint64 MaximumSingleTransferBytes = 32ll * 1024ll * 1024ll;

QString describeErrno(int errorNumber)
{
    return QString::fromLocal8Bit(std::strerror(errorNumber));
}

class UnixRawDevice : public RawDevice
{
    Q_DISABLE_COPY_MOVE(UnixRawDevice)
public:
    UnixRawDevice(int fileDescriptor, quint64 sizeBytes, quint32 sectorSizeBytes)
        : m_fileDescriptor(fileDescriptor)
        , m_sizeBytes(sizeBytes)
        , m_sectorSizeBytes(sectorSizeBytes)
    {}

    ~UnixRawDevice() override
    {
        if (m_fileDescriptor >= 0)
        {
            ::close(m_fileDescriptor);
        }
    }

    qint64 read(std::uint8_t* data, qint64 sizeBytes) override
    {
        qint64 transferred = 0;

        while (transferred < sizeBytes)
        {
            const auto requested =
                static_cast<std::size_t>(std::min(sizeBytes - transferred, MaximumSingleTransferBytes));
            const ssize_t received = ::read(m_fileDescriptor, data + transferred, requested);

            if (received < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }

                setLastError(describeErrno(errno));
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
            const auto requested =
                static_cast<std::size_t>(std::min(sizeBytes - transferred, MaximumSingleTransferBytes));
            const ssize_t written = ::write(m_fileDescriptor, data + transferred, requested);

            if (written < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }

                setLastError(describeErrno(errno));
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
        if (::lseek(m_fileDescriptor, static_cast<off_t>(offsetBytes), SEEK_SET) < 0)
        {
            setLastError(describeErrno(errno));
            return false;
        }

        return true;
    }

    bool sync() override
    {
#if defined(Q_OS_MACOS)
        // fsync() only hands the data to the drive on macOS; F_FULLFSYNC also makes the drive commit it.
        if (::fcntl(m_fileDescriptor, F_FULLFSYNC) == 0)
        {
            return true;
        }
#endif

        if (::fsync(m_fileDescriptor) != 0)
        {
            setLastError(describeErrno(errno));
            return false;
        }

        return true;
    }

    quint64 getSizeBytes() const override { return m_sizeBytes; }
    quint32 getSectorSizeBytes() const override { return m_sectorSizeBytes; }

private:
    int m_fileDescriptor{ -1 };
    quint64 m_sizeBytes{ 0 };
    quint32 m_sectorSizeBytes{ 512 };
};

quint64 queryDeviceSize(int fileDescriptor, quint64 fallbackSizeBytes)
{
#if defined(Q_OS_LINUX)
    quint64 sizeBytes = 0;

    if (::ioctl(fileDescriptor, BLKGETSIZE64, &sizeBytes) == 0 && sizeBytes > 0)
    {
        return sizeBytes;
    }
#elif defined(Q_OS_MACOS)
    quint64 blockCount = 0;
    quint32 blockSizeBytes = 0;

    if (::ioctl(fileDescriptor, DKIOCGETBLOCKCOUNT, &blockCount) == 0
        && ::ioctl(fileDescriptor, DKIOCGETBLOCKSIZE, &blockSizeBytes) == 0
        && blockCount > 0 && blockSizeBytes > 0)
    {
        return blockCount * blockSizeBytes;
    }
#endif

    const off_t endOffset = ::lseek(fileDescriptor, 0, SEEK_END);
    ::lseek(fileDescriptor, 0, SEEK_SET);

    return endOffset > 0 ? static_cast<quint64>(endOffset) : fallbackSizeBytes;
}

quint32 querySectorSize(int fileDescriptor, quint32 fallbackSectorSizeBytes)
{
#if defined(Q_OS_LINUX)
    int sectorSizeBytes = 0;

    if (::ioctl(fileDescriptor, BLKSSZGET, &sectorSizeBytes) == 0 && sectorSizeBytes > 0)
    {
        return static_cast<quint32>(sectorSizeBytes);
    }
#elif defined(Q_OS_MACOS)
    quint32 blockSizeBytes = 0;

    if (::ioctl(fileDescriptor, DKIOCGETBLOCKSIZE, &blockSizeBytes) == 0 && blockSizeBytes > 0)
    {
        return blockSizeBytes;
    }
#else
    Q_UNUSED(fileDescriptor)
#endif

    return fallbackSectorSizeBytes;
}
} // namespace

std::unique_ptr<RawDevice> RawDevice::open(const DeviceInfo& device, AccessMode mode, QString& errorMessage)
{
    int flags = (mode == AccessMode::Read ? O_RDONLY : O_RDWR) | O_CLOEXEC;

#if defined(Q_OS_LINUX)
    // The kernel refuses an exclusive open of a block device while any filesystem on it is mounted, so
    // this is the guard that holds even if unmounting silently failed.
    if (mode == AccessMode::Write)
    {
        flags |= O_EXCL;
    }
#endif

    const int fileDescriptor = ::open(device.path.constData(), flags);

    if (fileDescriptor < 0)
    {
        const int errorNumber = errno;

        if (errorNumber == EACCES || errorNumber == EPERM)
        {
            errorMessage = QCoreApplication::translate("RawDevice",
                "Access to %1 was denied. Raw disk access needs root rights.")
                .arg(QString::fromUtf8(device.path));
        }
        else if (errorNumber == EBUSY)
        {
            errorMessage = QCoreApplication::translate("RawDevice",
                "%1 is in use — a filesystem on it is still mounted.")
                .arg(QString::fromUtf8(device.path));
        }
        else
        {
            errorMessage = QCoreApplication::translate("RawDevice", "Could not open %1: %2")
                .arg(QString::fromUtf8(device.path), describeErrno(errorNumber));
        }

        return nullptr;
    }

    const quint64 sizeBytes = queryDeviceSize(fileDescriptor, device.sizeBytes);
    const quint32 sectorSizeBytes = querySectorSize(fileDescriptor, device.logicalSectorSizeBytes);

    return std::make_unique<UnixRawDevice>(fileDescriptor, sizeBytes, sectorSizeBytes);
}
} // namespace UDI
