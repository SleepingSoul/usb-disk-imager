#include <disk/filesystemresizer.hpp>


namespace UDI
{
// Neither losetup's zero-copy loop devices nor macOS's hdiutil have a Windows equivalent for exposing a
// byte range of a file as its own device node, so there is nothing here yet to expose the tools through.
// See docs/filesystem-shrink-grow.md for the two paths considered (WSL2, and a copy-out/splice-back
// fallback) and why neither is implemented yet.
bool FilesystemResizer::isSupported()
{
    return false;
}

QString FilesystemResizer::getUnsupportedReason()
{
    return tr("Not available on Windows yet.");
}

std::optional<FilesystemResizer::ShrinkResult> FilesystemResizer::shrinkFilesystem(const QString& path,
    quint64 partitionStartSector,
    quint64 partitionSectorCount,
    quint32 sectorSizeBytes,
    QString& errorMessage)
{
    Q_UNUSED(path)
    Q_UNUSED(partitionStartSector)
    Q_UNUSED(partitionSectorCount)
    Q_UNUSED(sectorSizeBytes)

    errorMessage = getUnsupportedReason();
    return std::nullopt;
}

bool FilesystemResizer::growFilesystem(const QString& path,
    quint64 partitionStartSector,
    quint64 partitionSectorCount,
    quint32 sectorSizeBytes,
    QString& errorMessage)
{
    Q_UNUSED(path)
    Q_UNUSED(partitionStartSector)
    Q_UNUSED(partitionSectorCount)
    Q_UNUSED(sectorSizeBytes)

    errorMessage = getUnsupportedReason();
    return false;
}
} // namespace UDI
