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

## license

mit. do whatever u want with it, see [LICENSE](LICENSE).
