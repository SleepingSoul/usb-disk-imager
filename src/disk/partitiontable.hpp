#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <QString>
#include <QtTypes>


namespace UDI
{
enum class PartitionScheme
{
    None,
    Mbr,
    Gpt
};

struct PartitionEntry
{
    quint64 firstSector{ 0 };
    quint64 sectorCount{ 0 };
    QString typeName;
};

// GPT keeps a second copy of its header and partition-entry array at the very end of the medium, which
// is exactly what truncating an image cuts off. Rewriting both headers to describe the shorter image
// keeps a trimmed GPT image self-consistent, so it can be written back or mounted like any other.
struct GptTrimTrailer
{
    std::vector<std::uint8_t> primaryHeaderSector;
    std::vector<std::uint8_t> trailerSectors;
    quint64 primaryHeaderSectorIndex{ 1 };
};

struct TrimPlan
{
    // Bytes copied verbatim from the start of the device.
    quint64 dataSizeBytes{ 0 };
    // Size of the resulting image, larger than dataSizeBytes when a GPT trailer is appended.
    quint64 imageSizeBytes{ 0 };
    std::optional<GptTrimTrailer> gptTrailer;
};

class PartitionTable
{
public:
    // Reads whatever of MBR and GPT fits in \a head, which must start at sector 0 of the device and be
    // long enough to cover the GPT entry array (one megabyte is plenty).
    static PartitionTable parse(const std::vector<std::uint8_t>& head,
        quint32 sectorSizeBytes,
        quint64 deviceSizeBytes);

    PartitionScheme getScheme() const { return m_scheme; }
    const std::vector<PartitionEntry>& getPartitions() const { return m_partitions; }

    // Inclusive index of the last sector occupied by any partition, or 0 when there is nothing to trim.
    quint64 getLastUsedSector() const { return m_lastUsedSector; }

    // std::nullopt when the device has no usable table, when the partitions reach the end of the medium
    // anyway, or when the GPT metadata is laid out too unusually to be rebuilt safely.
    std::optional<TrimPlan> planTrim(quint64 alignmentBytes) const;

private:
    bool parseGpt(const std::vector<std::uint8_t>& head);
    bool parseMbr(const std::vector<std::uint8_t>& head);
    std::optional<TrimPlan> planGptTrim() const;

    PartitionScheme m_scheme{ PartitionScheme::None };
    std::vector<PartitionEntry> m_partitions;
    quint64 m_lastUsedSector{ 0 };

    quint32 m_sectorSizeBytes{ 512 };
    quint64 m_deviceSizeBytes{ 0 };

    std::vector<std::uint8_t> m_gptPrimaryHeader;
    std::vector<std::uint8_t> m_gptEntryArray;
    quint64 m_gptEntryArrayFirstSector{ 0 };
    quint32 m_gptEntryCount{ 0 };
    quint32 m_gptEntrySizeBytes{ 0 };
};
} // namespace UDI
