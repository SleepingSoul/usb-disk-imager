#pragma once

#include <string>

#include <QByteArray>
#include <QString>

#include <windows.h>


namespace UDI::Windows
{
// A Win32 handle that closes itself, so the early returns all over the device code cannot leak one.
class ScopedHandle
{
    Q_DISABLE_COPY(ScopedHandle)
public:
    ScopedHandle() = default;
    explicit ScopedHandle(HANDLE handle);
    ScopedHandle(ScopedHandle&& other) noexcept;
    ScopedHandle& operator=(ScopedHandle&& other) noexcept;
    ~ScopedHandle();

    bool isValid() const;
    HANDLE get() const { return m_handle; }
    void reset();
    HANDLE release();

private:
    HANDLE m_handle{ INVALID_HANDLE_VALUE };
};

QString formatSystemError(DWORD errorCode);

// Uses the last error of the calling thread, so call it before anything else that can fail.
QString formatLastSystemError();

std::wstring toWideString(const QString& text);
std::wstring toWideString(const QByteArray& text);

// Reads a NUL-terminated ASCII field that a STORAGE_DEVICE_DESCRIPTOR points at by byte offset.
QString readDescriptorString(const std::uint8_t* descriptor, DWORD fieldOffset, DWORD descriptorSize);
} // namespace UDI::Windows
