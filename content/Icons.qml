pragma Singleton
import QtQuick

// Icons as SVG path data on a 24×24 grid, stroked by AppIcon. Keeping them as paths rather than as image
// files is what lets every icon take its colour from the theme and stay sharp at any scale.
QtObject {
    readonly property string write: "M12 3 L12 15 M7 10 L12 15 L17 10 M4 20 L20 20"
    readonly property string read: "M12 16 L12 4 M7 9 L12 4 L17 9 M4 20 L20 20"
    readonly property string verify: "M12 3 L20 6 V12 C20 17 16.5 19.8 12 21 C7.5 19.8 4 17 4 12 V6 Z M8.5 12 L11 14.5 L15.5 10"
    readonly property string info: "M2 12 A10 10 0 0 1 22 12 A10 10 0 0 1 2 12 M12 7.5 L12 7.6 M12 11 L12 16.5"
    readonly property string refresh: "M20.5 12 A8.5 8.5 0 1 1 17.5 5.4 M20.5 4 V9.2 H15.3"
    readonly property string folder: "M3 7 H9 L11 9.5 H21 V19 H3 Z"
    readonly property string drive: "M3 6.5 H21 V17.5 H3 Z M6.5 14.5 H10.5 M17.5 14.5 L17.5 14.6"
    readonly property string warning: "M12 3.5 L21.5 20 H2.5 Z M12 9.5 V14 M12 17 L12 17.1"
    readonly property string error: "M2 12 A10 10 0 0 1 22 12 A10 10 0 0 1 2 12 M9 9 L15 15 M15 9 L9 15"
    readonly property string success: "M2 12 A10 10 0 0 1 22 12 A10 10 0 0 1 2 12 M8 12 L11 15 L16 9"
    readonly property string cancel: "M6 6 L18 18 M18 6 L6 18"
    readonly property string chevronDown: "M6 9.5 L12 15.5 L18 9.5"
    readonly property string external: "M14 4 H20 V10 M20 4 L11.5 12.5 M18 14 V19 H5 V6 H10"
    readonly property string copy: "M9 9 H20 V20 H9 Z M5 15 H4 V4 H15 V5"
    readonly property string mail: "M3 6 H21 V18 H3 Z M3 7 L12 13.5 L21 7"
    readonly property string shield: "M12 3 L20 6 V12 C20 17 16.5 19.8 12 21 C7.5 19.8 4 17 4 12 V6 Z"
    readonly property string globe: "M2 12 A10 10 0 0 1 22 12 A10 10 0 0 1 2 12 M2.5 12 H21.5 M12 2 C15 5.5 15 18.5 12 22 C9 18.5 9 5.5 12 2"
    readonly property string trim: "M6 4 V14 A3 3 0 1 0 6 20 A3 3 0 1 0 6 14 M18 4 L8.5 17 M6 4 L15.5 17 M18 14 A3 3 0 1 0 18 20 A3 3 0 1 0 18 14"
}
