#include <imaging/hasher.hpp>

#include <QtEndian>


namespace UDI
{
Hasher::Hasher(bool withSha256)
    : m_fastState(XXH3_createState(), &XXH3_freeState)
{
    if (!m_fastState)
    {
        qFatal("Hasher: could not allocate an XXH3 state");
    }

    XXH3_64bits_reset(m_fastState.get());

    if (withSha256)
    {
        m_sha256 = std::make_unique<QCryptographicHash>(QCryptographicHash::Sha256);
    }
}

void Hasher::update(const std::uint8_t* data, std::size_t sizeBytes)
{
    XXH3_64bits_update(m_fastState.get(), data, sizeBytes);

    if (m_sha256)
    {
        m_sha256->addData(QByteArrayView{ data, static_cast<qsizetype>(sizeBytes) });
    }
}

QByteArray Hasher::getFastDigestHex() const
{
    const XXH64_hash_t digest = XXH3_64bits_digest(m_fastState.get());

    // Big-endian text, so the digest reads the same as the xxhsum command-line tool prints it.
    XXH64_canonical_t canonical;
    XXH64_canonicalFromHash(&canonical, digest);

    return QByteArray{ reinterpret_cast<const char*>(canonical.digest), sizeof(canonical.digest) }.toHex();
}

QByteArray Hasher::getSha256Hex() const
{
    return m_sha256 ? m_sha256->result().toHex() : QByteArray{};
}
} // namespace UDI
