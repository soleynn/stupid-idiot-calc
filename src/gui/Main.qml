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
    height: 640
    minimumWidth: 300
    minimumHeight: 560
    title: qsTr("stupid idiot calc")
    color: Theme.base

    // the expression the user is building up, tap by tap.
    property string expression: ""
    // is the scientific function panel showing? collapsed by default.
    property bool sciOpen: false
    // height the panel takes when open. the keypad below shrinks by this much to
    // make room - the window itself never resizes, the keys just get smaller (a
    // phone cant grow, so neither does this).
    readonly property int sciPanelHeight: 130

    Engine { id: engine }

    function tap(t) { window.expression += t }
    function toggleSci() { window.sciOpen = !window.sciOpen }
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
        spacing: 12

        // the screen: a recessed mantle panel. history, the live expression and
        // the result climb through three brightness steps so the eye lands on
        // the answer.
        Rectangle {
            Layout.fillWidth: true
            // gives a little height back to the keys when the panel is open, so
            // it isnt only the keypad that shrinks. animates in step with it.
            Layout.preferredHeight: window.sciOpen ? 100 : 130
            radius: 18
            color: Theme.mantle

            Behavior on Layout.preferredHeight {
                NumberAnimation { duration: 200; easing.type: Easing.InOutCubic }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 4

                // past results, newest at the bottom (nearest the keys). min 0
                // so it collapses when the screen is short (the function panel
                // open, a small window) instead of pushing the result off-panel.
                ListView {
                    id: historyView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 0
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

        // reveals the functions the engine already has. collapsed by default so
        // the basic keypad stays the clean thing u see first. "deg" flags that
        // the trig keys work in degrees (theres no rad mode yet).
        Button {
            id: sciToggle
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            onClicked: window.toggleSci()

            contentItem: Item {
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    text: window.sciOpen ? "ƒ(x)  ▴" : "ƒ(x)  ▾"
                    font.pixelSize: 17
                    font.bold: true
                    color: Theme.blue
                }
                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("deg")
                    font.pixelSize: 13
                    color: Theme.overlay1
                }
            }
            background: Rectangle {
                radius: 12
                color: sciToggle.down ? Qt.lighter(Theme.surface0, 1.3)
                     : sciToggle.hovered ? Qt.lighter(Theme.surface0, 1.15)
                     : Theme.surface0
                border.width: sciToggle.activeFocus ? 2 : 0
                border.color: Theme.lavender
                Behavior on color { ColorAnimation { duration: 90 } }
            }
        }

        // the scientific keys. the wrapper animates its height between 0 and the
        // panel height and clips, so the panel slides open/shut; the keypad below
        // (fillHeight) takes back exactly that much, shrinking its keys to fit.
        // each key inserts a call ready for its argument (`sin(` etc.); pi/e drop
        // in as bare names. blue marks the whole zone.
        Item {
            id: sciWrap
            Layout.fillWidth: true
            Layout.preferredHeight: window.sciOpen ? window.sciPanelHeight : 0
            clip: true

            Behavior on Layout.preferredHeight {
                NumberAnimation { duration: 200; easing.type: Easing.InOutCubic }
            }

            GridLayout {
                id: sciPanel
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: window.sciPanelHeight
                columns: 4
                rowSpacing: 8
                columnSpacing: 10
                opacity: window.sciOpen ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 160 } }

                CalcButton { text: "sin";   role: "sci"; onClicked: window.tap("sin(") }
                CalcButton { text: "cos";   role: "sci"; onClicked: window.tap("cos(") }
                CalcButton { text: "tan";   role: "sci"; onClicked: window.tap("tan(") }
                CalcButton { text: "√";     role: "sci"; onClicked: window.tap("sqrt(") }

                CalcButton { text: "ln";    role: "sci"; onClicked: window.tap("ln(") }
                CalcButton { text: "log";   role: "sci"; onClicked: window.tap("log(") }
                CalcButton { text: "exp";   role: "sci"; onClicked: window.tap("exp(") }
                CalcButton { text: "abs";   role: "sci"; onClicked: window.tap("abs(") }

                CalcButton { text: "floor"; role: "sci"; onClicked: window.tap("floor(") }
                CalcButton { text: "ceil";  role: "sci"; onClicked: window.tap("ceil(") }
                CalcButton { text: "π";     role: "sci"; onClicked: window.tap("pi") }
                CalcButton { text: "e";     role: "sci"; onClicked: window.tap("e") }
            }
        }

        // the keys. 4 columns; only `=` spans two. role colours each key.
        // fillHeight so the keypad gives up room (smaller keys) when the panel
        // opens and reclaims it when it shuts.
        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
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
