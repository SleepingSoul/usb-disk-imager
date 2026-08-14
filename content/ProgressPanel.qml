import QtQuick
import QtQuick.Layouts
import usbdiskimager.qml

Card {
    id: root

    RowLayout {
        Layout.fillWidth: true
        spacing: Style.spacingMedium

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                text: Imager.stageText
                color: Style.accent
                font.pixelSize: Style.fontSizeMedium
                font.bold: true
            }

            Text {
                text: Imager.processedSizeText + " / " + Imager.totalSizeText
                color: Style.textMuted
                font.pixelSize: Style.fontSizeSmall
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

    GridLayout {
        Layout.fillWidth: true
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

        Item { Layout.fillWidth: true }

        AppButton {
            variant: "secondary"
            text: Imager.cancelling ? qsTr("Cancelling…") : qsTr("Cancel")
            iconPath: Icons.cancel
            enabled: !Imager.cancelling
            onClicked: Imager.cancel()
        }
    }
}
