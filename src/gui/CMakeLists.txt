# the Qt 6 Quick/QML front-end. one QObject (Engine) bridges qml to the engine;
# everything else is qml. it links calc_core UNCHANGED - the same i/o-free static
# lib the cli and tests link - which is the whole architectural payoff. Qt is
# dynamic-linked under LGPLv3 (see the README note + licenses/), comes prebuilt
# via aqtinstall in ci, and is never vendored into the repo.
find_package(Qt6 REQUIRED COMPONENTS Quick)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(calc-gui main.cpp)

qt_add_qml_module(calc-gui
  URI calc.gui
  VERSION 1.0
  QML_FILES
    Main.qml
    CalcButton.qml
  SOURCES
    engine_bridge.hpp
    engine_bridge.cpp)

# main.cpp + the generated TUs include "engine_bridge.hpp" from this dir.
target_include_directories(calc-gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# deliberately NOT linked to calc_warnings / calc_strict. the gui target also
# compiles moc- and qml-generated translation units we dont own, the same reason
# the isocline C lib is kept off our flag set; the real logic is in calc_core,
# which stays strictly warned + -Werror'd. Qt6::Quick's headers come in as system
# includes (imported target), so they dont trip warnings either.
target_link_libraries(calc-gui PRIVATE calc_core Qt6::Quick)
