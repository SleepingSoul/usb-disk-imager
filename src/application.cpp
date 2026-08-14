#include <application.hpp>

#include <algorithm>

#include <QIcon>

#include <managers/devicemanager.hpp>
#include <managers/imagingmanager.hpp>
#include <managers/localizationmanager.hpp>
#include <managers/loggingmanager.hpp>
#include <managers/uimanager.hpp>
#include <utils/buildinfo.hpp>


namespace UDI
{
namespace
{
const QLatin1StringView WindowIconResource{ ":/content/assets/icons/logo.svg" };

// A thread with nothing left to do exits in microseconds; the generous bound only matters if an imaging
// run is still unwinding a device handle, which must never be cut short.
constexpr unsigned ThreadQuitTimeoutMs = 15000;
} // namespace

Application::Application(int argc, char** argv)
    : QGuiApplication(argc, argv)
{
    setApplicationName(BuildInfo::ApplicationName);
    setApplicationVersion(BuildInfo::Version);
    setOrganizationName(BuildInfo::OrganizationName);
    setOrganizationDomain(BuildInfo::OrganizationDomain);
    setDesktopFileName(BuildInfo::ApplicationId);
    setWindowIcon(QIcon{ WindowIconResource });

    // LoggingManager comes first so the Qt message handler is installed before anything else can log.
    m_managers.emplace_back(new LoggingManager());
    // Translations have to be in place before the QML engine evaluates its first qsTr().
    m_managers.emplace_back(new LocalizationManager());
    m_managers.emplace_back(new DeviceManager());
    m_managers.emplace_back(new ImagingManager());
    m_managers.emplace_back(new UiManager());

    connect(this, &Application::aboutToQuit, this, &Application::shutdown);

    {
        // Enumeration talks to whatever storage drivers are loaded and can block for seconds on an
        // unresponsive one, which would otherwise freeze the window.
        auto& deviceScanThread = m_threads.emplace_back(QObjectMakeUniquePtrDeleteLater<QThread>());
        deviceScanThread->setObjectName(QStringLiteral("device_scan_thread"));
        getManager<DeviceManager>().moveToThread(deviceScanThread.get());
    }

    {
        // A transfer holds its thread for the whole run — minutes for a full card — and is cancelled
        // through an atomic flag rather than through this thread's event loop.
        auto& imagingThread = m_threads.emplace_back(QObjectMakeUniquePtrDeleteLater<QThread>());
        imagingThread->setObjectName(QStringLiteral("imaging_thread"));
        getManager<ImagingManager>().moveToThread(imagingThread.get());
    }

    qDebug() << "Moved managers to their threads, starting postInit for all managers in thread"
             << QObject::thread()->objectName();

    for (auto* manager : m_managers)
    {
        manager->postInit();
    }

    for (auto& thread : m_threads)
    {
        thread->start();
    }
}

Application::~Application() = default;

Application& Application::getInstance()
{
    return static_cast<Application&>(*QGuiApplication::instance());
}

void Application::shutdown()
{
    std::for_each(m_managers.rbegin(), m_managers.rend(), [](IManager* manager)
    {
        manager->scheduleDeletion();
    });
    std::for_each(m_threads.begin(), m_threads.end(), [](auto& thread)
    {
        thread->quit();
    });

    // scheduleDeletion() only posts a deferred delete, so a manager is destroyed by its own thread after
    // quit() unwinds that thread's event loop, while this loop is already waiting. imaging_thread may
    // still be closing a device handle and flushing it, which is exactly the work that must not be
    // terminated halfway through.
    for (auto& thread : m_threads)
    {
        if (!thread->wait(ThreadQuitTimeoutMs))
        {
            qCritical() << "Waiting for too long for thread" << thread->objectName() << "to finish. Terminating";
            thread->terminate();
        }
    }

    // Clearing the vector is what deletes the threads: their holder posts a deleteLater() of its own, and
    // exec()'s cleanup flushes deferred deletes right after aboutToQuit. Deleting them here as well would
    // leave the holder calling deleteLater() on freed objects when Application itself is destroyed.
    m_threads.clear();
}
} // namespace UDI
