#include <disk/windows/windowsutils.hpp>

#include <algorithm>
#include <utility>


namespace UDI::Windows
{
ScopedHandle::ScopedHandle(HANDLE handle)
    : m_handle(handle)
{}

ScopedHandle::ScopedHandle(ScopedHandle&& other) noexcept
    : m_handle(std::exchange(other.m_handle, INVALID_HANDLE_VALUE))
{}

ScopedHandle& ScopedHandle::operator=(ScopedHandle&& other) noexcept
{
    if (this != &other)
    {
        reset();
        m_handle = std::exchange(other.m_handle, INVALID_HANDLE_VALUE);
    }

    return *this;
}

ScopedHandle::~ScopedHandle()
{
    reset();
}

bool ScopedHandle::isValid() const
{
    return m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr;
}

void ScopedHandle::reset()
{
    if (isValid())
    {
        CloseHandle(m_handle);
    }

    m_handle = INVALID_HANDLE_VALUE;
}

HANDLE ScopedHandle::release()
{
    return std::exchange(m_handle, INVALID_HANDLE_VALUE);
}

QString formatSystemError(DWORD errorCode)
{
    wchar_t* buffer = nullptr;

    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        0,
        reinterpret_cast<wchar_t*>(&buffer),
        0,
        nullptr);

    if (length == 0 || !buffer)
    {
        return QStringLiteral("Windows error %1").arg(errorCode);
    }

    QString message = QString::fromWCharArray(buffer, static_cast<qsizetype>(length)).trimmed();
    LocalFree(buffer);

    if (message.isEmpty())
    {
        return QStringLiteral("Windows error %1").arg(errorCode);
    }

    return QStringLiteral("%1 (0x%2)").arg(message).arg(errorCode, 0, 16);
}

QString formatLastSystemError()
{
    return formatSystemError(GetLastError());
}

std::wstring toWideString(const QString& text)
{
    std::wstring wide;
    wide.resize(static_cast<std::size_t>(text.size()));

    const int written = text.toWCharArray(wide.data());
    wide.resize(static_cast<std::size_t>(written));

    return wide;
}

std::wstring toWideString(const QByteArray& text)
{
    return toWideString(QString::fromUtf8(text));
}

QString readDescriptorString(const std::uint8_t* descriptor, DWORD fieldOffset, DWORD descriptorSize)
{
    if (fieldOffset == 0 || fieldOffset >= descriptorSize)
    {
        return QString{};
    }

    const char* const field = reinterpret_cast<const char*>(descriptor + fieldOffset);
    const char* const limit = reinterpret_cast<const char*>(descriptor + descriptorSize);
    const char* const terminator = std::find(field, limit, '\0');

    return QString::fromLatin1(field, static_cast<qsizetype>(terminator - field)).trimmed();
}
} // namespace UDI::Windows
