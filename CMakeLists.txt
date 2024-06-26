cmake_minimum_required(VERSION 3.22)

set(PROJECT_NAME "minipro-gui")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(${PROJECT_NAME} LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS
        Core
        Gui
        Widgets
        REQUIRED)

set(RESOURCE_FILES ${PROJECT_NAME}.qrc)

file(GLOB H_FILES "src/*.h")
file(GLOB CPP_FILES "src/*.cpp")
file(GLOB INCLUDE "src/include/*.*")

if (APPLE)
    set(MACOSX_BUNDLE_ICON_FILE AppIcon.icns)
    set(app_icon_macos "${CMAKE_CURRENT_SOURCE_DIR}/res/AppIcon.icns")
    set_source_files_properties(${app_icon_macos} PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${CPP_FILES} ${H_FILES} ${INCLUDE} ${app_icon_macos})
else ()
    add_executable(${PROJECT_NAME} ${CPP_FILES} ${H_FILES} ${INCLUDE})
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE Qt::Core
        Qt::Gui
        Qt::Widgets)