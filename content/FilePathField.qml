import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root

    property string path: ""
    property string placeholderText: ""
    property string browseText: qsTr("Browse…")
    property bool enabled: true

    signal browseRequested()
    signal pathEdited(string editedPath)

    spacing: Style.spacingSmall

    TextField {
        id: field

        Layout.fillWidth: true
        implicitHeight: Style.controlHeight
        enabled: root.enabled
        text: root.path
        placeholderText: root.placeholderText
        placeholderTextColor: Style.textDisabled
        color: Style.text
        font.pixelSize: Style.fontSizeNormal
        selectByMouse: true
        leftPadding: Style.spacingMedium
        rightPadding: Style.spacingMedium

        background: Rectangle {
            radius: Style.radiusMedium
            color: field.enabled ? Style.surfaceSunken : Style.surface
            border.width: 1
            border.color: field.activeFocus ? Style.accent : (field.hovered ? Style.borderStrong : Style.border)

            Behavior on border.color {
                ColorAnimation { duration: Style.animationFast }
            }
        }

        onEditingFinished: root.pathEdited(text)
    }

    AppButton {
        variant: "secondary"
        text: root.browseText
        iconPath: Icons.folder
        enabled: root.enabled
        minimumWidth: 132
        onClicked: root.browseRequested()
    }
}
