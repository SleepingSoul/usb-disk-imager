pragma Singleton
import QtQuick

// One dark-grey surface family with a single green accent. Every colour, radius, spacing step and font
// size in the app comes from here, so the whole UI can be retuned in one file.
QtObject {
    readonly property color background: "#1E2124"
    readonly property color surface: "#26292D"
    readonly property color surfaceElevated: "#2E3237"
    readonly property color surfaceSunken: "#191B1E"
    readonly property color border: "#383D42"
    readonly property color borderStrong: "#4B5157"

    readonly property color accent: "#41CD52"
    readonly property color accentHovered: "#55D965"
    readonly property color accentPressed: "#33A742"
    readonly property color accentSoft: "#1F3625"
    readonly property color textOnAccent: "#0E2411"

    readonly property color text: "#E9EBED"
    readonly property color textMuted: "#9CA4AB"
    readonly property color textDisabled: "#616870"

    readonly property color danger: "#E5534B"
    readonly property color dangerHovered: "#EE645C"
    readonly property color dangerPressed: "#C7443D"
    readonly property color dangerSoft: "#3A2120"

    readonly property color warning: "#E3A008"
    readonly property color warningSoft: "#3A2F13"

    readonly property color info: "#4A9EDA"
    readonly property color infoSoft: "#1C2C3A"

    readonly property int radiusSmall: 6
    readonly property int radiusMedium: 10
    readonly property int radiusLarge: 14
    readonly property int radiusPill: 999

    readonly property int spacingTiny: 4
    readonly property int spacingSmall: 8
    readonly property int spacingMedium: 16
    readonly property int spacingLarge: 24
    readonly property int spacingHuge: 32

    readonly property int fontSizeTiny: 11
    readonly property int fontSizeSmall: 12
    readonly property int fontSizeNormal: 13
    readonly property int fontSizeMedium: 15
    readonly property int fontSizeLarge: 19
    readonly property int fontSizeHuge: 26

    readonly property int controlHeight: 40
    readonly property int navItemHeight: 44
    readonly property int headerHeight: 64
    readonly property int navRailWidth: 208

    readonly property int animationFast: 120
    readonly property int animationNormal: 200

    readonly property string monospaceFamily: Qt.platform.os === "windows"
        ? "Consolas"
        : (Qt.platform.os === "osx" ? "Menlo" : "monospace")
}
