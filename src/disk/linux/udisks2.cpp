#include <disk/linux/udisks2.hpp>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QFileInfo>
#include <QVariantMap>

#include <fcntl.h>
#include <unistd.h>


namespace UDI
{
namespace UDisks2
{
namespace
{
using namespace Qt::Literals::StringLiterals;

const QLatin1StringView ServiceName{ "org.freedesktop.UDisks2" };
const QLatin1StringView BlockDevicesPath{ "/org/freedesktop/UDisks2/block_devices/" };
const QLatin1StringView BlockInterface{ "org.freedesktop.UDisks2.Block" };
const QLatin1StringView FilesystemInterface{ "org.freedesktop.UDisks2.Filesystem" };

// The call blocks while polkit's prompt is on screen, and a person typing a password takes far longer
// than the default 25 seconds D-Bus allows a reply.
constexpr int AuthenticationTimeoutMilliseconds = 10 * 60 * 1000;

// udisks2 names a block object after the kernel device, so /dev/sdb is .../block_devices/sdb.
QString blockObjectPath(const QByteArray& devicePath)
{
    return QString(BlockDevicesPath) + QFileInfo{ QString::fromUtf8(devicePath) }.fileName();
}

QString describeError(const QDBusError& error)
{
    // "…CanObtain" is udisks2 saying the user could authorise this but no prompt was answered, which is
    // a different thing to tell someone than a flat refusal.
    if (error.name().endsWith("NotAuthorizedCanObtain"_L1))
    {
        return QCoreApplication::translate("UDisks2", "Authorisation was not granted.");
    }

    if (error.name().endsWith("NotAuthorized"_L1) || error.name().endsWith("NotAuthorizedDismissed"_L1))
    {
        return QCoreApplication::translate("UDisks2", "This session is not allowed to open the device.");
    }

    return error.message();
}
} // namespace

bool isAvailable()
{
    const QDBusConnection systemBus = QDBusConnection::systemBus();

    if (!systemBus.isConnected())
    {
        return false;
    }

    const QDBusReply<bool> reply = systemBus.interface()->isServiceRegistered(QString(ServiceName));

    return reply.isValid() && reply.value();
}

int openDevice(const QByteArray& devicePath, bool forWriting, int openFlags, QString& errorMessage)
{
    QDBusInterface block{ QString(ServiceName), blockObjectPath(devicePath), QString(BlockInterface),
        QDBusConnection::systemBus() };

    if (!block.isValid())
    {
        errorMessage = QCoreApplication::translate("UDisks2", "udisks2 does not know about %1.")
            .arg(QString::fromUtf8(devicePath));
        return -1;
    }

    block.setTimeout(AuthenticationTimeoutMilliseconds);

    QVariantMap options;
    options.insert(QStringLiteral("flags"), QVariant::fromValue<int>(openFlags));

    const QDBusReply<QDBusUnixFileDescriptor> reply =
        block.call(QStringLiteral("OpenDevice"), forWriting ? QStringLiteral("rw") : QStringLiteral("r"), options);

    if (!reply.isValid())
    {
        errorMessage = describeError(reply.error());
        return -1;
    }

    // The descriptor belongs to the reply, which is about to go out of scope.
    const int fileDescriptor = ::dup(reply.value().fileDescriptor());

    if (fileDescriptor < 0)
    {
        errorMessage = QCoreApplication::translate("UDisks2",
            "The descriptor udisks2 returned for %1 could not be kept.").arg(QString::fromUtf8(devicePath));
        return -1;
    }

    return fileDescriptor;
}

bool unmountFilesystems(const DeviceInfo& device, QString& errorMessage)
{
    if (device.mountPoints.empty())
    {
        return true;
    }

    // A partition is its own udisks2 object, and the whole-disk object carries no filesystem to unmount,
    // so each mount is taken down through the partition it actually belongs to.
    for (const QString& partitionPath : device.mountedDevicePaths)
    {
        QDBusInterface filesystem{ QString(ServiceName), blockObjectPath(partitionPath.toUtf8()),
            QString(FilesystemInterface), QDBusConnection::systemBus() };

        if (!filesystem.isValid())
        {
            continue;
        }

        filesystem.setTimeout(AuthenticationTimeoutMilliseconds);

        const QDBusReply<void> reply = filesystem.call(QStringLiteral("Unmount"), QVariantMap{});

        // A partition that was not mounted answers with NotMounted, which is the state being asked for.
        if (!reply.isValid() && !reply.error().name().endsWith("NotMounted"_L1))
        {
            errorMessage = QCoreApplication::translate("UDisks2", "Could not unmount %1: %2")
                .arg(partitionPath, describeError(reply.error()));
            return false;
        }

        qInfo() << "Unmounted" << partitionPath << "through udisks2";
    }

    return true;
}
} // namespace UDisks2
} // namespace UDI
