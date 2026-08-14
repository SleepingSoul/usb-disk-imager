#include <disk/volumecontrol.hpp>

#include <cerrno>
#include <cstring>

#include <QCoreApplication>
#include <QFile>
#include <QProcess>

#include <fcntl.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <unistd.h>


namespace UDI
{
using namespace Qt::Literals::StringLiterals;

namespace
{

constexpr int UmountHelperTimeoutMilliseconds = 15000;

// A filesystem mounted through udisks belongs to the desktop session rather than to root, and only the
// umount helper knows how to take it down with the session's bookkeeping intact.
bool runUmountHelper(const QString& mountPoint)
{
    QProcess umount;
    umount.start(QStringLiteral("umount"), QStringList{ mountPoint });

    if (!umount.waitForFinished(UmountHelperTimeoutMilliseconds))
    {
        umount.kill();
        return false;
    }

    return umount.exitStatus() == QProcess::NormalExit && umount.exitCode() == 0;
}
} // namespace

VolumeControl::VolumeControl(DeviceInfo device)
    : m_device(std::move(device))
{}

VolumeControl::~VolumeControl()
{
    release();
    refreshLayout();
}

bool VolumeControl::acquire(QString& errorMessage)
{
    for (const QString& mountPoint : m_device.mountPoints)
    {
        const QByteArray nativeMountPoint = QFile::encodeName(mountPoint);

        if (::umount2(nativeMountPoint.constData(), 0) == 0)
        {
            qInfo() << "Unmounted" << mountPoint << "of" << m_device.path;
            continue;
        }

        const int umountErrorNumber = errno;

        if (runUmountHelper(mountPoint))
        {
            qInfo() << "Unmounted" << mountPoint << "of" << m_device.path << "through the umount helper";
            continue;
        }

        errorMessage = QCoreApplication::translate("VolumeControl",
            "Could not unmount %1: %2. Close any program using it and try again.")
            .arg(mountPoint, QString::fromLocal8Bit(std::strerror(umountErrorNumber)));

        return false;
    }

    return true;
}

void VolumeControl::release()
{
    // Nothing is held open: the exclusive open of the device is what keeps other writers out on Linux.
}

void VolumeControl::refreshLayout()
{
    const int fileDescriptor = ::open(m_device.path.constData(), O_RDONLY | O_CLOEXEC);

    if (fileDescriptor < 0)
    {
        qWarning() << "Could not reopen" << m_device.path << "to re-read its partition table:"
                   << QString::fromLocal8Bit(std::strerror(errno));
        return;
    }

    if (::ioctl(fileDescriptor, BLKRRPART) != 0)
    {
        // Expected when nothing was written or when the kernel considers the device busy; the partitions
        // reappear on the next hotplug event either way.
        qDebug() << "The kernel declined to re-read the partition table of" << m_device.path << ':'
                 << QString::fromLocal8Bit(std::strerror(errno));
    }

    ::close(fileDescriptor);
}
} // namespace UDI
