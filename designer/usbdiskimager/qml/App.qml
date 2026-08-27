pragma Singleton
import QtQuick

// Design-time stand-in for UDI::AppViewModel, which UiManager registers into this module at startup.
// Same properties and methods, plausible values, no behaviour. Set deviceAccessAvailable to false to
// bring up the rights banner ElevationAlert.qml draws.
QtObject {
    readonly property string applicationName: "USB Disk Imager"
    readonly property string version: "1.0.0"
    readonly property string authorName: "Tihran Katolikian"
    readonly property string authorEmail: "tkatolikian@outlook.com"
    readonly property string homepage: "https://github.com/SleepingSoul/usb-disk-imager"
    readonly property string licenseName: "MIT"
    readonly property string qtVersion: "6.8.3"
    readonly property string platformName: "Windows 11 Pro x86_64"
    readonly property string logsDirectoryPath: "C:/Users/You/AppData/Local/Tihran Katolikian/USB Disk Imager/logs"

    readonly property string description: "Reads a USB drive or memory card into an image file and writes an image file back, byte for byte."
    readonly property string copyright: "© 2026 Tihran Katolikian"

    readonly property var thirdPartyComponents: [
        {
            "name": "Qt",
            "version": "6.8.3",
            "license": "LGPL v3",
            "homepage": "https://www.qt.io",
            "purpose": "Application framework, QML user interface and translations"
        },
        {
            "name": "xxHash",
            "version": "0.8.3",
            "license": "BSD 2-Clause",
            "homepage": "https://github.com/Cyan4973/xxHash",
            "purpose": "Fast integrity digests of images and devices"
        },
        {
            "name": "spdlog",
            "version": "1.15.1",
            "license": "MIT",
            "homepage": "https://github.com/gabime/spdlog",
            "purpose": "Rotating log files behind Qt's message handler"
        }
    ]

    readonly property bool elevated: true
    readonly property bool deviceAccessAvailable: true
    readonly property bool canElevate: true
    readonly property string elevationHint: "Restart the app as administrator, or start it from an elevated terminal."

    readonly property int defaultWindowWidth: 1060
    readonly property int defaultWindowHeight: 730
    readonly property int minimumWindowWidth: 900
    readonly property int minimumWindowHeight: 640

    function requestElevation() {
        return false;
    }

    function openUrl(url) {}

    function openLogsDirectory() {}

    function copyToClipboard(text) {}
}
