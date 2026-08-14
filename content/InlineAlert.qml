import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    // info | success | warning | error
    property string kind: "info"
    property string text: ""
    property string title: ""

    default property alias trailing: trailingRow.data

    readonly property color _accentColor: {
        if (kind === "success")
            return Style.accent;
        if (kind === "warning")
            return Style.warning;
        if (kind === "error")
            return Style.danger;
        return Style.info;
    }
    readonly property color _fillColor: {
        if (kind === "success")
            return Style.accentSoft;
        if (kind === "warning")
            return Style.warningSoft;
        if (kind === "error")
            return Style.dangerSoft;
        return Style.infoSoft;
    }
    readonly property string _iconPath: {
        if (kind === "success")
            return Icons.success;
        if (kind === "warning")
            return Icons.warning;
        if (kind === "error")
            return Icons.error;
        return Icons.info;
    }

    color: _fillColor
    radius: Style.radiusMedium
    border.width: 1
    border.color: Qt.rgba(_accentColor.r, _accentColor.g, _accentColor.b, 0.35)
    implicitHeight: layout.implicitHeight + 2 * Style.spacingMedium

    RowLayout {
        id: layout

        anchors.fill: parent
        anchors.margins: Style.spacingMedium
        spacing: Style.spacingMedium

        AppIcon {
            path: root._iconPath
            color: root._accentColor
            size: 20
            Layout.alignment: Qt.AlignTop
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                Layout.fillWidth: true
                visible: root.title !== ""
                text: root.title
                color: Style.text
                font.pixelSize: Style.fontSizeNormal
                font.bold: true
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                visible: root.text !== ""
                text: root.text
                color: Style.textMuted
                font.pixelSize: Style.fontSizeSmall
                wrapMode: Text.WordWrap
                textFormat: Text.PlainText
            }
        }

        RowLayout {
            id: trailingRow

            Layout.alignment: Qt.AlignVCenter
            spacing: Style.spacingSmall
        }
    }
}
