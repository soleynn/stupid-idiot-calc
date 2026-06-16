import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// one calculator key. its `role` (set per key in Main.qml) picks the colours:
// digits sit quiet on a surface, operators carry the mauve accent, = is the one
// filled key, C and ⌫ glow red/peach. background + label are drawn by hand so
// the keys look identical on mac/linux/windows - the app pins the Basic controls
// style for the same reason. fills its grid cell, reads big enough to tap.
Button {
    id: key

    // digit | function | operator | clear | backspace | equals
    property string role: "digit"
    readonly property bool filled: role === "equals"

    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.minimumHeight: 56

    font.pixelSize: 24
    font.bold: role === "operator" || filled

    readonly property color faceColor: {
        switch (role) {
        case "equals":   return Theme.mauve
        case "operator": return Theme.surface1
        default:         return Theme.surface0
        }
    }
    readonly property color glyphColor: {
        switch (role) {
        case "equals":    return Theme.crust
        case "operator":  return Theme.mauve
        case "clear":     return Theme.red
        case "backspace": return Theme.peach
        case "function":  return Theme.subtext0
        default:          return Theme.text
        }
    }

    contentItem: Text {
        text: key.text
        font: key.font
        color: key.glyphColor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: 14
        color: key.down
               ? (key.filled ? Qt.darker(key.faceColor, 1.12) : Qt.lighter(key.faceColor, 1.32))
               : key.hovered
                 ? (key.filled ? Theme.lavender : Qt.lighter(key.faceColor, 1.15))
                 : key.faceColor
        // a lavender ring when tabbed to, so the keypad is keyboard-navigable.
        border.width: key.activeFocus ? 2 : 0
        border.color: Theme.lavender

        Behavior on color { ColorAnimation { duration: 90 } }
    }

    scale: down ? 0.96 : 1.0
    Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutQuad } }
}
