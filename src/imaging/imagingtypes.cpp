#include <imaging/imagingtypes.hpp>


namespace UDI
{
using namespace Qt::Literals::StringLiterals;

namespace
{
const QLatin1StringView TrimNoneToken{ "none" };
const QLatin1StringView TrimPartitionsToken{ "partitions" };
const QLatin1StringView TrimTrailingZerosToken{ "zeros" };
} // namespace

QLatin1StringView trimModeToken(TrimMode mode)
{
    switch (mode)
    {
    case TrimMode::Partitions:    return TrimPartitionsToken;
    case TrimMode::TrailingZeros: return TrimTrailingZerosToken;
    case TrimMode::None:          break;
    }

    return TrimNoneToken;
}

TrimMode trimModeFromToken(const QByteArray& token)
{
    if (token == TrimPartitionsToken)
    {
        return TrimMode::Partitions;
    }

    if (token == TrimTrailingZerosToken)
    {
        return TrimMode::TrailingZeros;
    }

    return TrimMode::None;
}
} // namespace UDI
