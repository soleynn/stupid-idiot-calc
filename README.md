# stupid idiot calc

a basic calculator im writing in c++. mostly an excuse to actually figure out how github works under the hood (branches, prs, ci, all that stuff).

dont expect anything fancy. its a calculator. it does calculator things.

## whats in here (eventually)

- [ ] add, subtract, multiply, divide
- [ ] dont fall over on garbage input
- [ ] maybe a little repl loop so u can keep typing at it
- [ ] who knows, well see

## building it

no real build setup yet, but the rough plan is just compile the sources straight with g++:

```sh
g++ -std=c++17 -Wall -Wextra -o calc *.cpp
./calc
```

## license

mit. do whatever u want with it, see [LICENSE](LICENSE).
