import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// one calculator key. a plain Controls Button that fills its grid cell and
// reads a bit bigger so its comfortable to tap on a phone. the grid in Main.qml
// wires each one's text + onClicked.
Button {
    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.minimumHeight: 56
    font.pixelSize: 22
}
