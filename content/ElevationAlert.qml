import QtQuick
import QtQuick.Layouts
import usbdiskimager.qml

// Devices can be listed without extra rights but not always opened, so the warning belongs on every page
// that starts a transfer rather than in a dialog at startup. It stays hidden where a broker such as
// udisks2 can open the device for us, since there is nothing for the user to do there.
InlineAlert {
    visible: !App.deviceAccessAvailable
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
