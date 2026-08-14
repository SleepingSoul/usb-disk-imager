#pragma once

#include <cstdint>
#include <memory>

#include <xxhash.h>

#include <QByteArray>
#include <QCryptographicHash>


namespace UDI
{
// XXH3 hashes at roughly memory bandwidth, so a digest of the whole transfer is free next to the device
// I/O and can be computed on every read and write. SHA-256 is around an order of magnitude slower, so it
// is only computed when the user asks for a digest to compare against a published one.
class Hasher
{
    Q_DISABLE_COPY_MOVE(Hasher)
public:
    explicit Hasher(bool withSha256);

    void update(const std::uint8_t* data, std::size_t sizeBytes);

    QByteArray getFastDigestHex() const;

    // Empty unless the hasher was constructed with SHA-256 enabled.
    QByteArray getSha256Hex() const;

private:
    std::unique_ptr<XXH3_state_t, decltype(&XXH3_freeState)> m_fastState;
    std::unique_ptr<QCryptographicHash> m_sha256;
};
} // namespace UDI
