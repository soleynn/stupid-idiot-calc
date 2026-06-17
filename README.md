# stupid idiot calc

<p align="center">
  <a href="https://github.com/soleynn/stupid-idiot-calc/actions/workflows/ci.yml"><img src="https://github.com/soleynn/stupid-idiot-calc/actions/workflows/ci.yml/badge.svg" alt="ci" /></a>
  <a href="https://github.com/soleynn/stupid-idiot-calc/releases/latest"><img src="https://img.shields.io/github/v/release/soleynn/stupid-idiot-calc?color=cba6f7&labelColor=302d41" alt="latest release" /></a>
  <a href="LICENSE"><img src="https://img.shields.io/github/license/soleynn/stupid-idiot-calc?color=a6e3a1&labelColor=302d41" alt="license" /></a>
  <a href="https://catppuccin.com"><img src="https://img.shields.io/badge/%F0%9F%8D%A8%20catppuccin-mocha-cba6f7?labelColor=302d41" alt="catppuccin mocha" /></a>
</p>

a basic calculator im writing in c++. mostly an excuse to actually figure out how github works under the hood: branches, prs, ci, all that stuff.

dont expect anything fancy. its a calculator. it does calculator things. theres a cli/repl and a little qt desktop gui, both driving the same engine underneath.

## grab a build

prebuilt installers for mac, linux and windows are on the [releases page](https://github.com/soleynn/stupid-idiot-calc/releases/latest). they bundle the gui (qt comes with it), so theres nothing to turn on. download, open, done.

theyre unsigned though, so ur os will throw up a warning the first time. heres how to get past it:

**macos (.dmg)**: not notarized, so gatekeeper quarantines it. mount the dmg, drag the app to Applications, then right-click it and pick Open (and Open again in the dialog). or clear the quarantine flag in a terminal:

```sh
xattr -dr com.apple.quarantine /Applications/calc-gui.app
```

**windows (.zip)**: smartscreen may say "Windows protected your PC". click More info, then Run anyway. unzip it and run `bin/calc-gui.exe` (keep the dlls next to it).

**linux (.flatpak)**: a single-file bundle. it needs the org.kde.Platform 6.10 runtime (add [flathub](https://flathub.org/setup) first if u dont have it):

```sh
flatpak install --user stupid-idiot-calc.flatpak
flatpak run io.github.soleynn.StupidIdiotCalc
```

## what it does

- the four basics (`+` `−` `×` `÷`) plus `%`, `^` and parentheses, with proper precedence
- functions: `sqrt sin cos tan ln log exp abs floor ceil` (trig works in degrees)
- the constants `pi` and `e`, `ans` for ur last result, `M+ M- MR MC` memory, and `let x = ...` to name values
- a repl with history, tab-completion and `:`commands, or one-shot mode: `calc "2 + 2"` prints `4` and nothing else, so it pipes
- doesnt fall over on garbage input: errors come back as a message with a caret pointing right at the mistake, never a crash

## building from source

needs cmake, ninja and a c++17 compiler.

```sh
cmake --preset debug
cmake --build --preset debug
./build/debug/calc
```

run the tests with:

```sh
ctest --preset debug
```

### the gui

theres also a little qt 6 quick/qml window front-end. its off by default so the plain build needs no qt. to build it urself u need qt 6 (6.5+) installed; then:

```sh
cmake --preset gui
cmake --build --preset gui
./build/gui/calc-gui
```

(if u just want to *use* the gui, grab a release above; this is only for building it from source.) same engine underneath, just buttons instead of typing, and one qml codebase so it can run on a phone later too.

## the theme

the gui wears [catppuccin mocha](https://catppuccin.com): the window, the recessed screen, the colour-coded keys, all of it. catppuccin doesnt ask for credit, but i love the palette and theyve more than earned a shoutout. go check it out.

## license

the calculator itself is mit: do whatever u want with it, see [LICENSE](LICENSE).

the gui links **qt 6**, used under the **LGPLv3**. qt is linked dynamically and isnt modified or bundled into this repo (the build fetches it separately), so the mit license on this code stands; the lgpl just covers qt. the full LGPLv3 + GPLv3 texts are in [licenses/](licenses/), and if u ship a build u have to keep qt replaceable (dynamic-linked) per the lgpl, which it already is. read qt.io's lgpl page urself before handing anyone a build.

<p align="center">
  <img src="https://raw.githubusercontent.com/catppuccin/catppuccin/main/assets/footers/gray0_ctp_on_line.png" />
</p>
