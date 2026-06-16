# stupid idiot calc

a basic calculator im writing in c++. mostly an excuse to actually figure out how github works under the hood (branches, prs, ci, all that stuff).

dont expect anything fancy. its a calculator. it does calculator things.

## whats in here (eventually)

- [ ] add, subtract, multiply, divide
- [ ] dont fall over on garbage input
- [ ] maybe a little repl loop so u can keep typing at it
- [ ] who knows, well see

## building it

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

theres also a little qt 6 quick/qml window front-end. its off by default so the
plain build needs no qt. to build it u need qt 6 (6.5+) installed; then:

```sh
cmake --preset gui
cmake --build --preset gui
./build/gui/calc-gui
```

same engine underneath, just buttons instead of typing. one qml codebase so it
can run on a phone later too.

## license

the calculator itself is mit - do whatever u want with it, see [LICENSE](LICENSE).

the gui links **qt 6**, which is used under the **LGPLv3**. qt is linked
dynamically and isnt modified or bundled into this repo (the build fetches it
separately), so the mit license on this code stands; the lgpl just covers qt.
the full LGPLv3 + GPLv3 texts are in [licenses/](licenses/), and if u ship a
build u have to keep qt replaceable (dynamic-linked) per the lgpl - which it
already is. read qt.io's lgpl page urself before handing anyone a build.
