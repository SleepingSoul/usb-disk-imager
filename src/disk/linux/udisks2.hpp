#pragma once

#include <QByteArray>
#include <QCoreApplication>
#include <QString>

#include <disk/disktypes.hpp>


namespace UDI
{
/*
 * The udisks2 system service, which hands an unprivileged process an already-open file descriptor for a
 * block device once polkit has authorised it.
 *
 * This is what lets the app image a card without being root: a desktop user answers a polkit prompt
 * instead of restarting the whole GUI under sudo. It is the same route Raspberry Pi Imager takes, and the
 * only one available wherever /dev/sdX cannot be opened directly and pkexec is unavailable.
 */
namespace UDisks2
{
// True when the service is on the system bus at all. Says nothing about authorisation, which is decided
// per call and may prompt the user.
bool isAvailable();

// Returns an owned file descriptor, or -1 with \a errorMessage filled in. \a openFlags is OR-ed into the
// open() udisks2 performs, which is how O_EXCL still guards a write against a mounted filesystem.
//
// Blocks for as long as the polkit prompt is on screen, so this must not run on the GUI thread.
int openDevice(const QByteArray& devicePath, bool forWriting, int openFlags, QString& errorMessage);

// Unmounts every mounted filesystem of \a device through udisks2, which unmounts as the owning desktop
// session rather than as root and keeps that session's bookkeeping intact.
bool unmountFilesystems(const DeviceInfo& device, QString& errorMessage);
} // namespace UDisks2
} // namespace UDI
