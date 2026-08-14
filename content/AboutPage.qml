import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import usbdiskimager.qml

ScrollView {
    id: root

    clip: true
    contentWidth: availableWidth

    ColumnLayout {
        width: root.availableWidth
        spacing: Style.spacingMedium

        Card {
            Layout.fillWidth: true

            RowLayout {
                Layout.fillWidth: true
                spacing: Style.spacingLarge

                Image {
                    source: "assets/icons/logo.svg"
                    sourceSize.width: 64
                    sourceSize.height: 64
                    fillMode: Image.PreserveAspectFit
                    Layout.alignment: Qt.AlignTop
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Style.spacingTiny

                    Text {
                        text: App.applicationName
                        color: Style.text
                        font.pixelSize: Style.fontSizeHuge
                        font.bold: true
                    }

                    Text {
                        Layout.fillWidth: true
                        text: App.description
                        color: Style.textMuted
                        font.pixelSize: Style.fontSizeNormal
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.topMargin: Style.spacingSmall
                        spacing: Style.spacingSmall

                        Badge {
                            text: qsTr("VERSION %1").arg(App.version)
                            textColor: Style.accent
                            fillColor: Style.accentSoft
                        }

                        Badge {
                            text: App.licenseName
                        }

                        Badge {
                            text: qsTr("QT %1").arg(App.qtVersion)
                        }
                    }
                }
            }
        }

        Card {
            Layout.fillWidth: true
            title: qsTr("Developer")
            iconPath: Icons.info

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: Style.spacingLarge
                rowSpacing: Style.spacingSmall

                Text {
                    text: qsTr("Author")
                    color: Style.textMuted
                    font.pixelSize: Style.fontSizeSmall
                }

                Text {
                    Layout.fillWidth: true
                    text: App.authorName
                    color: Style.text
                    font.pixelSize: Style.fontSizeNormal
                }

                Text {
                    text: qsTr("Contact")
                    color: Style.textMuted
                    font.pixelSize: Style.fontSizeSmall
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Style.spacingSmall

                    AppIcon {
                        path: Icons.mail
                        color: Style.textMuted
                        size: 15
                    }

                    Text {
                        text: App.authorEmail
                        color: Style.accent
                        font.pixelSize: Style.fontSizeNormal

                        HoverHandler {
                            cursorShape: Qt.PointingHandCursor
                        }

                        TapHandler {
                            onTapped: App.openUrl("mailto:" + App.authorEmail)
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                Text {
                    text: qsTr("Copyright")
                    color: Style.textMuted
                    font.pixelSize: Style.fontSizeSmall
                }

                Text {
                    Layout.fillWidth: true
                    text: App.copyright
                    color: Style.text
                    font.pixelSize: Style.fontSizeNormal
                }

                Text {
                    text: qsTr("Source code")
                    color: Style.textMuted
                    font.pixelSize: Style.fontSizeSmall
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Style.spacingSmall

                    Text {
                        text: App.homepage
                        color: Style.accent
                        font.pixelSize: Style.fontSizeNormal
                        elide: Text.ElideRight

                        HoverHandler {
                            cursorShape: Qt.PointingHandCursor
                        }

                        TapHandler {
                            onTapped: App.openUrl(App.homepage)
                        }
                    }

                    AppIcon {
                        path: Icons.external
                        color: Style.accent
                        size: 14
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        Card {
            Layout.fillWidth: true
            title: qsTr("Third-party software")
            subtitle: qsTr("This app is open source and stands on the shoulders of these projects.")
            iconPath: Icons.shield

            Repeater {
                model: App.thirdPartyComponents

                Rectangle {
                    id: componentRow

                    required property var modelData

                    Layout.fillWidth: true
                    color: Style.surfaceSunken
                    radius: Style.radiusMedium
                    border.width: 1
                    border.color: Style.border
                    implicitHeight: componentLayout.implicitHeight + 2 * Style.spacingMedium

                    ColumnLayout {
                        id: componentLayout

                        anchors.fill: parent
                        anchors.margins: Style.spacingMedium
                        spacing: Style.spacingTiny

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Style.spacingSmall

                            Text {
                                text: componentRow.modelData.name
                                color: Style.text
                                font.pixelSize: Style.fontSizeNormal
                                font.bold: true
                            }

                            Text {
                                text: componentRow.modelData.version
                                color: Style.textMuted
                                font.pixelSize: Style.fontSizeSmall
                                font.family: Style.monospaceFamily
                            }

                            Item { Layout.fillWidth: true }

                            Badge {
                                text: componentRow.modelData.license
                            }

                            IconButton {
                                iconPath: Icons.external
                                iconSize: 15
                                implicitWidth: 26
                                implicitHeight: 26
                                onClicked: App.openUrl(componentRow.modelData.homepage)
                                ToolTip.text: componentRow.modelData.homepage
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: componentRow.modelData.purpose
                            color: Style.textMuted
                            font.pixelSize: Style.fontSizeSmall
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }

        Card {
            Layout.fillWidth: true
            title: qsTr("Diagnostics")
            iconPath: Icons.folder

            Text {
                Layout.fillWidth: true
                text: qsTr("Every operation is logged, which is the first place to look when a card refuses to be written.")
                color: Style.textMuted
                font.pixelSize: Style.fontSizeSmall
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                text: App.logsDirectoryPath
                color: Style.text
                font.pixelSize: Style.fontSizeSmall
                font.family: Style.monospaceFamily
                elide: Text.ElideMiddle
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Style.spacingSmall

                AppButton {
                    variant: "secondary"
                    text: qsTr("Open log folder")
                    iconPath: Icons.folder
                    onClicked: App.openLogsDirectory()
                }

                AppButton {
                    variant: "ghost"
                    text: qsTr("Copy path")
                    iconPath: Icons.copy
                    onClicked: App.copyToClipboard(App.logsDirectoryPath)
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: App.platformName
                    color: Style.textDisabled
                    font.pixelSize: Style.fontSizeSmall
                }
            }
        }
    }
}
