# contributing

hi, this is mostly a personal learning project but if u wanna poke at it, cool. heres how stuff works around here.

## the basic flow

main is protected so u cant push straight to it. everything goes through a pull request:

1. branch off main -> `git checkout -b ur-branch-name`
2. do ur thing, commit as u go
3. push the branch -> `git push -u origin ur-branch-name`
4. open a pull request against main
5. ci has to go green, then it can get merged

## building

needs cmake, ninja and a c++17 compiler.

```sh
cmake --preset debug          # configure
cmake --build --preset debug  # build
ctest --preset debug          # run the tests
```

the binary lands at `build/debug/calc`. theres also a `sanitize` preset that builds the tests under asan/ubsan.

## commits

just write a plain summary of what u actually changed. like "add division and handle divide by zero", not "feat: add division". descriptive is good, the prefix tags are not my thing.

## linting

theres a clang-format check that runs in ci. it wont block a merge, its just there to flag formatting thats drifting. if it moans at u, run `clang-format -i` on ur files to auto fix.

## issues

found a bug or want a feature? open an issue, theres templates for both.
