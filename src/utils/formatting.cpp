#include <utils/formatting.hpp>

#include <array>
#include <cmath>

#include <QCoreApplication>


namespace UDI
{
namespace
{

constexpr double BytesPerUnit = 1000.0;

QString sizeUnitName(std::size_t unitIndex)
{
    switch (unitIndex)
    {
    case 0:  return QCoreApplication::translate("Formatting", "B");
    case 1:  return QCoreApplication::translate("Formatting", "kB");
    case 2:  return QCoreApplication::translate("Formatting", "MB");
    case 3:  return QCoreApplication::translate("Formatting", "GB");
    default: return QCoreApplication::translate("Formatting", "TB");
    }
}
} // namespace

QString formatByteSize(quint64 bytes)
{
    double value = static_cast<double>(bytes);
    std::size_t unitIndex = 0;

    while (value >= BytesPerUnit && unitIndex < 4)
    {
        value /= BytesPerUnit;
        ++unitIndex;
    }

    const int decimals = (unitIndex == 0 || value >= 100.0) ? 0 : (value >= 10.0 ? 1 : 2);

    return QCoreApplication::translate("Formatting", "%1 %2")
        .arg(QString::number(value, 'f', decimals), sizeUnitName(unitIndex));
}

QString formatTransferRate(double bytesPerSecond)
{
    if (!std::isfinite(bytesPerSecond) || bytesPerSecond <= 0.0)
    {
        return QCoreApplication::translate("Formatting", "—");
    }

    double value = bytesPerSecond;
    std::size_t unitIndex = 0;

    while (value >= BytesPerUnit && unitIndex < 4)
    {
        value /= BytesPerUnit;
        ++unitIndex;
    }

    return QCoreApplication::translate("Formatting", "%1 %2/s")
        .arg(QString::number(value, 'f', value >= 100.0 ? 0 : 1), sizeUnitName(unitIndex));
}

QString formatDuration(qint64 milliseconds)
{
    if (milliseconds < 0)
    {
        return QCoreApplication::translate("Formatting", "—");
    }

    const qint64 totalSeconds = milliseconds / 1000;
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;

    if (hours > 0)
    {
        return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'));
    }

    return QStringLiteral("%1:%2").arg(minutes).arg(seconds, 2, 10, QLatin1Char('0'));
}
} // namespace UDI
