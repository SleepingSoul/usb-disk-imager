#pragma once

#include <cstdint>

#include <QtTypes>


namespace UDI
{
// CRC-32/ISO-HDLC, the checksum GPT headers and partition-entry arrays carry. Needed because a trimmed
// GPT image gets its headers rewritten, and a header with a stale checksum is worse than no header.
quint32 computeCrc32(const std::uint8_t* data, std::size_t sizeBytes);
} // namespace UDI
