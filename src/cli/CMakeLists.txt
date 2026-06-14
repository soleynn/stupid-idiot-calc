# thin front-end: read a line, hand it to the engine, print the result. this is
# the only part that touches stdin/stdout.
add_executable(calc main.cpp)
target_link_libraries(calc PRIVATE calc_core calc_warnings calc_strict)
