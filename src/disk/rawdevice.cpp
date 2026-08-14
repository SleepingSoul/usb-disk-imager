#include <disk/rawdevice.hpp>

#include <new>


namespace UDI
{
AlignedBuffer::AlignedBuffer(std::size_t sizeBytes, std::size_t alignmentBytes)
    : m_sizeBytes(sizeBytes)
    , m_alignmentBytes(alignmentBytes)
{
    // Aligned operator new requires the size to be a multiple of the alignment.
    m_allocatedBytes = ((sizeBytes + alignmentBytes - 1) / alignmentBytes) * alignmentBytes;
    m_data = static_cast<std::uint8_t*>(::operator new(m_allocatedBytes, std::align_val_t{ m_alignmentBytes }));
}

AlignedBuffer::~AlignedBuffer()
{
    ::operator delete(m_data, m_allocatedBytes, std::align_val_t{ m_alignmentBytes });
}
} // namespace UDI
