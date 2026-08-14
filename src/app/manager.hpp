#pragma once

#include <typeindex>
#include <typeinfo>

#include <QLatin1StringView>
#include <QObject>

#include <app/managerconfig.hpp>


namespace UDI
{
class IManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(IManager)
public:
    ~IManager() override = default;

    // Runs on the main thread once every manager exists and has been moved to its thread. A manager that
    // owns thread-affine resources queues its real initialisation onto its own event loop from here
    // rather than touching them directly. A manager with nothing thread-affine to set up leaves it be.
    virtual void postInit() {}

    void scheduleDeletion();

    QLatin1StringView getName() const { return m_name; }

protected:
    explicit IManager(const char* name);

private:
    const QLatin1StringView m_name;
};

namespace detail
{
void registerManagerInstance(std::type_index type, IManager* manager);
void unregisterManagerInstance(std::type_index type);
IManager* findManagerInstance(std::type_index type);
} // namespace detail

template <typename TManager, typename TConfig>
class Manager : public IManager
{
public:
    const TConfig& getConfig() const { return m_config; }

    static TManager& instance();

protected:
    explicit Manager(const char* name)
        : IManager(name)
    {
        detail::registerManagerInstance(std::type_index{ typeid(TManager) }, this);

        const auto configMap = loadManagerConfigFile(m_config.getFileName());
        if (!configMap)
        {
            qFatal("%s: configuration file %s could not be loaded",
                name, qUtf8Printable(m_config.getFileName()));
        }

        // A config field that fails to load would otherwise leave the manager running on whatever
        // happened to be in memory, so refuse to start instead.
        if (!m_config.parse(*configMap))
        {
            qFatal("%s: configuration file %s is missing fields or has wrong value types",
                name, qUtf8Printable(m_config.getFileName()));
        }
    }

    ~Manager() override
    {
        detail::unregisterManagerInstance(std::type_index{ typeid(TManager) });
    }

private:
    TConfig m_config;
};

template <typename TManager>
TManager& getManager()
{
    IManager* const manager = detail::findManagerInstance(std::type_index{ typeid(TManager) });

    if (!manager)
    {
        qFatal("getManager(): %s has not been constructed yet", typeid(TManager).name());
    }

    return *static_cast<TManager*>(manager);
}

template <typename TManager, typename TConfig>
TManager& Manager<TManager, TConfig>::instance()
{
    return getManager<TManager>();
}
} // namespace UDI
