import QtQuick
import QtQuick.Shapes

// Strokes one of the Icons paths at an arbitrary size in an arbitrary colour.
Item {
    id: root

    property string path: ""
    property color color: Style.text
    property int size: 20
    property real strokeWidth: 1.7

    implicitWidth: size
    implicitHeight: size

    Shape {
        width: 24
        height: 24
        preferredRendererType: Shape.CurveRenderer

        transform: Scale {
            xScale: root.size / 24
            yScale: root.size / 24
        }

        ShapePath {
            strokeColor: root.color
            fillColor: "transparent"
            strokeWidth: root.strokeWidth
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            PathSvg { path: root.path }
        }
    }
}
