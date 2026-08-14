import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import usbdiskimager.qml

ScrollView {
    id: root

    readonly property bool activeHere: Imager.operation === "verify"
    readonly property bool canVerify: !Imager.busy
        && App.elevated
        && Devices.hasSelection
        && Imager.imageFileExists

    clip: true
    contentWidth: availableWidth

    ColumnLayout {
        width: root.availableWidth
        spacing: Style.spacingMedium

        ElevationAlert {
            Layout.fillWidth: true
        }

        InlineAlert {
            Layout.fillWidth: true
            kind: "info"
            title: qsTr("Compares a device against an image")
            text: qsTr("Reads as many bytes from the device as the image is long and stops at the first difference. Nothing is written.")
        }

        Card {
            Layout.fillWidth: true
            title: qsTr("Device")
            iconPath: Icons.drive

            DeviceSelector {
                Layout.fillWidth: true
            }
        }

        Card {
            Layout.fillWidth: true
            title: qsTr("Image file")
            iconPath: Icons.folder

            FilePathField {
                Layout.fillWidth: true
                path: Imager.imageFilePath
                enabled: !Imager.busy
                placeholderText: qsTr("Pick the image to compare against…")
                onBrowseRequested: openImageDialog.open()
                onPathEdited: editedPath => Imager.imageFilePath = editedPath
            }

            Text {
                Layout.fillWidth: true
                visible: Imager.imageFileExists
                text: qsTr("%1 · %2").arg(Imager.imageFileName).arg(Imager.imageSizeText)
                color: Style.textMuted
                font.pixelSize: Style.fontSizeSmall
                elide: Text.ElideRight
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Item { Layout.fillWidth: true }

            AppButton {
                variant: "primary"
                text: qsTr("Compare")
                iconPath: Icons.verify
                minimumWidth: 200
                enabled: root.canVerify
                onClicked: Imager.startVerify()
            }
        }

        ProgressPanel {
            Layout.fillWidth: true
            visible: Imager.busy && root.activeHere
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
}
