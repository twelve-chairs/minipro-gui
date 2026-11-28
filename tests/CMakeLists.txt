cmake_minimum_required(VERSION 3.16)

project(minipro-gui-tests LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Test Widgets)

qt_standard_project_setup()
# qt_standard_project_setup() turns on AUTOMOC and AUTOUIC but not AUTORCC (for the .qrc).
set(CMAKE_AUTORCC ON)
enable_testing()

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)

# Code coverage for SonarQube Cloud (tests/coverage.sh). Clang only: source-based coverage, read
# back with llvm-profdata and llvm-cov. The compilation database tells Sonar's C++ analyzer how
# each file was compiled.
option(COVERAGE "Build with clang source-based code coverage" OFF)
if (COVERAGE)
  if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "COVERAGE needs clang (source-based coverage), not ${CMAKE_CXX_COMPILER_ID}")
  endif ()
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif ()

# Instruments one target for coverage when COVERAGE is on. Not the fake minipro or the screenshot
# tool: they aren't app code, and the fake would write a profile on every call.
function(enable_coverage target)
  if (COVERAGE)
    target_compile_options(${target} PRIVATE -fprofile-instr-generate -fcoverage-mapping)
    # PUBLIC: anything linking the instrumented app_objects needs the profile runtime too.
    target_link_options(${target} PUBLIC -fprofile-instr-generate)
  endif ()
endfunction()

# One executable per QtTest class (each file has its own QTEST_MAIN).
# Usage: add_qt_test(<name> <test source> [extra sources...])
function(add_qt_test name test_source)
  qt_add_executable(${name} ${test_source} ${ARGN})
  target_include_directories(${name} PRIVATE ${SRC_DIR})
  target_link_libraries(${name} PRIVATE Qt6::Core Qt6::Gui Qt6::Test Qt6::Widgets)
  # A console program on Windows: qt_add_executable makes a GUI one, whose output is lost.
  set_target_properties(${name} PROPERTIES WIN32_EXECUTABLE OFF)
  add_test(NAME ${name} COMMAND ${name})
  # Run headless so tests work on CI and don't pop up windows locally.
  set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  enable_coverage(${name})
endfunction()

add_qt_test(hexview_test
        HexViewTest.cpp
        ${SRC_DIR}/HexView.h
        ${SRC_DIR}/HexView.cpp
)

if (WIN32)
  target_link_libraries(hexview_test PRIVATE psapi)  # GetProcessMemoryInfo, for the memory check
endif ()

# The app's sources (all of src/ but main.cpp), compiled once for mainwindow_test and screenshot.
# A new src/*.cpp file goes here.
add_library(app_objects OBJECT
        ${SRC_DIR}/MainWindow.h
        ${SRC_DIR}/MainWindow.cpp
        ${SRC_DIR}/HexView.h
        ${SRC_DIR}/HexView.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../minipro-gui.qrc  # the window icon, as in the app
)
target_include_directories(app_objects PUBLIC ${SRC_DIR})
target_link_libraries(app_objects PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets)
enable_coverage(app_objects)

if (COVERAGE)
  # Compiled only so it is in the compilation database: Sonar's C++ analyzer skips any file that
  # isn't. No test runs it (QTEST_MAIN provides main), so sonar-project.properties leaves it out
  # of coverage.
  add_library(app_main OBJECT ${SRC_DIR}/main.cpp)
  target_link_libraries(app_main PRIVATE app_objects)
endif ()

add_qt_test(mainwindow_test MainWindowTest.cpp)
target_link_libraries(mainwindow_test PRIVATE app_objects)

# The stand-in for the minipro CLI. MainWindowTest copies it into a temp dir as "minipro".
# Plain C++ with no Qt, so it runs on Windows without Qt DLLs next to it.
add_executable(fake_minipro fake_minipro.cpp)
add_dependencies(mainwindow_test fake_minipro)
target_compile_definitions(mainwindow_test PRIVATE
        FAKE_MINIPRO_SOURCE="$<TARGET_FILE:fake_minipro>")

# Not a test: renders the README screenshot from the real UI (tests/update_screenshot.sh).
qt_add_executable(screenshot screenshot.cpp)
target_link_libraries(screenshot PRIVATE app_objects)
set_target_properties(screenshot PROPERTIES WIN32_EXECUTABLE OFF)
add_dependencies(screenshot fake_minipro)
target_compile_definitions(screenshot PRIVATE FAKE_MINIPRO_SOURCE="$<TARGET_FILE:fake_minipro>")
