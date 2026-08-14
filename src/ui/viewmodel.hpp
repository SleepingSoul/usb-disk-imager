#pragma once

#include <QObject>


namespace UDI
{
class IViewModel : public QObject
{
public:
    ~IViewModel() override = default;

    // Called once after the QML engine has loaded, so every property starts from a known value rather
    // than from whatever the constructor happened to leave behind.
    virtual void resetToDefault() = 0;

    // Any property holding text that was translated when it was built has to announce itself again after
    // the language changes; QML's own qsTr() bindings are handled by the engine's retranslate().
    virtual void onLanguageChanged() {}
};
} // namespace UDI
