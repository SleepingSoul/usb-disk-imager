#pragma once

#include <cstdint>
#include <memory>

#include <QString>

#include <disk/disktypes.hpp>


namespace UDI
{
// Unbuffered device I/O has to be sector aligned — Windows enforces it on handles opened without
// caching, and macOS raw character devices reject anything else — so every transfer buffer comes from
// here rather than from a plain std::vector.
class AlignedBuffer
{
    Q_DISABLE_COPY_MOVE(AlignedBuffer)
public:
    explicit AlignedBuffer(std::size_t sizeBytes, std::size_t alignmentBytes = 4096);
    ~AlignedBuffer();

    std::uint8_t* data() { return m_data; }
    const std::uint8_t* data() const { return m_data; }
    std::size_t size() const { return m_sizeBytes; }

private:
    std::uint8_t* m_data{ nullptr };
    std::size_t m_sizeBytes{ 0 };
    std::size_t m_allocatedBytes{ 0 };
    std::size_t m_alignmentBytes{ 0 };
};

// Access to a whole physical device, bypassing the page cache so that imaging a 32 GB card neither
// evicts the user's working set nor reports a write as finished while it still sits in RAM.
class RawDevice
{
public:
    enum class AccessMode
    {
        Read,
        Write
    };

    virtual ~RawDevice() = default;

    // Returns nullptr and fills \a errorMessage with text fit for the UI. Opening for writing expects
    // the caller to hold a VolumeControl lock on the device already.
    static std::unique_ptr<RawDevice> open(const DeviceInfo& device, AccessMode mode, QString& errorMessage);

    // Offsets and sizes must be multiples of getSectorSizeBytes(); the imaging job pads its last chunk
    // to satisfy that.
    virtual qint64 read(std::uint8_t* data, qint64 sizeBytes) = 0;
    virtual qint64 write(const std::uint8_t* data, qint64 sizeBytes) = 0;
    virtual bool seek(quint64 offsetBytes) = 0;
    virtual bool sync() = 0;

    virtual quint64 getSizeBytes() const = 0;
    virtual quint32 getSectorSizeBytes() const = 0;

    const QString& getLastError() const { return m_lastError; }

protected:
    void setLastError(QString message) { m_lastError = std::move(message); }

private:
    QString m_lastError;
};
} // namespace UDI
