# changelog

the notable changes to stupid idiot calc. the format follows [keep a
changelog](https://keepachangelog.com/en/1.1.0/) and the project uses [semantic
versioning](https://semver.org/).

## [unreleased]

### added

- a scientific function panel behind a `ƒ(x)` toggle above the keypad: `sin`,
  `cos`, `tan`, `√`, `ln`, `log`, `exp`, `abs`, `floor`, `ceil` and the
  constants `π` and `e` — all things the engine could already do but the gui
  had no way to tap. a function key drops in `name(` so u just type the arg and
  close the paren; `π`/`e` go in as bare names. trig is in degrees (theres a
  `deg` marker on the toggle).
- the panel opens and closes with a smooth animation that reflows the keypad to
  fit instead of resizing the window — the keys just shrink to make room.

### changed

- reskinned the gui in catppuccin mocha: the window, a recessed display panel
  where history/expression/result step up in brightness, and colour-coded keys
  (mauve operators, blue functions, a filled `=`, red `C`, peach `⌫`). it reads
  `0` at rest now.
- pinned the Basic qt quick controls style so the gui looks identical on mac,
  linux and windows instead of each platforms native one.

### removed

- the "uses Qt 6 under LGPLv3" line from the window — that notice already lives
  in the readme and `licenses/`, no need to repeat it in the ui.

### fixed

- a long flat expression like `1+1+1+...`, right up to the 4096-token cap, no
  longer crashes the process on a small thread stack — the kind android and qt
  worker threads run on. it builds a tree ~2000 nodes deep, and walking or
  freeing that used to recurse one stack frame per node; the evaluator, the
  `--trace` tree render and the tree teardown all do it iteratively now, so the
  depth cant overflow the stack. desktop behaviour is unchanged and the same
  expression still returns the same answer.
- an oversized line (a giant paste, a multi-megabyte argument) is now rejected
  at the lexer the moment it passes the 4096-token cap, instead of building the
  whole token vector first and only then checking the size. a five-million char
  line used to balloon to a few hundred megabytes before the parser turned it
  down; now the token vector never grows past the cap. the error is the same
  "expression is too long" as before, just paid cheaply up front.
- a long session no longer grows its retained history without bound: the repl's
  `:history` log and the gui's history/recall lists are each capped at the last
  1000 entries, so the oldest fall off instead of piling up one per evaluate for
  the life of the session.
- a result that lands on negative zero (`0 * -1`, `0 / -1`, `-5 * 0`,
  `ceil(-0.9)`, ...) now prints a plain `0` instead of `-0`.
- `0` to a negative power (`0^-1`, `0^-2`, `0^-0.5`) now reports "cant divide by
  zero" — its a pole, same as `1/0` — instead of the misleading "result is too
  large".
- a number literal too small for a double (`1e-400`, `1e-500`) now reads as `0`
  instead of being rejected as "number is out of range" — that message is kept
  for a real overflow like `1e400`, which is a different thing.
- large-angle trig is accurate again: the angle is reduced mod 360 in degrees
  (exactly) before converting to radians, so `sin(360)` is `0`, and `sin(1e15)`
  / `cos(1e10)` no longer drift in the 4th significant figure. small and whole
  angles are unchanged.
- a piped repl line with an embedded NUL byte is rejected now instead of being
  silently mangled: the NUL used to get dropped and the bytes on either side
  merged into a different number (`2+2`, NUL, `2+3` came out as `2+22+3` = 27).
  piped input is read with getline, which keeps the NUL so the engine can flag
  it.

## [0.1.0] - 2026-06-16

the first release: the calculation engine, a cli/repl, a desktop gui, and
installable builds for mac, linux and windows.

### added

- the engine — a hand-rolled lexer → recursive-descent parser → tree-walking
  evaluator for `+ - * / % ^`, named functions (`sqrt sin cos tan abs ln log
  exp floor ceil`) and constants (`pi`, `e`), with `ans`, memory (`m+ m- mr
  mc`) and `let` bindings through an `Environment`. divide-by-zero and domain
  errors come back as values, never crashes, and parse errors point at the
  mistake with a clang-style caret. output goes through fmt.
- a cli/repl front-end: `--version`, one-shot mode, `--trace` to show the eval
  steps, a leveled logger, a crash handler, isocline line-editing and `:`
  meta-commands.
- a qt 6 quick/qml desktop gui (`calc-gui`) that drives the same engine
  unchanged — a history list, the expression, the result and a grid of keys.
- number parsing via fast_float, so the engine builds the same on gcc, clang
  and msvc (and libc++ and the android ndk).
- packaging: a mac `.dmg` and a windows `.zip` with qt bundled in, a linux
  `.flatpak`, and a tag-triggered draft github release that ties the three
  together. `--version` reports the release tag.
- ci on linux, macos and windows — the `build` check gates merges, with
  advisory sanitizer, clang-tidy, coverage, fuzz and libc++ legs alongside it.

[unreleased]: https://github.com/soleynn/stupid-idiot-calc/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/soleynn/stupid-idiot-calc/releases/tag/v0.1.0
