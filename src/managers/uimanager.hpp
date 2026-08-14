#pragma once

#include <memory>
#include <vector>

#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQuickWindow>

#include <app/manager.hpp>
#include <configs/uimanagerconfig.hpp>


namespace UDI
{
class IViewModel;
class DeviceListModel;
class ImagingViewModel;

class UiManager : public Manager<UiManager, UiManagerConfig>
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(UiManager)
public:
    UiManager();

    void postInit() override;

    // Only valid once qmlLoaded has been emitted.
    QQuickWindow* getMainWindow();

Q_SIGNALS:
    void qmlLoaded();

private:
    void registerQmlTypes();
    void connectManagers();

    const char* m_moduleName{ "usbdiskimager.qml" };
    int m_versionMajor{ 1 };
    int m_versionMinor{ 0 };

    QPointer<QQuickWindow> m_mainWindow;

    std::vector<std::unique_ptr<IViewModel>> m_viewModels;

    // Owned by this manager as a QObject child, because the QML singleton registration keeps only a raw
    // pointer and the model has to outlive the engine's bindings.
    DeviceListModel* m_deviceListModel{ nullptr };

    // Owned by m_viewModels; kept here so the manager wiring does not depend on registration order.
    ImagingViewModel* m_imagingViewModel{ nullptr };

    // Declared last on purpose: members are destroyed in reverse order, and the QML objects the engine
    // owns keep bindings to the view models above. Destroying the engine first tears those objects down
    // while the singletons they read are still alive.
    QQmlApplicationEngine m_engine;
};
} // namespace UDI
