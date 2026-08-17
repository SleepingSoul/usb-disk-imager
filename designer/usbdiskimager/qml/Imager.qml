pragma Singleton
import QtQuick

// Design-time stand-in for UDI::ImagingViewModel. busy and hasResult are the two switches worth flipping:
// they reveal ProgressPanel.qml and ResultPanel.qml, on whichever page operation names.
QtObject {
    property string imageFilePath: "C:/Images/raspios-bookworm-armhf.img"
    readonly property string imageFileName: "raspios-bookworm-armhf.img"
    readonly property string imageSizeText: "2.61 GB"
    readonly property bool imageFileExists: true

    property string trimMode: "partitions"
    property bool verifyAfterWrite: true
    property bool computeSha256: false
    property bool skipTrailingZerosOnWrite: false
    readonly property bool filesystemResizeSupported: true
    readonly property string filesystemResizeUnsupportedReason: ""
    property bool shrinkFilesystemAfterRead: false
    property bool growFilesystemToFillDevice: false

    readonly property bool busy: false
    readonly property bool cancelling: false
    readonly property bool cancellable: true
    readonly property string operation: "write"

    readonly property real progressFraction: 0.42
    readonly property string progressPercentText: "42%"
    readonly property string stageText: "Writing the image to the device"
    readonly property string processedSizeText: "1.10 GB"
    readonly property string totalSizeText: "2.61 GB"
    readonly property string transferRateText: "24.8 MB/s"
    readonly property string remainingTimeText: "1 min 2 s"
    readonly property string elapsedTimeText: "45 s"

    readonly property bool hasResult: false
    readonly property bool resultSucceeded: true
    readonly property bool resultCancelled: false
    readonly property bool resultVerified: true
    readonly property string resultTitle: "Write finished"
    readonly property string resultMessage: "2.61 GB written and verified against the image."
    readonly property string resultFastDigest: "7f4a1c93e0b25d68"
    readonly property string resultSha256Digest: "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08"

    function startRead() {}

    function startWrite() {}

    function startVerify() {}

    function cancel() {}

    function clearResult() {}

    function suggestImageFileUrl() {
        return "file:///C:/Images/disk-2-20260814-101500.img";
    }

    function getImageDirectoryUrl() {
        return "file:///C:/Images";
    }

    function showImageInFileManager() {}
}
