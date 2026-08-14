import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    property string title: ""
    property string iconPath: ""
    property bool current: false
    property bool enabled: true

    signal activated()

    implicitHeight: Style.navItemHeight
    radius: Style.radiusMedium
    color: current
        ? Style.accentSoft
        : (navHover.hovered && enabled ? Style.surfaceElevated : "transparent")
    opacity: enabled ? 1.0 : 0.45

    Behavior on color {
        ColorAnimation { duration: Style.animationFast }
    }

    HoverHandler {
        id: navHover
        enabled: root.enabled
    }

    TapHandler {
        enabled: root.enabled
        onTapped: root.activated()
    }

    Rectangle {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: 3
        height: 22
        radius: 2
        color: Style.accent
        visible: root.current
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Style.spacingMedium
        anchors.rightMargin: Style.spacingMedium
        spacing: Style.spacingMedium

        AppIcon {
            path: root.iconPath
            color: root.current ? Style.accent : Style.textMuted
            size: 19
        }

        Text {
            Layout.fillWidth: true
            text: root.title
            color: root.current ? Style.text : Style.textMuted
            font.pixelSize: Style.fontSizeNormal
            font.bold: root.current
            elide: Text.ElideRight
        }
    }
}
