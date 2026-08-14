#include <disk/volumecontrol.hpp>

#include <QCoreApplication>
#include <QProcess>


namespace UDI
{
using namespace Qt::Literals::StringLiterals;

namespace
{

const QLatin1StringView DiskUtilExecutable{ "diskutil" };

constexpr int DiskUtilTimeoutMilliseconds = 30000;

// diskutil takes the whole disk down in one step and tells the rest of the system about it, which is
// what stops Finder from remounting a partition halfway through a write.
QString blockDevicePath(const DeviceInfo& device)
{
    return "/dev/"_L1 + QString::fromUtf8(device.shortIdentifier);
}

bool runDiskUtil(const QStringList& arguments, QString& output)
{
    QProcess diskUtil;
    diskUtil.setProcessChannelMode(QProcess::MergedChannels);
    diskUtil.start(DiskUtilExecutable, arguments);

    if (!diskUtil.waitForFinished(DiskUtilTimeoutMilliseconds))
    {
        diskUtil.kill();
        output = QCoreApplication::translate("VolumeControl", "diskutil did not finish in time.");
        return false;
    }

    output = QString::fromLocal8Bit(diskUtil.readAll()).trimmed();

    return diskUtil.exitStatus() == QProcess::NormalExit && diskUtil.exitCode() == 0;
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
    if (m_device.mountPoints.empty())
    {
        return true;
    }

    QString output;

    if (!runDiskUtil(QStringList{ QStringLiteral("unmountDisk"), blockDevicePath(m_device) }, output))
    {
        errorMessage = QCoreApplication::translate("VolumeControl",
            "Could not unmount %1: %2. Close any program using it and try again.")
            .arg(blockDevicePath(m_device), output);

        return false;
    }

    qInfo() << "Unmounted every volume of" << m_device.path;

    return true;
}

void VolumeControl::release()
{
    // Nothing is held open: the unmount above is the whole lock.
}

void VolumeControl::refreshLayout()
{
    QString output;

    if (!runDiskUtil(QStringList{ QStringLiteral("mountDisk"), blockDevicePath(m_device) }, output))
    {
        // A freshly written card often carries a filesystem macOS cannot mount, which is not an error
        // here — the write itself already succeeded.
        qDebug() << "diskutil did not remount" << blockDevicePath(m_device) << ':' << output;
    }
}
} // namespace UDI
