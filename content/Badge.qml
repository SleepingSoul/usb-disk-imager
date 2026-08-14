import QtQuick

Rectangle {
    id: root

    property alias text: label.text
    property color textColor: Style.textMuted
    property color fillColor: Style.surfaceElevated

    implicitWidth: label.implicitWidth + 2 * Style.spacingSmall
    implicitHeight: 21
    radius: Style.radiusSmall
    color: fillColor

    Text {
        id: label

        anchors.centerIn: parent
        color: root.textColor
        font.pixelSize: Style.fontSizeTiny
        font.bold: true
    }
}
