#include <managers/uimanager.hpp>

#include <QQmlContext>
#include <QQuickStyle>

#include <application.hpp>
#include <managers/devicemanager.hpp>
#include <managers/imagingmanager.hpp>
#include <managers/localizationmanager.hpp>
#include <ui/appviewmodel.hpp>
#include <ui/devicelistmodel.hpp>
#include <ui/imagingviewmodel.hpp>
#include <ui/localizationviewmodel.hpp>


namespace UDI
{
namespace
{
const QUrl EntryPointUrl{ QStringLiteral("qrc:/content/App.qml") };
} // namespace

UiManager::UiManager()
    : Manager("UiManager")
{
    // Every control in content/ is drawn by this project, so the platform styles must not reinterpret
    // them; Basic is the one style that changes nothing.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    qmlRegisterModule(m_moduleName, m_versionMajor, m_versionMinor);
}

void UiManager::postInit()
{
    registerQmlTypes();
    connectManagers();

    connect(&m_engine, &QQmlApplicationEngine::objectCreated, this, [this](QObject* rootObject, const QUrl&)
    {
        m_mainWindow = qobject_cast<QQuickWindow*>(rootObject);

        Q_EMIT qmlLoaded();
    }, Qt::QueuedConnection);

    connect(&m_engine, &QQmlApplicationEngine::objectCreationFailed, this, [](const QUrl& url)
    {
        qCritical() << "QML loading failed for object:" << url;
        QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    connect(&m_engine, &QQmlApplicationEngine::quit, &Application::getInstance(), &Application::quit);

    m_engine.load(EntryPointUrl);

    for (const auto& viewModel : m_viewModels)
    {
        viewModel->resetToDefault();
    }
}

QQuickWindow* UiManager::getMainWindow()
{
    return m_mainWindow.data();
}

void UiManager::registerQmlTypes()
{
    m_deviceListModel = new DeviceListModel(this);
    qmlRegisterSingletonInstance<DeviceListModel>(m_moduleName, m_versionMajor, m_versionMinor,
        "Devices", m_deviceListModel);

    auto appViewModel = std::make_unique<AppViewModel>();
    qmlRegisterSingletonInstance<AppViewModel>(m_moduleName, m_versionMajor, m_versionMinor,
        "App", appViewModel.get());
    m_viewModels.push_back(std::move(appViewModel));

    auto localizationViewModel = std::make_unique<LocalizationViewModel>();
    qmlRegisterSingletonInstance<LocalizationViewModel>(m_moduleName, m_versionMajor, m_versionMinor,
        "Localization", localizationViewModel.get());
    m_viewModels.push_back(std::move(localizationViewModel));

    auto imagingViewModel = std::make_unique<ImagingViewModel>(m_deviceListModel);
    m_imagingViewModel = imagingViewModel.get();
    qmlRegisterSingletonInstance<ImagingViewModel>(m_moduleName, m_versionMajor, m_versionMinor,
        "Imager", imagingViewModel.get());
    m_viewModels.push_back(std::move(imagingViewModel));
}

void UiManager::connectManagers()
{
    // The view models live on the GUI thread while the two worker managers do not, so everything crossing
    // between them is queued.
    auto& deviceManager = getManager<DeviceManager>();
    connect(&deviceManager, &DeviceManager::devicesUpdated,
        m_deviceListModel, &DeviceListModel::onDevicesUpdated, Qt::QueuedConnection);

    auto& imagingManager = getManager<ImagingManager>();
    connect(m_imagingViewModel, &ImagingViewModel::imagingRequested,
        &imagingManager, &ImagingManager::start, Qt::QueuedConnection);
    connect(&imagingManager, &ImagingManager::operationStarted,
        m_imagingViewModel, &ImagingViewModel::onOperationStarted, Qt::QueuedConnection);
    connect(&imagingManager, &ImagingManager::progressUpdated,
        m_imagingViewModel, &ImagingViewModel::onProgressUpdated, Qt::QueuedConnection);
    connect(&imagingManager, &ImagingManager::operationFinished,
        m_imagingViewModel, &ImagingViewModel::onOperationFinished, Qt::QueuedConnection);

    connect(&getManager<LocalizationManager>(), &LocalizationManager::languageChanged, this, [this]
    {
        // Re-evaluates every qsTr() binding in the loaded QML; the view models rebuild the strings they
        // translated themselves.
        m_engine.retranslate();

        for (const auto& viewModel : m_viewModels)
        {
            viewModel->onLanguageChanged();
        }
    });
}
} // namespace UDI
