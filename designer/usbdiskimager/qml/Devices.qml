pragma Singleton
import QtQuick

// Design-time stand-in for UDI::DeviceListModel. The elements use the role names the C++ model exposes,
// so the ComboBox delegate in DeviceSelector.qml renders here exactly as it does at runtime. Point
// selectedIndex at the second row to see how the SYSTEM badge and the write refusals look.
ListModel {
    id: root

    property int selectedIndex: 0
    property bool includeFixedDisks: false

    readonly property bool hasSelection: root.selectedIndex >= 0 && root.selectedIndex < root.count
    readonly property string selectedName: root.hasSelection ? root.get(root.selectedIndex).name : ""
    readonly property string selectedDetails: root.hasSelection ? root.get(root.selectedIndex).details : ""
    readonly property string selectedPath: root.hasSelection ? root.get(root.selectedIndex).path : ""
    readonly property string selectedSizeText: root.hasSelection ? root.get(root.selectedIndex).sizeText : ""
    readonly property string selectedMountPointsText: root.hasSelection ? root.get(root.selectedIndex).mountPointsText : ""
    readonly property bool selectedIsSystemDevice: root.hasSelection && root.get(root.selectedIndex).systemDevice
    readonly property bool selectedIsWriteProtected: root.hasSelection && root.get(root.selectedIndex).writeProtected

    // Anything in here draws a warning alert under the device list.
    readonly property var warnings: []

    ListElement {
        name: "Generic STORAGE DEVICE"
        details: "16.0 GB · USB · D:"
        path: "\\\\.\\PhysicalDrive2"
        shortIdentifier: "Disk 2"
        sizeText: "16.0 GB"
        busName: "USB"
        mountPointsText: "D:"
        removable: true
        systemDevice: false
        writeProtected: false
    }

    ListElement {
        name: "Samsung SSD 990 PRO 2TB"
        details: "2.00 TB · NVMe · C:"
        path: "\\\\.\\PhysicalDrive0"
        shortIdentifier: "Disk 0"
        sizeText: "2.00 TB"
        busName: "NVMe"
        mountPointsText: "C:"
        removable: false
        systemDevice: true
        writeProtected: false
    }

    function refresh() {}
}
