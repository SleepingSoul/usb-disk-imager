import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    property string title: ""
    property string subtitle: ""
    property string iconPath: ""

    default property alias content: contentColumn.data

    color: Style.surface
    border.color: Style.border
    border.width: 1
    radius: Style.radiusLarge
    implicitHeight: layout.implicitHeight + 2 * Style.spacingLarge

    ColumnLayout {
        id: layout

        anchors.fill: parent
        anchors.margins: Style.spacingLarge
        spacing: Style.spacingMedium

        RowLayout {
            Layout.fillWidth: true
            visible: root.title !== ""
            spacing: Style.spacingSmall

            AppIcon {
                visible: root.iconPath !== ""
                path: root.iconPath
                color: Style.accent
                size: 18
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: 2
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    text: root.title
                    color: Style.text
                    font.pixelSize: Style.fontSizeMedium
                    font.bold: true
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.subtitle !== ""
                    text: root.subtitle
                    color: Style.textMuted
                    font.pixelSize: Style.fontSizeSmall
                    wrapMode: Text.WordWrap
                }
            }
        }

        ColumnLayout {
            id: contentColumn

            Layout.fillWidth: true
            spacing: Style.spacingMedium
        }
    }
}
