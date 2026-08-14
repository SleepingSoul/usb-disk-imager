import QtQuick
import QtQuick.Layouts
import usbdiskimager.qml

// Devices can be listed without extra rights but not opened, so the warning belongs on every page that
// starts a transfer rather than in a dialog at startup.
InlineAlert {
    visible: !App.elevated
    kind: "warning"
    title: qsTr("Administrator rights are needed to read or write a device")
    text: App.elevationHint

    AppButton {
        variant: "secondary"
        visible: App.canElevate
        text: qsTr("Restart with rights")
        iconPath: Icons.shield
        minimumWidth: 180
        onClicked: {
            if (App.requestElevation())
                Qt.quit();
        }
    }
}
