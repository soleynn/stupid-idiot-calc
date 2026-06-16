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

# stamp the version in so `calc-gui --version` reports it, same as the cli.
# defaults to the project version; a release build overrides CALC_VERSION.
target_compile_definitions(calc-gui PRIVATE CALC_VERSION="${CALC_VERSION}")

# deliberately NOT linked to calc_warnings / calc_strict. the gui target also
# compiles moc- and qml-generated translation units we dont own, the same reason
# the isocline C lib is kept off our flag set; the real logic is in calc_core,
# which stays strictly warned + -Werror'd. Qt6::Quick's headers come in as system
# includes (imported target), so they dont trip warnings either.
target_link_libraries(calc-gui PRIVATE calc_core Qt6::Quick)

# install rules — the flatpak (and later the mac/win deploy) install from here.
# the binary goes to bin/; the freedesktop desktop file, appstream metainfo and
# icon are linux-only (mac/windows dont use them), keyed to the reverse-dns app id
# so the app shows up with a name + icon after `flatpak install`.
include(GNUInstallDirs)
set(CALC_APP_ID io.github.soleynn.StupidIdiotCalc)

# linux/flatpak keeps the bin/ layout; mac/win get a root-level install (below)
# so the bundled Qt sits right next to the app in the archive.
if(NOT (APPLE OR WIN32))
  install(TARGETS calc-gui RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  install(FILES ${PROJECT_SOURCE_DIR}/flatpak/${CALC_APP_ID}.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
  install(FILES ${PROJECT_SOURCE_DIR}/flatpak/${CALC_APP_ID}.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
  install(FILES ${PROJECT_SOURCE_DIR}/flatpak/${CALC_APP_ID}.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
endif()

# self-contained desktop archives: a mac .dmg and a windows .zip with Qt bundled
# (dynamic, LGPLv3). linux is packaged via flatpak, so this whole block is
# apple/win only and never runs on the linux gui build. (the cli is not shipped -
# only calc-gui.)
if(APPLE OR WIN32)
  # mac wants a relocatable .app so macdeployqt can rewrite the framework load
  # paths to @executable_path/../Frameworks. stamp the Info.plist from the vars
  # cmake generates - no custom plist template for now.
  if(APPLE)
    set_target_properties(calc-gui PROPERTIES
      MACOSX_BUNDLE TRUE
      MACOSX_BUNDLE_BUNDLE_NAME "stupid idiot calc"
      MACOSX_BUNDLE_GUI_IDENTIFIER "${CALC_APP_ID}"
      MACOSX_BUNDLE_BUNDLE_VERSION "${CALC_VERSION}"
      MACOSX_BUNDLE_SHORT_VERSION_STRING "${CALC_VERSION}")
  endif()

  # mac: the .app at the dmg root (macdeployqt puts the frameworks inside it).
  # win: the exe into bin/ so it sits with its Qt6*.dll - windeployqt deploys
  # those to CMAKE_INSTALL_BINDIR, so the exe has to live there too or it cant
  # find them at launch.
  install(TARGETS calc-gui
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

  # bundle Qt + the runtime QML modules (QtQuick / Controls / Layouts) the app
  # imports. our qml is compiled into the exe via qmlcachegen, so a plain
  # mac/windeployqt --qmldir scan would MISS those imports - the _qml_ variant
  # replays the imports qt_add_qml_module recorded at build time, which is the
  # right tool here. NO_UNSUPPORTED_PLATFORM_ERROR keeps a stray linux configure
  # of this block from hard-erroring; NO_TRANSLATIONS keeps the archive lean.
  qt_generate_deploy_qml_app_script(
    TARGET calc-gui
    OUTPUT_SCRIPT calc_gui_deploy_script
    NO_TRANSLATIONS
    NO_UNSUPPORTED_PLATFORM_ERROR)
  install(SCRIPT ${calc_gui_deploy_script})
endif()
