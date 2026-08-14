import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    property string label: ""
    property string value: ""
    property bool monospace: false

    color: Style.surfaceSunken
    radius: Style.radiusMedium
    border.width: 1
    border.color: Style.border
    implicitHeight: 60
    implicitWidth: 120

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Style.spacingSmall + 2
        spacing: 2

        Text {
            text: root.label
            color: Style.textMuted
            font.pixelSize: Style.fontSizeTiny
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 0.6
        }

        Text {
            Layout.fillWidth: true
            text: root.value
            color: Style.text
            font.pixelSize: Style.fontSizeMedium
            font.family: root.monospace ? Style.monospaceFamily : font.family
            elide: Text.ElideRight
        }
    }
}
