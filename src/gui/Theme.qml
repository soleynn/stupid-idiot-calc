pragma Singleton
import QtQuick

// catppuccin mocha, and the one place colour lives - Main.qml and CalcButton.qml
// read these names, nothing hardcodes a hex. to reskin to another flavour swap
// these values (or this whole file); the layout never has to change.
QtObject {
    // surfaces, darkest first: the window sits on base, the screen recesses to
    // mantle, the keys ride up on surface0/1.
    readonly property color crust:    "#11111b"
    readonly property color mantle:   "#181825"
    readonly property color base:     "#1e1e2e"
    readonly property color surface0: "#313244"
    readonly property color surface1: "#45475a"

    // text, brightest to dimmest - the three steps the screen leans on.
    readonly property color text:     "#cdd6f4"
    readonly property color subtext0: "#a6adc8"
    readonly property color overlay1: "#7f849c"

    // accents. mauve is the action colour (operators + the = key), blue the
    // scientific functions + constants, red flags a clear or an error, peach the
    // backspace.
    readonly property color mauve:    "#cba6f7"
    readonly property color lavender: "#b4befe"
    readonly property color blue:     "#89b4fa"
    readonly property color red:      "#f38ba8"
    readonly property color peach:    "#fab387"
}
