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
    ShrinkingFilesystem,
    GrowingFilesystem,
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
    // A run of trailing zero bytes in the image is not written at all, leaving whatever was already on
    // the device there. Safe only because that tail lies outside every partition the image describes.
    bool skipTrailingZerosOnWrite{ false };
    // Shrinks the ext2/3/4 filesystem in the last partition to the smallest e2fsprogs will allow, and
    // truncates the image to match, once the read itself has finished.
    bool shrinkFilesystemAfterRead{ false };
    // Grows the ext2/3/4 filesystem in the last partition of the image to fill the destination device,
    // before the write itself starts.
    bool growFilesystemToFillDevice{ false };
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
