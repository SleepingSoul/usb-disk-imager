import QtQuick
import QtQuick.Layouts
import usbdiskimager.qml

Rectangle {
    id: root

    implicitWidth: layout.implicitWidth + 2 * padding
    implicitHeight: 32
    radius: Style.radiusSmall
    color: Style.surfaceSunken
    border.width: 1
    border.color: Style.border

    readonly property int padding: 3

    RowLayout {
        id: layout

        anchors.centerIn: parent
        spacing: 2

        Repeater {
            model: Localization.availableLanguages

            Rectangle {
                id: languageOption

                required property var modelData

                readonly property bool current: modelData.code === Localization.currentLanguage

                implicitWidth: label.implicitWidth + 2 * Style.spacingMedium
                implicitHeight: 26
                radius: Style.radiusSmall - 1
                color: current ? Style.accent : (optionHover.hovered ? Style.surfaceElevated : "transparent")

                Behavior on color {
                    ColorAnimation { duration: Style.animationFast }
                }

                HoverHandler {
                    id: optionHover
                }

                TapHandler {
                    onTapped: Localization.selectLanguage(languageOption.modelData.code)
                }

                Text {
                    id: label

                    anchors.centerIn: parent
                    text: languageOption.modelData.name
                    color: languageOption.current ? Style.textOnAccent : Style.textMuted
                    font.pixelSize: Style.fontSizeSmall
                    font.bold: languageOption.current
                }
            }
        }
    }
}
