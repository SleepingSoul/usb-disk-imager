#include <utils/crc32.hpp>

#include <array>


namespace UDI
{
namespace
{
constexpr quint32 ReversedPolynomial = 0xEDB88320u;

std::array<quint32, 256> buildCrc32Table()
{
    std::array<quint32, 256> table{};

    for (std::size_t index = 0; index < table.size(); ++index)
    {
        quint32 remainder = static_cast<quint32>(index);

        for (int bit = 0; bit < 8; ++bit)
        {
            remainder = (remainder & 1u) ? ((remainder >> 1) ^ ReversedPolynomial) : (remainder >> 1);
        }

        table[index] = remainder;
    }

    return table;
}

const std::array<quint32, 256> Crc32Table = buildCrc32Table();
} // namespace

quint32 computeCrc32(const std::uint8_t* data, std::size_t sizeBytes)
{
    quint32 crc = 0xFFFFFFFFu;

    for (std::size_t index = 0; index < sizeBytes; ++index)
    {
        crc = Crc32Table[(crc ^ data[index]) & 0xFFu] ^ (crc >> 8);
    }

    return crc ^ 0xFFFFFFFFu;
}
} // namespace UDI
