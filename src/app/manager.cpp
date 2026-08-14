#include <app/manager.hpp>

#include <mutex>
#include <unordered_map>


namespace UDI
{
namespace
{
std::unordered_map<std::type_index, IManager*>& managerRegistry()
{
    static std::unordered_map<std::type_index, IManager*> registry;

    return registry;
}

// Registration happens on the main thread, but deregistration does not: a manager living on a worker
// thread is destroyed by that thread as it unwinds during shutdown, and two of those unwinding at once
// would otherwise mutate the map concurrently.
std::mutex& managerRegistryMutex()
{
    static std::mutex mutex;

    return mutex;
}
} // namespace

IManager::IManager(const char* name)
    : m_name(QLatin1StringView{ name })
{
    setObjectName(m_name);
}

void IManager::scheduleDeletion()
{
    deleteLater();
}

namespace detail
{
void registerManagerInstance(std::type_index type, IManager* manager)
{
    const std::lock_guard<std::mutex> lock{ managerRegistryMutex() };

    const auto [iterator, inserted] = managerRegistry().try_emplace(type, manager);

    if (!inserted)
    {
        qFatal("Manager %s was constructed twice", type.name());
    }
}

void unregisterManagerInstance(std::type_index type)
{
    const std::lock_guard<std::mutex> lock{ managerRegistryMutex() };

    managerRegistry().erase(type);
}

IManager* findManagerInstance(std::type_index type)
{
    const std::lock_guard<std::mutex> lock{ managerRegistryMutex() };

    const auto iterator = managerRegistry().find(type);

    return iterator != managerRegistry().cend() ? iterator->second : nullptr;
}
} // namespace detail
} // namespace UDI
