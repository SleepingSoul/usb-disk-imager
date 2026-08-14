import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import usbdiskimager.qml

ColumnLayout {
    id: root

    property bool showInternalDisksToggle: true

    spacing: Style.spacingMedium

    RowLayout {
        Layout.fillWidth: true
        spacing: Style.spacingSmall

        ComboBox {
            id: deviceCombo

            Layout.fillWidth: true
            implicitHeight: 58
            model: Devices
            enabled: !Imager.busy && Devices.count > 0
            currentIndex: Devices.selectedIndex
            onActivated: index => Devices.selectedIndex = index

            background: Rectangle {
                radius: Style.radiusMedium
                color: deviceCombo.enabled ? Style.surfaceSunken : Style.surface
                border.width: 1
                border.color: deviceCombo.activeFocus
                    ? Style.accent
                    : (deviceCombo.hovered ? Style.borderStrong : Style.border)

                Behavior on border.color {
                    ColorAnimation { duration: Style.animationFast }
                }
            }

            contentItem: RowLayout {
                spacing: Style.spacingMedium
                anchors.margins: Style.spacingMedium

                AppIcon {
                    Layout.leftMargin: Style.spacingMedium
                    path: Icons.drive
                    color: Devices.hasSelection ? Style.accent : Style.textDisabled
                    size: 22
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Text {
                        Layout.fillWidth: true
                        text: Devices.hasSelection ? Devices.selectedName : qsTr("No device selected")
                        color: Devices.hasSelection ? Style.text : Style.textDisabled
                        font.pixelSize: Style.fontSizeNormal
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: Devices.hasSelection
                        text: Devices.selectedDetails
                        color: Style.textMuted
                        font.pixelSize: Style.fontSizeSmall
                        elide: Text.ElideRight
                    }
                }

                Badge {
                    visible: Devices.selectedIsSystemDevice
                    text: qsTr("SYSTEM")
                    textColor: Style.danger
                    fillColor: Style.dangerSoft
                }

                Badge {
                    visible: Devices.selectedIsWriteProtected
                    text: qsTr("READ-ONLY")
                    textColor: Style.warning
                    fillColor: Style.warningSoft
                }
            }

            indicator: AppIcon {
                x: deviceCombo.width - width - Style.spacingMedium
                y: (deviceCombo.height - height) / 2
                path: Icons.chevronDown
                color: Style.textMuted
                size: 18
            }

            popup: Popup {
                y: deviceCombo.height + 4
                width: deviceCombo.width
                implicitHeight: Math.min(contentItem.implicitHeight + 2, 320)
                padding: 1

                background: Rectangle {
                    color: Style.surfaceElevated
                    radius: Style.radiusMedium
                    border.width: 1
                    border.color: Style.borderStrong
                }

                contentItem: ListView {
                    implicitHeight: contentHeight
                    model: deviceCombo.popup.visible ? deviceCombo.delegateModel : null
                    currentIndex: deviceCombo.highlightedIndex
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    ScrollIndicator.vertical: ScrollIndicator {}
                }
            }

            delegate: ItemDelegate {
                id: deviceDelegate

                required property int index
                required property string name
                required property string details
                required property string shortIdentifier
                required property bool systemDevice
                required property bool writeProtected

                width: deviceCombo.width
                height: 58
                highlighted: deviceCombo.highlightedIndex === index

                background: Rectangle {
                    color: deviceDelegate.highlighted ? Style.accentSoft : "transparent"
                }

                contentItem: RowLayout {
                    spacing: Style.spacingMedium

                    AppIcon {
                        path: Icons.drive
                        color: deviceDelegate.systemDevice ? Style.danger : Style.accent
                        size: 20
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1

                        Text {
                            Layout.fillWidth: true
                            text: deviceDelegate.name
                            color: Style.text
                            font.pixelSize: Style.fontSizeNormal
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            text: deviceDelegate.shortIdentifier + " · " + deviceDelegate.details
                            color: Style.textMuted
                            font.pixelSize: Style.fontSizeSmall
                            elide: Text.ElideRight
                        }
                    }

                    Badge {
                        visible: deviceDelegate.systemDevice
                        text: qsTr("SYSTEM")
                        textColor: Style.danger
                        fillColor: Style.dangerSoft
                    }

                    Badge {
                        visible: deviceDelegate.writeProtected
                        text: qsTr("READ-ONLY")
                        textColor: Style.warning
                        fillColor: Style.warningSoft
                    }
                }
            }
        }

        IconButton {
            iconPath: Icons.refresh
            enabled: !Imager.busy
            onClicked: Devices.refresh()
            ToolTip.text: qsTr("Rescan devices")
        }
    }

    InlineAlert {
        Layout.fillWidth: true
        visible: Devices.count === 0
        kind: "info"
        text: qsTr("No removable device found. Insert a USB drive or a memory card — the list refreshes by itself.")
    }

    AppCheckBox {
        Layout.fillWidth: true
        visible: root.showInternalDisksToggle
        enabled: !Imager.busy
        text: qsTr("Also list internal disks")
        description: qsTr("Internal disks are hidden by default. The disk holding the running system is never offered as a write target.")
        checked: Devices.includeFixedDisks
        onToggled: Devices.includeFixedDisks = checked
    }

    Repeater {
        model: Devices.warnings

        InlineAlert {
            required property string modelData

            Layout.fillWidth: true
            kind: "warning"
            text: modelData
        }
    }
}
