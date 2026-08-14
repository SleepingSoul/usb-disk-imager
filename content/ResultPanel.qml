import QtQuick
import QtQuick.Layouts
import usbdiskimager.qml

Card {
    id: root

    property bool showRevealButton: false

    InlineAlert {
        Layout.fillWidth: true
        kind: Imager.resultSucceeded ? "success" : (Imager.resultCancelled ? "warning" : "error")
        title: Imager.resultTitle
        text: Imager.resultMessage
    }

    DigestRow {
        Layout.fillWidth: true
        visible: Imager.resultFastDigest !== ""
        label: qsTr("XXH3")
        digest: Imager.resultFastDigest
    }

    DigestRow {
        Layout.fillWidth: true
        visible: Imager.resultSha256Digest !== ""
        label: qsTr("SHA-256")
        digest: Imager.resultSha256Digest
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Style.spacingSmall

        AppButton {
            variant: "secondary"
            visible: root.showRevealButton && Imager.resultSucceeded
            text: qsTr("Show in folder")
            iconPath: Icons.folder
            onClicked: Imager.showImageInFileManager()
        }

        Item { Layout.fillWidth: true }

        AppButton {
            variant: "ghost"
            text: qsTr("Dismiss")
            onClicked: Imager.clearResult()
        }
    }
}
