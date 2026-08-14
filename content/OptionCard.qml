import QtQuick
import QtQuick.Layouts

// One choice in a group of mutually exclusive options, with room to explain what it does — the trim modes
// need that explanation far more than they need to be compact.
Rectangle {
    id: root

    property string title: ""
    property string description: ""
    property string value: ""
    property bool selected: false
    property bool enabled: true

    signal picked(string value)

    color: selected ? Style.accentSoft : (hoverArea.hovered ? Style.surfaceElevated : Style.surfaceSunken)
    radius: Style.radiusMedium
    border.width: 1
    border.color: selected ? Style.accent : Style.border
    implicitHeight: layout.implicitHeight + 2 * Style.spacingMedium
    opacity: enabled ? 1.0 : 0.5

    Behavior on color {
        ColorAnimation { duration: Style.animationFast }
    }

    Behavior on border.color {
        ColorAnimation { duration: Style.animationFast }
    }

    HoverHandler {
        id: hoverArea
        enabled: root.enabled
    }

    TapHandler {
        enabled: root.enabled
        onTapped: root.picked(root.value)
    }

    RowLayout {
        id: layout

        anchors.fill: parent
        anchors.margins: Style.spacingMedium
        spacing: Style.spacingMedium

        Rectangle {
            Layout.alignment: Qt.AlignTop
            Layout.topMargin: 2
            implicitWidth: 18
            implicitHeight: 18
            radius: 9
            color: "transparent"
            border.width: root.selected ? 5 : 1
            border.color: root.selected ? Style.accent : Style.borderStrong

            Behavior on border.width {
                NumberAnimation { duration: Style.animationFast }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3

            Text {
                Layout.fillWidth: true
                text: root.title
                color: Style.text
                font.pixelSize: Style.fontSizeNormal
                font.bold: true
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                visible: root.description !== ""
                text: root.description
                color: Style.textMuted
                font.pixelSize: Style.fontSizeSmall
                wrapMode: Text.WordWrap
            }
        }
    }
}
