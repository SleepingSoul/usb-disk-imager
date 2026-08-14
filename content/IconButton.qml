import QtQuick
import QtQuick.Controls

Button {
    id: control

    property string iconPath: ""
    property int iconSize: 18
    property color iconColor: control.enabled
        ? (control.hovered ? Style.text : Style.textMuted)
        : Style.textDisabled

    implicitWidth: 36
    implicitHeight: 36
    hoverEnabled: true

    background: Rectangle {
        radius: Style.radiusSmall
        color: control.pressed
            ? Style.surfaceSunken
            : (control.hovered ? Style.surfaceElevated : "transparent")

        Behavior on color {
            ColorAnimation { duration: Style.animationFast }
        }
    }

    contentItem: Item {
        AppIcon {
            anchors.centerIn: parent
            path: control.iconPath
            color: control.iconColor
            size: control.iconSize
        }
    }

    ToolTip.visible: hovered && ToolTip.text !== ""
    ToolTip.delay: 500
}
