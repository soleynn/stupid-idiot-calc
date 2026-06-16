import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// the whole window: a history list, the expression being typed, the result, a
// grid of keys, and a small license line. Engine is the C++ bridge from this
// same qml module, so it needs no import. one codebase here serves desktop and
// (later) android - thats why its Quick/QML and not Widgets.
ApplicationWindow {
    id: window
    visible: true
    width: 360
    height: 600
    minimumWidth: 280
    minimumHeight: 460
    title: qsTr("stupid idiot calc")

    // the expression the user is building up, tap by tap.
    property string expression: ""

    Engine { id: engine }

    function tap(t) { window.expression += t }
    function clearAll() {
        window.expression = ""
        result.text = ""
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
        anchors.margins: 10
        spacing: 8

        // past results, newest at the bottom (nearest the keys).
        ListView {
            id: historyView
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 60
            clip: true
            model: engine.history
            verticalLayoutDirection: ListView.BottomToTop
            delegate: Label {
                required property string modelData
                width: historyView.width
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideLeft
                color: palette.placeholderText
                font.pixelSize: 14
                text: modelData
            }
        }

        // what's being typed.
        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideLeft
            font.pixelSize: 22
            color: palette.text
            text: window.expression.length > 0 ? window.expression : " "
        }

        // the last result (or "error: ...").
        Label {
            id: result
            objectName: "result"
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideLeft
            font.pixelSize: 36
            font.bold: true
            text: ""
        }

        // the keys. 4 columns; only `=` spans two.
        GridLayout {
            Layout.fillWidth: true
            columns: 4
            rowSpacing: 6
            columnSpacing: 6

            CalcButton { text: "C"; onClicked: window.clearAll() }
            CalcButton { text: "("; onClicked: window.tap("(") }
            CalcButton { text: ")"; onClicked: window.tap(")") }
            CalcButton { text: "⌫"; onClicked: window.backspace() }

            CalcButton { text: "7"; onClicked: window.tap("7") }
            CalcButton { text: "8"; onClicked: window.tap("8") }
            CalcButton { text: "9"; onClicked: window.tap("9") }
            CalcButton { text: "÷"; onClicked: window.tap("/") }

            CalcButton { text: "4"; onClicked: window.tap("4") }
            CalcButton { text: "5"; onClicked: window.tap("5") }
            CalcButton { text: "6"; onClicked: window.tap("6") }
            CalcButton { text: "×"; onClicked: window.tap("*") }

            CalcButton { text: "1"; onClicked: window.tap("1") }
            CalcButton { text: "2"; onClicked: window.tap("2") }
            CalcButton { text: "3"; onClicked: window.tap("3") }
            CalcButton { text: "−"; onClicked: window.tap("-") }

            CalcButton { text: "0"; onClicked: window.tap("0") }
            CalcButton { text: "."; onClicked: window.tap(".") }
            CalcButton { text: "^"; onClicked: window.tap("^") }
            CalcButton { text: "+"; onClicked: window.tap("+") }

            CalcButton { text: "%"; onClicked: window.tap("%") }
            CalcButton { text: "ans"; onClicked: window.tap("ans") }
            CalcButton {
                text: "="
                Layout.columnSpan: 2
                onClicked: window.equals()
            }
        }

        // lgpl obligation: Qt is dynamically linked under LGPLv3. full texts
        // live in licenses/; the readme has the relink note.
        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 11
            color: palette.placeholderText
            text: qsTr("uses Qt 6 under LGPLv3 · see licenses/")
        }
    }
}
