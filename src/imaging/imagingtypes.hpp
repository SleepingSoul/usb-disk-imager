#pragma once

#include <QByteArray>
#include <QLatin1StringView>
#include <QMetaType>
#include <QString>

#include <disk/disktypes.hpp>


namespace UDI
{
enum class ImagingOperation
{
    None,
    Read,
    Write,
    Verify
};

// How much of the source device a read stores.
enum class TrimMode
{
    None,          // byte-for-byte copy of the whole device
    Partitions,    // stop after the last partition, rebuilding GPT metadata for the shorter image
    TrailingZeros  // stop after the last sector that is not entirely zero
};

enum class ImagingStage
{
    Idle,
    Preparing,
    Unmounting,
    Analyzing,
    Transferring,
    Verifying,
    Finalizing,
    Complete,
    Failed,
    Cancelled
};

struct ImagingRequest
{
    ImagingOperation operation{ ImagingOperation::None };
    DeviceInfo device;
    QString imageFilePath;
    TrimMode trimMode{ TrimMode::None };
    bool verifyAfterWrite{ false };
    bool computeSha256{ false };
};

struct ImagingProgress
{
    ImagingStage stage{ ImagingStage::Idle };
    quint64 processedBytes{ 0 };
    quint64 totalBytes{ 0 };
    double bytesPerSecond{ 0.0 };
    qint64 elapsedMilliseconds{ 0 };
    // Negative until enough has been transferred for the estimate to mean anything.
    qint64 remainingMilliseconds{ -1 };
};

struct ImagingResult
{
    ImagingOperation operation{ ImagingOperation::None };
    ImagingStage finalStage{ ImagingStage::Idle };
    bool succeeded{ false };
    bool cancelled{ false };
    bool verificationMatched{ false };
    QString errorMessage;
    QString imageFilePath;
    quint64 processedBytes{ 0 };
    qint64 elapsedMilliseconds{ 0 };
    QByteArray fastDigestHex;
    QByteArray sha256Hex;
};

// QML picks a trim mode by token rather than by number, so the choice reads the same in the QML radio
// buttons, in the settings file and in the log.
QLatin1StringView trimModeToken(TrimMode mode);
TrimMode trimModeFromToken(const QByteArray& token);
} // namespace UDI

Q_DECLARE_METATYPE(UDI::ImagingRequest)
Q_DECLARE_METATYPE(UDI::ImagingProgress)
Q_DECLARE_METATYPE(UDI::ImagingResult)
Q_DECLARE_METATYPE(UDI::ImagingStage)
