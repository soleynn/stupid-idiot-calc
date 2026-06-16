import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// the whole window: a recessed screen (history, the live expression, the result)
// over a grid of keys. Engine is the C++ bridge from this same qml module, so it
// needs no import; colours come from the Theme singleton, also in-module. one
// codebase here serves desktop and (later) android - thats why its Quick/QML.
ApplicationWindow {
    id: window
    visible: true
    width: 360
    height: 600
    minimumWidth: 300
    minimumHeight: 520
    title: qsTr("stupid idiot calc")
    color: Theme.base

    // the expression the user is building up, tap by tap.
    property string expression: ""

    Engine { id: engine }

    function tap(t) { window.expression += t }
    function clearAll() {
        window.expression = ""
        result.text = "0"
    }
    function backspace() {
        window.expression = window.expression.slice(0, -1)
    }
    function equals() {
        if (window.expression.length === 0)
            return
        result.text = engine.evaluate(window.expression)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 14

        // the screen: a recessed mantle panel. history, the live expression and
        // the result climb through three brightness steps so the eye lands on
        // the answer.
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 150
            radius: 18
            color: Theme.mantle

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 6

                // past results, newest at the bottom (nearest the keys).
                ListView {
                    id: historyView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 40
                    clip: true
                    model: engine.history
                    verticalLayoutDirection: ListView.BottomToTop
                    delegate: Label {
                        required property string modelData
                        width: historyView.width
                        horizontalAlignment: Text.AlignRight
                        elide: Text.ElideLeft
                        color: Theme.overlay1
                        font.pixelSize: 14
                        text: modelData
                    }
                }

                // what's being typed.
                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideLeft
                    font.pixelSize: 24
                    color: Theme.subtext0
                    text: window.expression.length > 0 ? window.expression : " "
                }

                // the last result, or "error: ..." - which turns the line red.
                Label {
                    id: result
                    objectName: "result"
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideLeft
                    font.pixelSize: 44
                    font.bold: true
                    color: result.text.startsWith("error") ? Theme.red : Theme.text
                    // a calculator at rest reads 0; clearAll resets here too.
                    text: "0"
                }
            }
        }

        // the keys. 4 columns; only `=` spans two. role colours each key.
        GridLayout {
            Layout.fillWidth: true
            columns: 4
            rowSpacing: 10
            columnSpacing: 10

            CalcButton { text: "C";  role: "clear";     onClicked: window.clearAll() }
            CalcButton { text: "(";  role: "function";  onClicked: window.tap("(") }
            CalcButton { text: ")";  role: "function";  onClicked: window.tap(")") }
            CalcButton { text: "⌫"; role: "backspace"; onClicked: window.backspace() }

            CalcButton { text: "7"; onClicked: window.tap("7") }
            CalcButton { text: "8"; onClicked: window.tap("8") }
            CalcButton { text: "9"; onClicked: window.tap("9") }
            CalcButton { text: "÷"; role: "operator"; onClicked: window.tap("/") }

            CalcButton { text: "4"; onClicked: window.tap("4") }
            CalcButton { text: "5"; onClicked: window.tap("5") }
            CalcButton { text: "6"; onClicked: window.tap("6") }
            CalcButton { text: "×"; role: "operator"; onClicked: window.tap("*") }

            CalcButton { text: "1"; onClicked: window.tap("1") }
            CalcButton { text: "2"; onClicked: window.tap("2") }
            CalcButton { text: "3"; onClicked: window.tap("3") }
            CalcButton { text: "−"; role: "operator"; onClicked: window.tap("-") }

            CalcButton { text: "0"; onClicked: window.tap("0") }
            CalcButton { text: "."; onClicked: window.tap(".") }
            CalcButton { text: "^"; role: "operator"; onClicked: window.tap("^") }
            CalcButton { text: "+"; role: "operator"; onClicked: window.tap("+") }

            CalcButton { text: "%";   role: "function"; onClicked: window.tap("%") }
            CalcButton { text: "ans"; role: "function"; onClicked: window.tap("ans") }
            CalcButton {
                text: "="
                role: "equals"
                Layout.columnSpan: 2
                onClicked: window.equals()
            }
        }
    }
}
