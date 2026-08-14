import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import usbdiskimager.qml

RowLayout {
    id: root

    property string label: ""
    property string digest: ""

    spacing: Style.spacingSmall

    Text {
        text: root.label
        color: Style.textMuted
        font.pixelSize: Style.fontSizeSmall
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 0.6
    }

    Text {
        Layout.fillWidth: true
        text: root.digest
        color: Style.text
        font.pixelSize: Style.fontSizeSmall
        font.family: Style.monospaceFamily
        elide: Text.ElideRight
    }

    IconButton {
        iconPath: Icons.copy
        iconSize: 16
        implicitWidth: 28
        implicitHeight: 28
        onClicked: App.copyToClipboard(root.digest)
        ToolTip.text: qsTr("Copy to clipboard")
    }
}
