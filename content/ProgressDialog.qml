import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import usbdiskimager.qml

// The running transfer, over a dimmed window. A run already locks every other page, so dimming them
// says so plainly instead of leaving a page on screen that looks like it could still be used.
Popup {
    id: control

    readonly property string operationIcon: Imager.operation === "read"
        ? Icons.read
        : (Imager.operation === "verify" ? Icons.verify : Icons.write)

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    // Neither Escape nor a click outside dismisses this: a device is being written to for as long as it
    // is up, and hiding that while it continues is exactly what this dialog exists to prevent.
    closePolicy: Popup.NoAutoClose
    visible: Imager.busy
    implicitWidth: 560
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
                path: control.operationIcon
                color: Style.accent
                size: 26
                Layout.alignment: Qt.AlignTop
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    text: Imager.stageText
                    color: Style.text
                    font.pixelSize: Style.fontSizeLarge
                    font.bold: true
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: Devices.selectedName !== ""
                        ? qsTr("%1 · %2").arg(Devices.selectedName).arg(Imager.imageFileName)
                        : Imager.imageFileName
                    color: Style.textMuted
                    font.pixelSize: Style.fontSizeSmall
                    elide: Text.ElideRight
                }
            }

            Text {
                text: Imager.progressPercentText
                color: Style.text
                font.pixelSize: Style.fontSizeHuge
                font.family: Style.monospaceFamily
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 10
            radius: 5
            color: Style.surfaceSunken
            border.width: 1
            border.color: Style.border

            Rectangle {
                width: Math.max(parent.width * Imager.progressFraction, Imager.progressFraction > 0 ? 10 : 0)
                height: parent.height
                radius: parent.radius

                gradient: Gradient {
                    orientation: Gradient.Horizontal

                    GradientStop { position: 0.0; color: Style.accentPressed }
                    GradientStop { position: 1.0; color: Style.accentHovered }
                }

                Behavior on width {
                    NumberAnimation { duration: Style.animationNormal; easing.type: Easing.OutQuad }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: Imager.processedSizeText + " / " + Imager.totalSizeText
            color: Style.textMuted
            font.pixelSize: Style.fontSizeSmall
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.topMargin: Style.spacingTiny
            columns: 4
            columnSpacing: Style.spacingSmall
            rowSpacing: Style.spacingSmall

            StatTile {
                Layout.fillWidth: true
                label: qsTr("Speed")
                value: Imager.transferRateText
            }

            StatTile {
                Layout.fillWidth: true
                label: qsTr("Remaining")
                value: Imager.remainingTimeText
            }

            StatTile {
                Layout.fillWidth: true
                label: qsTr("Elapsed")
                value: Imager.elapsedTimeText
            }

            StatTile {
                Layout.fillWidth: true
                label: qsTr("Copied")
                value: Imager.processedSizeText
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Style.spacingSmall
            spacing: Style.spacingSmall

            Text {
                Layout.fillWidth: true
                visible: !Imager.cancellable
                text: qsTr("Resizing a filesystem cannot be interrupted without damaging it.")
                color: Style.textMuted
                font.pixelSize: Style.fontSizeSmall
                wrapMode: Text.WordWrap
            }

            Item {
                Layout.fillWidth: true
                visible: Imager.cancellable
            }

            AppButton {
                variant: "secondary"
                text: Imager.cancelling ? qsTr("Cancelling…") : qsTr("Cancel")
                iconPath: Icons.cancel
                enabled: Imager.cancellable && !Imager.cancelling
                onClicked: Imager.cancel()
            }
        }
    }
}
