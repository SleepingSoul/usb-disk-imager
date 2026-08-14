#pragma once

#include <QString>


namespace UDI
{
// Decimal units, matching the capacity printed on the drive rather than the binary units a file manager
// would show, so a "32 GB" card does not read as 29.8 of something.
QString formatByteSize(quint64 bytes);

QString formatTransferRate(double bytesPerSecond);

// h:mm:ss, or m:ss below an hour.
QString formatDuration(qint64 milliseconds);
} // namespace UDI
