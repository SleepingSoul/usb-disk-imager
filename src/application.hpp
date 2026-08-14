#pragma once

#include <memory>
#include <vector>

#include <QGuiApplication>
#include <QThread>

#include <app/commonutils.hpp>
#include <app/manager.hpp>


namespace UDI
{
class Application : public QGuiApplication
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Application)
public:
    Application(int argc, char** argv);
    ~Application() override;

    static Application& getInstance();

private Q_SLOTS:
    void shutdown();

private:
    std::vector<IManager*> m_managers;
    std::vector<QObjectUniquePtrDeleteLater<QThread>> m_threads;
};
} // namespace UDI
