# the calculator engine. pure logic, knows nothing about stdin/stdout. the cli
# and the tests both link this.
add_library(calc_core STATIC
  lexer.cpp
  parser.cpp
  evaluator.cpp
  output_formatter.cpp)

target_include_directories(calc_core PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(calc_core PUBLIC calc_sanitizers
                                PRIVATE calc_warnings calc_strict)
