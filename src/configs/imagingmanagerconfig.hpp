#pragma once

#include <app/managerconfig.hpp>


namespace UDI
{
using namespace Qt::Literals::StringLiterals;

class ImagingManagerConfig
{
public:
    // Big enough that the per-transfer overhead disappears against the medium's own throughput, small
    // enough that a cancel takes effect immediately.
    quint64 chunkSizeBytes;
    quint64 partitionScanSizeBytes;
    quint64 trimAlignmentBytes;
    int progressIntervalMilliseconds;

    bool parse(const ManagerConfigMap& config)
    {
        bool result = true;

        result = config["chunk_size_b"_L1].get(chunkSizeBytes) && result;
        result = config["partition_scan_size_b"_L1].get(partitionScanSizeBytes) && result;
        result = config["trim_alignment_b"_L1].get(trimAlignmentBytes) && result;
        result = config["progress_interval_ms"_L1].get(progressIntervalMilliseconds) && result;

        return result;
    }

    QString getFileName() const
    {
        return QStringLiteral("imagingmanagerconfig.json");
    }
};
} // namespace UDI
