import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import usbdiskimager.qml

Dialog {
    id: control

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    closePolicy: Popup.CloseOnEscape
    implicitWidth: 520
    padding: Style.spacingLarge

    Overlay.modal: Rectangle {
        color: "#B0000000"
    }

    background: Rectangle {
        color: Style.surface
        radius: Style.radiusLarge
        border.width: 1
        border.color: Style.borderStrong
    }

    contentItem: ColumnLayout {
        spacing: Style.spacingMedium

        RowLayout {
            Layout.fillWidth: true
            spacing: Style.spacingMedium

            AppIcon {
                path: Icons.warning
                color: Style.danger
                size: 26
                Layout.alignment: Qt.AlignTop
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Style.spacingTiny

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Erase %1?").arg(Devices.selectedName)
                    color: Style.text
                    font.pixelSize: Style.fontSizeLarge
                    font.bold: true
                    wrapMode: Text.WordWrap
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Everything currently on the device is overwritten and cannot be recovered.")
                    color: Style.textMuted
                    font.pixelSize: Style.fontSizeSmall
                    wrapMode: Text.WordWrap
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            color: Style.surfaceSunken
            radius: Style.radiusMedium
            border.width: 1
            border.color: Style.border
            implicitHeight: summary.implicitHeight + 2 * Style.spacingMedium

            GridLayout {
                id: summary

                anchors.fill: parent
                anchors.margins: Style.spacingMedium
                columns: 2
                columnSpacing: Style.spacingMedium
                rowSpacing: Style.spacingSmall

                Text {
                    text: qsTr("Device")
                    color: Style.textMuted
                    font.pixelSize: Style.fontSizeSmall
                }

                Text {
                    Layout.fillWidth: true
                    text: Devices.selectedName + " (" + Devices.selectedPath + ")"
                    color: Style.text
                    font.pixelSize: Style.fontSizeSmall
                    elide: Text.ElideRight
                }

                Text {
                    text: qsTr("Capacity")
                    color: Style.textMuted
                    font.pixelSize: Style.fontSizeSmall
                }

                Text {
                    Layout.fillWidth: true
                    text: Devices.selectedSizeText
                    color: Style.text
                    font.pixelSize: Style.fontSizeSmall
                }

                Text {
                    visible: Devices.selectedMountPointsText !== ""
                    text: qsTr("Mounted as")
                    color: Style.textMuted
                    font.pixelSize: Style.fontSizeSmall
                }

                Text {
                    Layout.fillWidth: true
                    visible: Devices.selectedMountPointsText !== ""
                    text: Devices.selectedMountPointsText
                    color: Style.danger
                    font.pixelSize: Style.fontSizeSmall
                    elide: Text.ElideRight
                }

                Text {
                    text: qsTr("Image")
                    color: Style.textMuted
                    font.pixelSize: Style.fontSizeSmall
                }

                Text {
                    Layout.fillWidth: true
                    text: Imager.imageFileName + " · " + Imager.imageSizeText
                    color: Style.text
                    font.pixelSize: Style.fontSizeSmall
                    elide: Text.ElideRight
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Style.spacingSmall
            spacing: Style.spacingSmall

            Item { Layout.fillWidth: true }

            AppButton {
                variant: "secondary"
                text: qsTr("Cancel")
                onClicked: control.reject()
            }

            AppButton {
                variant: "danger"
                text: qsTr("Write now")
                iconPath: Icons.write
                onClicked: control.accept()
            }
        }
    }
}
