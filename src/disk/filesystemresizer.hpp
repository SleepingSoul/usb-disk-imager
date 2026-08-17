#pragma once

#include <optional>

#include <QCoreApplication>
#include <QString>
#include <QtTypes>


namespace UDI
{
// Shrinks or grows the ext2/3/4 filesystem inside one partition of a disk image file or raw device, via
// the system's e2fsprogs (e2fsck, resize2fs). Every call here operates on bytes already on disk — never
// on a live mounted filesystem — so the caller is free to run this on its own worker thread.
//
// A resize is not cancellable once started: killing e2fsck or resize2fs mid-run can leave the filesystem
// half-migrated, which is a far worse outcome than waiting for it to finish. Callers should disable
// cancellation for the duration of a call here rather than thread it through.
class FilesystemResizer
{
    Q_DECLARE_TR_FUNCTIONS(FilesystemResizer)
public:
    struct ShrinkResult
    {
        // Sector count the shrunk filesystem now occupies. The caller repositions the partition table
        // entry and truncates the image to match.
        quint64 newPartitionSectorCount{ 0 };
    };

    // True when e2fsck and resize2fs are available right now. The Read/Write pages use this to grey the
    // corresponding option out instead of hiding it.
    static bool isSupported();

    // Explains why isSupported() is false, suitable for display next to the greyed-out option. Empty
    // when isSupported() is true.
    static QString getUnsupportedReason();

    // Shrinks the ext2/3/4 filesystem occupying sectors
    // [partitionStartSector, partitionStartSector + partitionSectorCount) of \a path to the smallest
    // size e2fsprogs will allow. \a path may be a regular file or a raw device special file — either way
    // it must not be open elsewhere for writing, since this attaches it as a loop device.
    static std::optional<ShrinkResult> shrinkFilesystem(const QString& path,
        quint64 partitionStartSector,
        quint64 partitionSectorCount,
        quint32 sectorSizeBytes,
        QString& errorMessage);

    // Grows the ext2/3/4 filesystem occupying sectors starting at \a partitionStartSector to fill the
    // full \a partitionSectorCount sectors now reserved for it.
    static bool growFilesystem(const QString& path,
        quint64 partitionStartSector,
        quint64 partitionSectorCount,
        quint32 sectorSizeBytes,
        QString& errorMessage);
};
} // namespace UDI
