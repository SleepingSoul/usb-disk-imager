import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button {
    id: control

    // primary | secondary | danger | ghost
    property string variant: "primary"
    property string iconPath: ""
    // A floor rather than a fixed width: a translated label is often half again as long as the English
    // one, and a button that cannot grow clips it.
    property int minimumWidth: 104

    readonly property bool _accented: variant === "primary" || variant === "danger"
    readonly property color _baseColor: variant === "danger" ? Style.danger : Style.accent
    readonly property color _hoveredColor: variant === "danger" ? Style.dangerHovered : Style.accentHovered
    readonly property color _pressedColor: variant === "danger" ? Style.dangerPressed : Style.accentPressed
    readonly property color _labelColor: {
        if (!control.enabled)
            return Style.textDisabled;
        if (variant === "primary")
            return Style.textOnAccent;
        if (variant === "danger")
            return "#FFFFFF";
        if (variant === "ghost")
            return control.hovered ? Style.text : Style.textMuted;
        return Style.text;
    }

    implicitHeight: Style.controlHeight
    implicitWidth: Math.max(minimumWidth, contentRow.implicitWidth + 2 * Style.spacingLarge)
    hoverEnabled: true
    font.pixelSize: Style.fontSizeNormal

    background: Rectangle {
        radius: Style.radiusMedium
        color: {
            if (!control.enabled)
                return variant === "ghost" ? "transparent" : Style.surfaceElevated;
            if (control._accented)
                return control.pressed ? control._pressedColor
                    : (control.hovered ? control._hoveredColor : control._baseColor);
            if (control.pressed)
                return Style.surfaceSunken;
            return control.hovered ? Style.surfaceElevated : (variant === "ghost" ? "transparent" : Style.surface);
        }
        border.width: control._accented ? 0 : 1
        border.color: control.enabled
            ? (variant === "ghost" ? "transparent" : Style.borderStrong)
            : Style.border

        Behavior on color {
            ColorAnimation { duration: Style.animationFast }
        }

        // Keyboard focus has to be visible without a platform style drawing it for us.
        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: parent.radius + 3
            color: "transparent"
            border.width: 2
            border.color: Style.accent
            opacity: control.visualFocus ? 0.6 : 0.0
            visible: opacity > 0

            Behavior on opacity {
                NumberAnimation { duration: Style.animationFast }
            }
        }
    }

    contentItem: RowLayout {
        id: contentRow

        spacing: Style.spacingSmall

        Item { Layout.fillWidth: true }

        AppIcon {
            visible: control.iconPath !== ""
            path: control.iconPath
            color: control._labelColor
            size: 17
            strokeWidth: 1.9
        }

        Text {
            text: control.text
            color: control._labelColor
            font.pixelSize: control.font.pixelSize
            font.bold: control._accented
        }

        Item { Layout.fillWidth: true }
    }
}
