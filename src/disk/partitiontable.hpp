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
    // Only set by a resize plan: a trim never changes any entry's contents, so the primary copy near the
    // start of the medium stays valid on its own, but a resize patches one entry's extent and therefore
    // has to rewrite both copies of the array, not just the one that lives inside this trailer.
    std::optional<std::vector<std::uint8_t>> patchedPrimaryEntryArray;
    quint64 primaryEntryArrayFirstSector{ 0 };
};

struct TrimPlan
{
    // Bytes copied verbatim from the start of the device.
    quint64 dataSizeBytes{ 0 };
    // Size of the resulting image, larger than dataSizeBytes when a GPT trailer is appended.
    quint64 imageSizeBytes{ 0 };
    std::optional<GptTrimTrailer> gptTrailer;
    // Only set by a resize plan on an MBR table: sector 0, with the resized partition's entry patched
    // in, ready to overwrite the image's or device's existing sector 0.
    std::optional<std::vector<std::uint8_t>> patchedFirstSector;
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
    quint32 getSectorSizeBytes() const { return m_sectorSizeBytes; }

    // Inclusive index of the last sector occupied by any partition, or 0 when there is nothing to trim.
    quint64 getLastUsedSector() const { return m_lastUsedSector; }

    // The partition occupying the highest sectors, or nullptr when there are none. This is the one a
    // filesystem shrink or grow targets — the only partition that can change size without moving every
    // partition after it.
    const PartitionEntry* getLastPartition() const;

    // std::nullopt when the device has no usable table, when the partitions reach the end of the medium
    // anyway, or when the GPT metadata is laid out too unusually to be rebuilt safely.
    std::optional<TrimPlan> planTrim(quint64 alignmentBytes) const;

    // Rewrites the last partition's extent to \a newSectorCount sectors — smaller for a filesystem
    // shrink, larger for a grow — and produces the same shape of plan planTrim() would for the resulting
    // boundary: a GPT rebuild carries the resized entry and a re-checksummed trailer; an MBR rebuild
    // carries a patched copy of sector 0. std::nullopt when there is no last partition, \a newSectorCount
    // is zero, or the result would not fit the medium this table was parsed against.
    std::optional<TrimPlan> planShrinkLastPartition(quint64 newSectorCount) const;

    // Grows the last partition to the most it can hold while a GPT device still has room for its
    // trailing metadata, up to \a targetSizeBytes, then rounds down to \a alignmentBytes. std::nullopt
    // when there is no last partition or it already reaches that size.
    std::optional<TrimPlan> planGrowLastPartition(quint64 targetSizeBytes, quint64 alignmentBytes) const;

private:
    bool parseGpt(const std::vector<std::uint8_t>& head);
    bool parseMbr(const std::vector<std::uint8_t>& head);
    std::optional<TrimPlan> planGptTrim() const;

    std::optional<TrimPlan> resizeLastPartitionTo(const PartitionEntry& target,
        quint64 newSectorCount,
        quint64 maxAllowedSizeBytes) const;
    std::optional<TrimPlan> planGptResize(const PartitionEntry& target,
        quint64 newLastSector,
        quint64 maxAllowedSizeBytes) const;
    std::optional<TrimPlan> planMbrResize(const PartitionEntry& target,
        quint64 newSectorCount,
        quint64 maxAllowedSizeBytes) const;

    PartitionScheme m_scheme{ PartitionScheme::None };
    std::vector<PartitionEntry> m_partitions;
    quint64 m_lastUsedSector{ 0 };

    quint32 m_sectorSizeBytes{ 512 };
    quint64 m_deviceSizeBytes{ 0 };

    std::vector<std::uint8_t> m_mbrSector;

    std::vector<std::uint8_t> m_gptPrimaryHeader;
    std::vector<std::uint8_t> m_gptEntryArray;
    quint64 m_gptEntryArrayFirstSector{ 0 };
    quint32 m_gptEntryCount{ 0 };
    quint32 m_gptEntrySizeBytes{ 0 };
};
} // namespace UDI
