import QtQuick
import QtQuick.Layouts
import usbdiskimager.qml

Rectangle {
    id: root

    implicitHeight: Style.headerHeight
    color: Style.surface

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Style.border
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Style.spacingLarge
        anchors.rightMargin: Style.spacingLarge
        spacing: Style.spacingMedium

        Image {
            source: "assets/icons/logo.svg"
            sourceSize.width: 34
            sourceSize.height: 34
            fillMode: Image.PreserveAspectFit
        }

        ColumnLayout {
            spacing: 0

            Text {
                text: App.applicationName
                color: Style.text
                font.pixelSize: Style.fontSizeMedium
                font.bold: true
            }

            Text {
                text: qsTr("Version %1").arg(App.version)
                color: Style.textMuted
                font.pixelSize: Style.fontSizeTiny
            }
        }

        Item { Layout.fillWidth: true }

        Badge {
            visible: App.deviceAccessAvailable
            text: App.elevated ? qsTr("ADMINISTRATOR") : qsTr("DEVICE ACCESS")
            textColor: Style.accent
            fillColor: Style.accentSoft
        }

        Badge {
            visible: !App.deviceAccessAvailable
            text: qsTr("LIMITED RIGHTS")
            textColor: Style.warning
            fillColor: Style.warningSoft
        }

        AppIcon {
            path: Icons.globe
            color: Style.textMuted
            size: 17
        }

        LanguageSwitch {}
    }
}
