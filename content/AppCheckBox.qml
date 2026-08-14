import QtQuick
import QtQuick.Controls

CheckBox {
    id: control

    property string description: ""

    hoverEnabled: true
    padding: 0
    spacing: Style.spacingSmall
    font.pixelSize: Style.fontSizeNormal

    indicator: Rectangle {
        x: 0
        y: (control.height - height) / 2
        implicitWidth: 20
        implicitHeight: 20
        radius: Style.radiusSmall
        color: control.checked ? Style.accent : (control.hovered ? Style.surfaceElevated : Style.surfaceSunken)
        border.width: control.checked ? 0 : 1
        border.color: control.hovered ? Style.borderStrong : Style.border
        opacity: control.enabled ? 1.0 : 0.5

        Behavior on color {
            ColorAnimation { duration: Style.animationFast }
        }

        AppIcon {
            anchors.centerIn: parent
            path: "M5 12.5 L9.5 17 L19 7"
            color: Style.textOnAccent
            size: 16
            strokeWidth: 2.4
            opacity: control.checked ? 1.0 : 0.0

            Behavior on opacity {
                NumberAnimation { duration: Style.animationFast }
            }
        }
    }

    contentItem: Column {
        leftPadding: control.indicator.width + control.spacing
        spacing: 2

        Text {
            text: control.text
            color: control.enabled ? Style.text : Style.textDisabled
            font: control.font
        }

        Text {
            width: control.width - control.indicator.width - control.spacing
            visible: control.description !== ""
            text: control.description
            color: Style.textMuted
            font.pixelSize: Style.fontSizeSmall
            wrapMode: Text.WordWrap
        }
    }
}
