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

# Window icon (":/res/AppIcon.png"), compiled in through AUTORCC.
set(RESOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.qrc)

file(GLOB H_FILES "src/*.h")
file(GLOB CPP_FILES "src/*.cpp")
file(GLOB INCLUDE "src/include/*.*")

if (APPLE)
    set(CMAKE_MACOSX_BUNDLE YES)

    set(XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Manual")
    set(XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "6553V8F734")
    set(XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Developer ID Application")
    set(XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED TRUE)

    set(MACOSX_BUNDLE_ICON_FILE AppIcon.icns)
    set(app_icon_macos "${CMAKE_CURRENT_SOURCE_DIR}/res/AppIcon.icns")
    set_source_files_properties(${app_icon_macos} PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${CPP_FILES} ${H_FILES} ${INCLUDE} ${RESOURCE_FILES} ${app_icon_macos})
    set_target_properties(${PROJECT_NAME} PROPERTIES
            MACOSX_BUNDLE TRUE
            XCODE_ATTRIBUTE_CODE_SIGN_STYLE ${XCODE_ATTRIBUTE_CODE_SIGN_STYLE}
            XCODE_ATTRIBUTE_DEVELOPMENT_TEAM ${XCODE_ATTRIBUTE_DEVELOPMENT_TEAM}
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ${XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY}
            XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED ${XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED}
            XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS "--timestamp=http://timestamp.apple.com/ts01  --options=runtime,library"
            XCODE_ATTRIBUTE_CODE_SIGN_INJECT_BASE_ENTITLEMENTS "NO"
    )

elseif (WIN32)
    # A GUI app (no console window). The .exe's own icon, shown by Explorer and the taskbar,
    # comes from a Windows resource; the window icon comes from the .qrc like elsewhere. The .rc
    # is generated so it can name the .ico by absolute path.
    set(app_icon_windows "${CMAKE_CURRENT_BINARY_DIR}/AppIcon.rc")
    file(WRITE "${app_icon_windows}" "IDI_ICON1 ICON \"${CMAKE_CURRENT_SOURCE_DIR}/res/AppIcon.ico\"\n")
    add_executable(${PROJECT_NAME} WIN32 ${CPP_FILES} ${H_FILES} ${INCLUDE} ${RESOURCE_FILES} ${app_icon_windows})

else ()
    add_executable(${PROJECT_NAME} ${CPP_FILES} ${H_FILES} ${INCLUDE} ${RESOURCE_FILES})
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE Qt::Core
        Qt::Gui
        Qt::Widgets)