import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import usbdiskimager.qml

ApplicationWindow {
    id: root

    property int currentPage: 0

    width: App.defaultWindowWidth
    height: App.defaultWindowHeight
    minimumWidth: App.minimumWindowWidth
    minimumHeight: App.minimumWindowHeight
    visible: true
    title: App.applicationName
    color: Style.background

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Header {
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.fillHeight: true
                implicitWidth: Style.navRailWidth
                color: Style.surface

                Rectangle {
                    anchors.right: parent.right
                    width: 1
                    height: parent.height
                    color: Style.border
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Style.spacingSmall
                    spacing: Style.spacingTiny

                    NavItem {
                        Layout.fillWidth: true
                        title: qsTr("Write")
                        iconPath: Icons.write
                        current: root.currentPage === 0
                        enabled: !Imager.busy
                        onActivated: root.currentPage = 0
                    }

                    NavItem {
                        Layout.fillWidth: true
                        title: qsTr("Read")
                        iconPath: Icons.read
                        current: root.currentPage === 1
                        enabled: !Imager.busy
                        onActivated: root.currentPage = 1
                    }

                    NavItem {
                        Layout.fillWidth: true
                        title: qsTr("Verify")
                        iconPath: Icons.verify
                        current: root.currentPage === 2
                        enabled: !Imager.busy
                        onActivated: root.currentPage = 2
                    }

                    NavItem {
                        Layout.fillWidth: true
                        title: qsTr("About")
                        iconPath: Icons.info
                        current: root.currentPage === 3
                        enabled: !Imager.busy
                        onActivated: root.currentPage = 3
                    }

                    Item {
                        Layout.fillHeight: true
                    }

                    Text {
                        Layout.fillWidth: true
                        Layout.margins: Style.spacingSmall
                        visible: Imager.busy
                        text: qsTr("An operation is running — the other pages are locked until it finishes.")
                        color: Style.textDisabled
                        font.pixelSize: Style.fontSizeTiny
                        wrapMode: Text.WordWrap
                    }
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: Style.spacingLarge
                currentIndex: root.currentPage

                WritePage {}

                ReadPage {}

                VerifyPage {}

                AboutPage {}
            }
        }
    }

    // One instance for the whole window rather than one per page: it is modal, so which page is behind
    // it no longer decides whether progress is on screen.
    ProgressDialog {}
}
