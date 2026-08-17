import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import usbdiskimager.qml

ScrollView {
    id: root

    readonly property bool activeHere: Imager.operation === "read"
    readonly property bool canRead: !Imager.busy
        && App.elevated
        && Devices.hasSelection
        && Imager.imageFilePath !== ""

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
            title: qsTr("Source device")
            subtitle: qsTr("The device is only read from — nothing is written to it.")
            iconPath: Icons.drive

            DeviceSelector {
                Layout.fillWidth: true
            }
        }

        Card {
            Layout.fillWidth: true
            title: qsTr("Image file")
            subtitle: qsTr("Where the image is saved. An existing file is replaced only once the read completes.")
            iconPath: Icons.folder

            FilePathField {
                Layout.fillWidth: true
                path: Imager.imageFilePath
                enabled: !Imager.busy
                placeholderText: qsTr("Choose where to save the image…")
                onBrowseRequested: saveImageDialog.open()
                onPathEdited: editedPath => Imager.imageFilePath = editedPath
            }
        }

        Card {
            Layout.fillWidth: true
            title: qsTr("How much to read")
            subtitle: qsTr("A 32 GB card holding a 2 GB system writes a 32 GB image unless it is trimmed.")
            iconPath: Icons.trim

            OptionCard {
                Layout.fillWidth: true
                enabled: !Imager.busy
                value: "partitions"
                selected: Imager.trimMode === "partitions"
                title: qsTr("Stop after the last partition")
                description: qsTr("Reads the partition table and stops where the last partition ends. On a GPT device the backup header is rebuilt, so the shorter image stays valid on its own.")
                onPicked: value => Imager.trimMode = value
            }

            OptionCard {
                Layout.fillWidth: true
                enabled: !Imager.busy
                value: "zeros"
                selected: Imager.trimMode === "zeros"
                title: qsTr("Stop after the last non-empty sector")
                description: qsTr("Works without a partition table. Scans the device from the end backwards first, which takes a little longer.")
                onPicked: value => Imager.trimMode = value
            }

            OptionCard {
                Layout.fillWidth: true
                enabled: !Imager.busy
                value: "none"
                selected: Imager.trimMode === "none"
                title: qsTr("Read the whole device")
                description: qsTr("An exact copy of every sector, including the unused ones. The image is always as large as the device.")
                onPicked: value => Imager.trimMode = value
            }

            AppCheckBox {
                Layout.fillWidth: true
                enabled: !Imager.busy
                text: qsTr("Also compute a SHA-256 digest")
                description: qsTr("Useful when the image will be published alongside a digest. Noticeably slower than the built-in XXH3 digest.")
                checked: Imager.computeSha256
                onToggled: Imager.computeSha256 = checked
            }

            AppCheckBox {
                Layout.fillWidth: true
                enabled: !Imager.busy && Imager.filesystemResizeSupported
                text: qsTr("Shrink the filesystem afterwards")
                description: Imager.filesystemResizeSupported
                    ? qsTr("Shrinks the last partition's ext2/3/4 filesystem to the smallest it can be, and truncates the image to match. Useful for a 64 GB card that only needs 8 GB.")
                    : Imager.filesystemResizeUnsupportedReason
                checked: Imager.shrinkFilesystemAfterRead
                onToggled: Imager.shrinkFilesystemAfterRead = checked
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Item { Layout.fillWidth: true }

            AppButton {
                variant: "primary"
                text: qsTr("Read into image")
                iconPath: Icons.read
                minimumWidth: 200
                enabled: root.canRead
                onClicked: Imager.startRead()
            }
        }

        ProgressPanel {
            Layout.fillWidth: true
            visible: Imager.busy && root.activeHere
        }

        ResultPanel {
            Layout.fillWidth: true
            visible: Imager.hasResult && root.activeHere
            showRevealButton: true
        }
    }

    FileDialog {
        id: saveImageDialog

        title: qsTr("Save the image as")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "img"
        selectedFile: Imager.suggestImageFileUrl()
        nameFilters: [qsTr("Disk images (*.img)"), qsTr("All files (*)")]
        onAccepted: Imager.imageFilePath = selectedFile
    }
}
