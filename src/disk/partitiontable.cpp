#include <disk/partitiontable.hpp>

#include <algorithm>
#include <array>
#include <cstring>

#include <QLoggingCategory>

#include <utils/crc32.hpp>


namespace UDI
{
using namespace Qt::Literals::StringLiterals;

namespace
{
namespace Mbr
{
constexpr std::size_t SignatureOffset = 510;
constexpr std::size_t FirstEntryOffset = 446;
constexpr std::size_t EntrySizeBytes = 16;
constexpr std::size_t EntryCount = 4;
constexpr std::size_t TypeOffset = 4;
constexpr std::size_t FirstSectorOffset = 8;
constexpr std::size_t SectorCountOffset = 12;
constexpr std::uint8_t GptProtectiveType = 0xEE;
} // namespace Mbr

namespace Gpt
{
constexpr std::size_t HeaderSectorIndex = 1;
constexpr std::size_t SignatureSizeBytes = 8;
constexpr std::size_t HeaderSizeOffset = 0x0C;
constexpr std::size_t HeaderCrcOffset = 0x10;
constexpr std::size_t MyLbaOffset = 0x18;
constexpr std::size_t AlternateLbaOffset = 0x20;
constexpr std::size_t LastUsableLbaOffset = 0x30;
constexpr std::size_t EntryArrayLbaOffset = 0x48;
constexpr std::size_t EntryCountOffset = 0x50;
constexpr std::size_t EntrySizeOffset = 0x54;
constexpr std::size_t EntryArrayCrcOffset = 0x58;
constexpr std::size_t MinimumHeaderSizeBytes = 92;

constexpr std::size_t EntryTypeGuidOffset = 0x00;
constexpr std::size_t EntryFirstLbaOffset = 0x20;
constexpr std::size_t EntryLastLbaOffset = 0x28;
constexpr std::size_t GuidSizeBytes = 16;
constexpr std::size_t MinimumEntrySizeBytes = 128;
constexpr std::size_t MaximumEntrySizeBytes = 4096;
constexpr quint32 MaximumEntryCount = 8192;

const std::array<std::uint8_t, SignatureSizeBytes> Signature{ 'E', 'F', 'I', ' ', 'P', 'A', 'R', 'T' };
} // namespace Gpt

quint32 readLittleEndian32(const std::uint8_t* data)
{
    return static_cast<quint32>(data[0])
        | (static_cast<quint32>(data[1]) << 8)
        | (static_cast<quint32>(data[2]) << 16)
        | (static_cast<quint32>(data[3]) << 24);
}

quint64 readLittleEndian64(const std::uint8_t* data)
{
    return static_cast<quint64>(readLittleEndian32(data))
        | (static_cast<quint64>(readLittleEndian32(data + 4)) << 32);
}

void writeLittleEndian32(std::uint8_t* data, quint32 value)
{
    data[0] = static_cast<std::uint8_t>(value & 0xFFu);
    data[1] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
    data[2] = static_cast<std::uint8_t>((value >> 16) & 0xFFu);
    data[3] = static_cast<std::uint8_t>((value >> 24) & 0xFFu);
}

void writeLittleEndian64(std::uint8_t* data, quint64 value)
{
    writeLittleEndian32(data, static_cast<quint32>(value & 0xFFFFFFFFu));
    writeLittleEndian32(data + 4, static_cast<quint32>(value >> 32));
}

// The first three fields of a GPT GUID are stored little-endian and the last two big-endian, so the
// canonical text form is the only representation worth comparing against.
QString formatGuid(const std::uint8_t* data)
{
    return QStringLiteral("%1-%2-%3-%4%5-%6%7%8%9%10%11")
        .arg(readLittleEndian32(data), 8, 16, QLatin1Char('0'))
        .arg(static_cast<quint32>(data[4] | (data[5] << 8)), 4, 16, QLatin1Char('0'))
        .arg(static_cast<quint32>(data[6] | (data[7] << 8)), 4, 16, QLatin1Char('0'))
        .arg(data[8], 2, 16, QLatin1Char('0'))
        .arg(data[9], 2, 16, QLatin1Char('0'))
        .arg(data[10], 2, 16, QLatin1Char('0'))
        .arg(data[11], 2, 16, QLatin1Char('0'))
        .arg(data[12], 2, 16, QLatin1Char('0'))
        .arg(data[13], 2, 16, QLatin1Char('0'))
        .arg(data[14], 2, 16, QLatin1Char('0'))
        .arg(data[15], 2, 16, QLatin1Char('0'))
        .toUpper();
}

QString gptPartitionTypeName(const std::uint8_t* typeGuid)
{
    static const std::array KnownTypes{
        std::pair{ "C12A7328-F81F-11D2-BA4B-00A0C93EC93B"_L1, "EFI System"_L1 },
        std::pair{ "EBD0A0A2-B9E5-4433-87C0-68B6B72699C7"_L1, "Microsoft basic data"_L1 },
        std::pair{ "E3C9E316-0FC8-4FF0-B724-31000B4B7A9B"_L1, "Microsoft reserved"_L1 },
        std::pair{ "DE94BBA4-06D1-4D40-A16A-BFD50179D6AC"_L1, "Windows recovery"_L1 },
        std::pair{ "0FC63DAF-8483-4772-8E79-3D69D8477DE4"_L1, "Linux filesystem"_L1 },
        std::pair{ "0657FD6D-A4AB-43C4-84E5-0933C84B4F4F"_L1, "Linux swap"_L1 },
        std::pair{ "E6D6D379-F507-44C2-A23C-238F2A3DF928"_L1, "Linux LVM"_L1 },
        std::pair{ "BC13C2FF-59E6-4262-A352-B275FD6F7172"_L1, "Linux extended boot"_L1 },
        std::pair{ "933AC7E1-2EB4-4F13-B844-0E14E2AEF915"_L1, "Linux home"_L1 },
        std::pair{ "48465300-0000-11AA-AA11-00306543ECAC"_L1, "Apple HFS+"_L1 },
        std::pair{ "7C3457EF-0000-11AA-AA11-00306543ECAC"_L1, "Apple APFS"_L1 },
        std::pair{ "426F6F74-0000-11AA-AA11-00306543ECAC"_L1, "Apple boot"_L1 }
    };

    const QString guid = formatGuid(typeGuid);

    const auto match = std::find_if(KnownTypes.cbegin(), KnownTypes.cend(), [&guid](const auto& knownType)
    {
        return guid == knownType.first;
    });

    return match != KnownTypes.cend() ? QString{ match->second } : guid;
}

QString mbrPartitionTypeName(std::uint8_t type)
{
    switch (type)
    {
    case 0x01: return "FAT12"_L1;
    case 0x04:
    case 0x06:
    case 0x0E: return "FAT16"_L1;
    case 0x05:
    case 0x0F: return "Extended"_L1;
    case 0x07: return "NTFS / exFAT"_L1;
    case 0x0B:
    case 0x0C: return "FAT32"_L1;
    case 0x82: return "Linux swap"_L1;
    case 0x83: return "Linux"_L1;
    case 0x8E: return "Linux LVM"_L1;
    case 0xA5: return "FreeBSD"_L1;
    case 0xAF: return "Apple HFS+"_L1;
    case 0xEE: return "GPT protective"_L1;
    case 0xEF: return "EFI System"_L1;
    default:   break;
    }

    return QStringLiteral("Type 0x%1").arg(type, 2, 16, QLatin1Char('0')).toUpper();
}

bool isAllZero(const std::uint8_t* data, std::size_t sizeBytes)
{
    return std::all_of(data, data + sizeBytes, [](std::uint8_t byte)
    {
        return byte == 0;
    });
}

bool gptHeaderChecksumMatches(const std::uint8_t* header, quint32 headerSizeBytes)
{
    std::vector<std::uint8_t> scratch(header, header + headerSizeBytes);
    const quint32 storedCrc = readLittleEndian32(scratch.data() + Gpt::HeaderCrcOffset);

    writeLittleEndian32(scratch.data() + Gpt::HeaderCrcOffset, 0u);

    return computeCrc32(scratch.data(), scratch.size()) == storedCrc;
}

void refreshGptHeaderChecksum(std::vector<std::uint8_t>& headerSector)
{
    const quint32 headerSizeBytes = readLittleEndian32(headerSector.data() + Gpt::HeaderSizeOffset);

    writeLittleEndian32(headerSector.data() + Gpt::HeaderCrcOffset, 0u);
    writeLittleEndian32(headerSector.data() + Gpt::HeaderCrcOffset,
        computeCrc32(headerSector.data(), headerSizeBytes));
}
} // namespace

PartitionTable PartitionTable::parse(const std::vector<std::uint8_t>& head,
    quint32 sectorSizeBytes,
    quint64 deviceSizeBytes)
{
    PartitionTable table;
    table.m_sectorSizeBytes = sectorSizeBytes;
    table.m_deviceSizeBytes = deviceSizeBytes;

    if (sectorSizeBytes == 0 || head.size() < sectorSizeBytes)
    {
        return table;
    }

    if (table.parseGpt(head))
    {
        table.m_scheme = PartitionScheme::Gpt;
    }
    else if (table.parseMbr(head))
    {
        table.m_scheme = PartitionScheme::Mbr;
    }

    return table;
}

bool PartitionTable::parseGpt(const std::vector<std::uint8_t>& head)
{
    const std::size_t headerOffset = Gpt::HeaderSectorIndex * m_sectorSizeBytes;

    if (head.size() < headerOffset + Gpt::MinimumHeaderSizeBytes)
    {
        return false;
    }

    const std::uint8_t* const header = head.data() + headerOffset;

    if (std::memcmp(header, Gpt::Signature.data(), Gpt::Signature.size()) != 0)
    {
        return false;
    }

    const quint32 headerSizeBytes = readLittleEndian32(header + Gpt::HeaderSizeOffset);

    if (headerSizeBytes < Gpt::MinimumHeaderSizeBytes || headerSizeBytes > m_sectorSizeBytes)
    {
        qWarning() << "GPT header declares an implausible size of" << headerSizeBytes << "bytes";
        return false;
    }

    // A header whose checksum does not match is corrupt, and rewriting a corrupt header into a trimmed
    // image would turn a recoverable disk into a confidently wrong one.
    if (!gptHeaderChecksumMatches(header, headerSizeBytes))
    {
        qWarning() << "GPT header checksum mismatch; treating the device as unpartitioned";
        return false;
    }

    const quint64 entryArrayFirstSector = readLittleEndian64(header + Gpt::EntryArrayLbaOffset);
    const quint32 entryCount = readLittleEndian32(header + Gpt::EntryCountOffset);
    const quint32 entrySizeBytes = readLittleEndian32(header + Gpt::EntrySizeOffset);
    const quint32 entryArrayCrc = readLittleEndian32(header + Gpt::EntryArrayCrcOffset);

    if (entrySizeBytes < Gpt::MinimumEntrySizeBytes || entrySizeBytes > Gpt::MaximumEntrySizeBytes
        || entryCount == 0 || entryCount > Gpt::MaximumEntryCount
        || entryArrayFirstSector < Gpt::HeaderSectorIndex + 1)
    {
        qWarning() << "GPT header describes an implausible entry array:" << entryCount
                   << "entries of" << entrySizeBytes << "bytes at sector" << entryArrayFirstSector;
        return false;
    }

    const std::size_t entryArrayOffset = static_cast<std::size_t>(entryArrayFirstSector) * m_sectorSizeBytes;
    const std::size_t entryArraySizeBytes = static_cast<std::size_t>(entryCount) * entrySizeBytes;

    if (head.size() < entryArrayOffset + entryArraySizeBytes)
    {
        qWarning() << "GPT entry array lies beyond the inspected head of the device";
        return false;
    }

    const std::uint8_t* const entryArray = head.data() + entryArrayOffset;

    if (computeCrc32(entryArray, entryArraySizeBytes) != entryArrayCrc)
    {
        qWarning() << "GPT partition-entry array checksum mismatch";
        return false;
    }

    for (quint32 index = 0; index < entryCount; ++index)
    {
        const std::uint8_t* const entry = entryArray + static_cast<std::size_t>(index) * entrySizeBytes;

        if (isAllZero(entry + Gpt::EntryTypeGuidOffset, Gpt::GuidSizeBytes))
        {
            continue;
        }

        const quint64 firstSector = readLittleEndian64(entry + Gpt::EntryFirstLbaOffset);
        const quint64 lastSector = readLittleEndian64(entry + Gpt::EntryLastLbaOffset);

        if (lastSector < firstSector)
        {
            continue;
        }

        m_partitions.push_back(PartitionEntry{ firstSector,
            lastSector - firstSector + 1,
            gptPartitionTypeName(entry + Gpt::EntryTypeGuidOffset) });

        m_lastUsedSector = std::max(m_lastUsedSector, lastSector);
    }

    m_gptPrimaryHeader.assign(header, header + m_sectorSizeBytes);
    m_gptEntryArray.assign(entryArray, entryArray + entryArraySizeBytes);
    m_gptEntryArrayFirstSector = entryArrayFirstSector;
    m_gptEntryCount = entryCount;
    m_gptEntrySizeBytes = entrySizeBytes;

    return true;
}

bool PartitionTable::parseMbr(const std::vector<std::uint8_t>& head)
{
    if (head.size() < Mbr::SignatureOffset + 2)
    {
        return false;
    }

    if (head[Mbr::SignatureOffset] != 0x55 || head[Mbr::SignatureOffset + 1] != 0xAA)
    {
        return false;
    }

    m_mbrSector.assign(head.cbegin(), head.cbegin() + m_sectorSizeBytes);

    for (std::size_t index = 0; index < Mbr::EntryCount; ++index)
    {
        const std::uint8_t* const entry = head.data() + Mbr::FirstEntryOffset + index * Mbr::EntrySizeBytes;
        const std::uint8_t type = entry[Mbr::TypeOffset];

        if (type == 0x00 || type == Mbr::GptProtectiveType)
        {
            continue;
        }

        const quint64 firstSector = readLittleEndian32(entry + Mbr::FirstSectorOffset);
        const quint64 sectorCount = readLittleEndian32(entry + Mbr::SectorCountOffset);

        if (sectorCount == 0)
        {
            continue;
        }

        m_partitions.push_back(PartitionEntry{ firstSector, sectorCount, mbrPartitionTypeName(type) });

        // An extended partition's own extent spans every logical partition inside it, so the EBR chain
        // does not have to be walked to find where the used area ends.
        m_lastUsedSector = std::max(m_lastUsedSector, firstSector + sectorCount - 1);
    }

    return !m_partitions.empty();
}

const PartitionEntry* PartitionTable::getLastPartition() const
{
    const auto lastPartitionIt = std::max_element(m_partitions.cbegin(), m_partitions.cend(),
        [](const PartitionEntry& a, const PartitionEntry& b)
        {
            return a.firstSector + a.sectorCount < b.firstSector + b.sectorCount;
        });

    return lastPartitionIt != m_partitions.cend() ? &(*lastPartitionIt) : nullptr;
}

std::optional<TrimPlan> PartitionTable::planTrim(quint64 alignmentBytes) const
{
    if (m_scheme == PartitionScheme::None || m_lastUsedSector == 0)
    {
        return std::nullopt;
    }

    if (m_scheme == PartitionScheme::Gpt)
    {
        return planGptTrim();
    }

    const quint64 dataSizeBytes = (m_lastUsedSector + 1) * m_sectorSizeBytes;
    const quint64 alignment = std::max<quint64>(alignmentBytes, m_sectorSizeBytes);
    const quint64 alignedSizeBytes = ((dataSizeBytes + alignment - 1) / alignment) * alignment;
    const quint64 imageSizeBytes = std::min(alignedSizeBytes, m_deviceSizeBytes);

    if (imageSizeBytes >= m_deviceSizeBytes)
    {
        return std::nullopt;
    }

    TrimPlan plan;
    plan.dataSizeBytes = imageSizeBytes;
    plan.imageSizeBytes = imageSizeBytes;

    return plan;
}

std::optional<TrimPlan> PartitionTable::planGptTrim() const
{
    if (m_gptPrimaryHeader.size() != m_sectorSizeBytes || m_gptEntryArray.empty())
    {
        return std::nullopt;
    }

    const quint64 dataSectorCount = m_lastUsedSector + 1;
    const quint64 entryArraySectorCount =
        (static_cast<quint64>(m_gptEntryArray.size()) + m_sectorSizeBytes - 1) / m_sectorSizeBytes;
    const quint64 imageSectorCount = dataSectorCount + entryArraySectorCount + 1;
    const quint64 imageSizeBytes = imageSectorCount * m_sectorSizeBytes;

    if (imageSizeBytes >= m_deviceSizeBytes)
    {
        return std::nullopt;
    }

    const quint64 backupHeaderSector = imageSectorCount - 1;
    const quint64 backupEntryArrayFirstSector = dataSectorCount;

    GptTrimTrailer trailer;
    trailer.primaryHeaderSectorIndex = Gpt::HeaderSectorIndex;
    trailer.primaryHeaderSector = m_gptPrimaryHeader;

    writeLittleEndian64(trailer.primaryHeaderSector.data() + Gpt::MyLbaOffset, Gpt::HeaderSectorIndex);
    writeLittleEndian64(trailer.primaryHeaderSector.data() + Gpt::AlternateLbaOffset, backupHeaderSector);
    writeLittleEndian64(trailer.primaryHeaderSector.data() + Gpt::LastUsableLbaOffset, m_lastUsedSector);
    refreshGptHeaderChecksum(trailer.primaryHeaderSector);

    std::vector<std::uint8_t> backupHeaderSectorBytes = trailer.primaryHeaderSector;
    writeLittleEndian64(backupHeaderSectorBytes.data() + Gpt::MyLbaOffset, backupHeaderSector);
    writeLittleEndian64(backupHeaderSectorBytes.data() + Gpt::AlternateLbaOffset, Gpt::HeaderSectorIndex);
    writeLittleEndian64(backupHeaderSectorBytes.data() + Gpt::EntryArrayLbaOffset, backupEntryArrayFirstSector);
    refreshGptHeaderChecksum(backupHeaderSectorBytes);

    trailer.trailerSectors.resize(static_cast<std::size_t>((entryArraySectorCount + 1) * m_sectorSizeBytes), 0u);
    std::copy(m_gptEntryArray.cbegin(), m_gptEntryArray.cend(), trailer.trailerSectors.begin());
    std::copy(backupHeaderSectorBytes.cbegin(), backupHeaderSectorBytes.cend(),
        trailer.trailerSectors.begin() + static_cast<std::ptrdiff_t>(entryArraySectorCount * m_sectorSizeBytes));

    TrimPlan plan;
    plan.dataSizeBytes = dataSectorCount * m_sectorSizeBytes;
    plan.imageSizeBytes = imageSizeBytes;
    plan.gptTrailer = std::move(trailer);

    return plan;
}

std::optional<TrimPlan> PartitionTable::planShrinkLastPartition(quint64 newSectorCount) const
{
    const PartitionEntry* const target = getLastPartition();

    if (!target || newSectorCount == 0 || newSectorCount > target->sectorCount)
    {
        return std::nullopt;
    }

    return resizeLastPartitionTo(*target, newSectorCount, m_deviceSizeBytes);
}

std::optional<TrimPlan> PartitionTable::planGrowLastPartition(quint64 targetSizeBytes, quint64 alignmentBytes) const
{
    const PartitionEntry* const target = getLastPartition();

    if (!target || m_sectorSizeBytes == 0)
    {
        return std::nullopt;
    }

    const quint64 targetSectorCount = targetSizeBytes / m_sectorSizeBytes;
    // A GPT device reserves its trailing metadata past the data area; an MBR device has none, so the
    // partition may run to the very last sector.
    const quint64 reservedTrailerSectors = m_scheme == PartitionScheme::Gpt
        ? (static_cast<quint64>(m_gptEntryArray.size()) + m_sectorSizeBytes - 1) / m_sectorSizeBytes + 1
        : 0;

    if (targetSectorCount <= reservedTrailerSectors)
    {
        return std::nullopt;
    }

    const quint64 maxLastSector = targetSectorCount - reservedTrailerSectors - 1;

    if (maxLastSector < target->firstSector)
    {
        return std::nullopt;
    }

    const quint64 maxSectorCount = maxLastSector - target->firstSector + 1;
    const quint64 alignment = std::max<quint64>(alignmentBytes, m_sectorSizeBytes) / m_sectorSizeBytes;
    const quint64 alignedSectorCount = alignment > 0 ? (maxSectorCount / alignment) * alignment : maxSectorCount;

    if (alignedSectorCount <= target->sectorCount)
    {
        return std::nullopt;
    }

    return resizeLastPartitionTo(*target, alignedSectorCount, targetSizeBytes);
}

std::optional<TrimPlan> PartitionTable::resizeLastPartitionTo(const PartitionEntry& target,
    quint64 newSectorCount,
    quint64 maxAllowedSizeBytes) const
{
    if (m_scheme == PartitionScheme::Gpt)
    {
        return planGptResize(target, target.firstSector + newSectorCount - 1, maxAllowedSizeBytes);
    }

    return planMbrResize(target, newSectorCount, maxAllowedSizeBytes);
}

std::optional<TrimPlan> PartitionTable::planGptResize(const PartitionEntry& target,
    quint64 newLastSector,
    quint64 maxAllowedSizeBytes) const
{
    if (m_gptPrimaryHeader.size() != m_sectorSizeBytes || m_gptEntryArray.empty())
    {
        return std::nullopt;
    }

    std::vector<std::uint8_t> patchedEntryArray = m_gptEntryArray;
    bool patched = false;

    for (quint32 index = 0; index < m_gptEntryCount; ++index)
    {
        std::uint8_t* const entry = patchedEntryArray.data() + static_cast<std::size_t>(index) * m_gptEntrySizeBytes;

        if (isAllZero(entry + Gpt::EntryTypeGuidOffset, Gpt::GuidSizeBytes))
        {
            continue;
        }

        if (readLittleEndian64(entry + Gpt::EntryFirstLbaOffset) == target.firstSector)
        {
            writeLittleEndian64(entry + Gpt::EntryLastLbaOffset, newLastSector);
            patched = true;
            break;
        }
    }

    if (!patched)
    {
        return std::nullopt;
    }

    // The entry just changed, so the checksum the header carries for the whole array is now stale;
    // trimming never patches an entry, which is the one thing that lets it leave this checksum alone.
    const quint32 entryArrayCrc = computeCrc32(patchedEntryArray.data(), patchedEntryArray.size());

    const quint64 dataSectorCount = newLastSector + 1;
    const quint64 entryArraySectorCount =
        (static_cast<quint64>(patchedEntryArray.size()) + m_sectorSizeBytes - 1) / m_sectorSizeBytes;
    const quint64 imageSectorCount = dataSectorCount + entryArraySectorCount + 1;
    const quint64 imageSizeBytes = imageSectorCount * m_sectorSizeBytes;

    if (imageSizeBytes > maxAllowedSizeBytes)
    {
        return std::nullopt;
    }

    const quint64 backupHeaderSector = imageSectorCount - 1;
    const quint64 backupEntryArrayFirstSector = dataSectorCount;

    GptTrimTrailer trailer;
    trailer.primaryHeaderSectorIndex = Gpt::HeaderSectorIndex;
    trailer.primaryHeaderSector = m_gptPrimaryHeader;

    writeLittleEndian64(trailer.primaryHeaderSector.data() + Gpt::MyLbaOffset, Gpt::HeaderSectorIndex);
    writeLittleEndian64(trailer.primaryHeaderSector.data() + Gpt::AlternateLbaOffset, backupHeaderSector);
    writeLittleEndian64(trailer.primaryHeaderSector.data() + Gpt::LastUsableLbaOffset, newLastSector);
    writeLittleEndian32(trailer.primaryHeaderSector.data() + Gpt::EntryArrayCrcOffset, entryArrayCrc);
    refreshGptHeaderChecksum(trailer.primaryHeaderSector);

    std::vector<std::uint8_t> backupHeaderSectorBytes = trailer.primaryHeaderSector;
    writeLittleEndian64(backupHeaderSectorBytes.data() + Gpt::MyLbaOffset, backupHeaderSector);
    writeLittleEndian64(backupHeaderSectorBytes.data() + Gpt::AlternateLbaOffset, Gpt::HeaderSectorIndex);
    writeLittleEndian64(backupHeaderSectorBytes.data() + Gpt::EntryArrayLbaOffset, backupEntryArrayFirstSector);
    refreshGptHeaderChecksum(backupHeaderSectorBytes);

    trailer.trailerSectors.resize(static_cast<std::size_t>((entryArraySectorCount + 1) * m_sectorSizeBytes), 0u);
    std::copy(patchedEntryArray.cbegin(), patchedEntryArray.cend(), trailer.trailerSectors.begin());
    std::copy(backupHeaderSectorBytes.cbegin(), backupHeaderSectorBytes.cend(),
        trailer.trailerSectors.begin() + static_cast<std::ptrdiff_t>(entryArraySectorCount * m_sectorSizeBytes));

    trailer.primaryEntryArrayFirstSector = m_gptEntryArrayFirstSector;
    trailer.patchedPrimaryEntryArray = std::move(patchedEntryArray);

    TrimPlan plan;
    plan.dataSizeBytes = dataSectorCount * m_sectorSizeBytes;
    plan.imageSizeBytes = imageSizeBytes;
    plan.gptTrailer = std::move(trailer);

    return plan;
}

std::optional<TrimPlan> PartitionTable::planMbrResize(const PartitionEntry& target,
    quint64 newSectorCount,
    quint64 maxAllowedSizeBytes) const
{
    if (m_mbrSector.size() != m_sectorSizeBytes)
    {
        return std::nullopt;
    }

    std::vector<std::uint8_t> patchedSector = m_mbrSector;
    bool patched = false;

    for (std::size_t index = 0; index < Mbr::EntryCount; ++index)
    {
        std::uint8_t* const entry = patchedSector.data() + Mbr::FirstEntryOffset + index * Mbr::EntrySizeBytes;
        const std::uint8_t type = entry[Mbr::TypeOffset];

        if (type == 0x00 || type == Mbr::GptProtectiveType)
        {
            continue;
        }

        if (readLittleEndian32(entry + Mbr::FirstSectorOffset) == target.firstSector)
        {
            writeLittleEndian32(entry + Mbr::SectorCountOffset, static_cast<quint32>(newSectorCount));
            patched = true;
            break;
        }
    }

    if (!patched)
    {
        return std::nullopt;
    }

    const quint64 newLastSector = target.firstSector + newSectorCount - 1;
    const quint64 imageSizeBytes = (newLastSector + 1) * m_sectorSizeBytes;

    if (imageSizeBytes > maxAllowedSizeBytes)
    {
        return std::nullopt;
    }

    TrimPlan plan;
    plan.dataSizeBytes = imageSizeBytes;
    plan.imageSizeBytes = imageSizeBytes;
    plan.patchedFirstSector = std::move(patchedSector);

    return plan;
}
} // namespace UDI
