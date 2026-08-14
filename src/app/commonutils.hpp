#pragma once

#include <memory>
#include <utility>

#include <QObject>


namespace UDI
{
template <typename TQObject>
struct QObjectDeleteLaterDeleter
{
    void operator()(TQObject* object) const
    {
        if (object)
        {
            object->deleteLater();
        }
    }
};

// Deleting a QObject that lives on another thread from this thread is a race; deleteLater() hands the
// destruction to the owning thread's event loop instead.
template <typename TQObject>
using QObjectUniquePtrDeleteLater = std::unique_ptr<TQObject, QObjectDeleteLaterDeleter<TQObject>>;

template <typename TQObject, typename... TArgs>
QObjectUniquePtrDeleteLater<TQObject> QObjectMakeUniquePtrDeleteLater(TArgs&&... args)
{
    return QObjectUniquePtrDeleteLater<TQObject>(new TQObject(std::forward<TArgs>(args)...));
}
} // namespace UDI
