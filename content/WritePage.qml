import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import usbdiskimager.qml

ScrollView {
    id: root

    readonly property bool activeHere: Imager.operation === "write"
    readonly property bool canWrite: !Imager.busy
        && App.elevated
        && Devices.hasSelection
        && !Devices.selectedIsSystemDevice
        && !Devices.selectedIsWriteProtected
        && Imager.imageFileExists

    clip: true
    contentWidth: availableWidth

    ColumnLayout {
        width: root.availableWidth
        spacing: Style.spacingMedium

        ElevationAlert {
            Layout.fillWidth: true
        }

        Card {
            Layout.fillWidth: true
            title: qsTr("Image file")
            subtitle: qsTr("The file whose contents are copied onto the device, byte for byte.")
            iconPath: Icons.folder

            FilePathField {
                Layout.fillWidth: true
                path: Imager.imageFilePath
                enabled: !Imager.busy
                placeholderText: qsTr("Pick the image to write…")
                onBrowseRequested: openImageDialog.open()
                onPathEdited: editedPath => Imager.imageFilePath = editedPath
            }

            RowLayout {
                Layout.fillWidth: true
                visible: Imager.imageFilePath !== ""
                spacing: Style.spacingSmall

                AppIcon {
                    path: Imager.imageFileExists ? Icons.success : Icons.error
                    color: Imager.imageFileExists ? Style.accent : Style.danger
                    size: 15
                }

                Text {
                    Layout.fillWidth: true
                    text: Imager.imageFileExists
                        ? qsTr("%1 · %2").arg(Imager.imageFileName).arg(Imager.imageSizeText)
                        : qsTr("This file does not exist or is empty.")
                    color: Imager.imageFileExists ? Style.textMuted : Style.danger
                    font.pixelSize: Style.fontSizeSmall
                    elide: Text.ElideRight
                }
            }
        }

        Card {
            Layout.fillWidth: true
            title: qsTr("Target device")
            subtitle: qsTr("Everything on the selected device will be overwritten.")
            iconPath: Icons.drive

            DeviceSelector {
                Layout.fillWidth: true
            }

            InlineAlert {
                Layout.fillWidth: true
                visible: Devices.selectedIsSystemDevice
                kind: "error"
                title: qsTr("This is the system disk")
                text: qsTr("The disk holding the running operating system is never written to.")
            }

            InlineAlert {
                Layout.fillWidth: true
                visible: Devices.selectedIsWriteProtected
                kind: "error"
                title: qsTr("This device is write protected")
                text: qsTr("Release the write-protect switch on the card or adapter and rescan.")
            }
        }

        Card {
            Layout.fillWidth: true
            title: qsTr("How much to write")
            iconPath: Icons.trim

            AppCheckBox {
                Layout.fillWidth: true
                enabled: !Imager.busy
                text: qsTr("Skip trailing zeros in the image")
                description: qsTr("Stops once only zeros remain and leaves the rest of the device untouched — faster, but anything left over from the device's previous contents stays there.")
                checked: Imager.skipTrailingZerosOnWrite
                onToggled: Imager.skipTrailingZerosOnWrite = checked
            }

            AppCheckBox {
                Layout.fillWidth: true
                enabled: !Imager.busy && Imager.growSupported
                text: qsTr("Grow the filesystem to fill the device")
                description: Imager.growSupported
                    ? qsTr("Grows the last partition's ext2/3/4 filesystem before writing, so the card is fully usable right away instead of waiting for a first-boot resize.")
                    : Imager.growUnsupportedReason
                checked: Imager.growFilesystemToFillDevice
                onToggled: Imager.growFilesystemToFillDevice = checked
            }
        }

        Card {
            Layout.fillWidth: true
            title: qsTr("After writing")
            iconPath: Icons.verify

            AppCheckBox {
                Layout.fillWidth: true
                enabled: !Imager.busy
                text: qsTr("Verify the device against the image")
                description: qsTr("Reads the device back and compares every byte. Roughly doubles the time and is worth it for anything you intend to boot.")
                checked: Imager.verifyAfterWrite
                onToggled: Imager.verifyAfterWrite = checked
            }

            AppCheckBox {
                Layout.fillWidth: true
                enabled: !Imager.busy
                text: qsTr("Also compute a SHA-256 digest")
                description: qsTr("For comparing against a digest published alongside the image. Noticeably slower than the built-in XXH3 digest.")
                checked: Imager.computeSha256
                onToggled: Imager.computeSha256 = checked
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Item { Layout.fillWidth: true }

            AppButton {
                variant: "danger"
                text: qsTr("Write to device")
                iconPath: Icons.write
                minimumWidth: 200
                enabled: root.canWrite
                onClicked: confirmDialog.open()
            }
        }

        ResultPanel {
            Layout.fillWidth: true
            visible: Imager.hasResult && root.activeHere
        }
    }

    FileDialog {
        id: openImageDialog

        title: qsTr("Select a disk image")
        fileMode: FileDialog.OpenFile
        currentFolder: Imager.getImageDirectoryUrl()
        nameFilters: [qsTr("Disk images (*.img *.bin *.iso *.raw)"), qsTr("All files (*)")]
        onAccepted: Imager.imageFilePath = selectedFile
    }

    ConfirmWriteDialog {
        id: confirmDialog

        onAccepted: Imager.startWrite()
    }
}
