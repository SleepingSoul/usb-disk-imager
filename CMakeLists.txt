cmake_minimum_required(VERSION 3.21)

project(usb-disk-imager
    VERSION 1.0.0
    DESCRIPTION "Cross-platform raw disk imager for USB drives and memory cards"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Product metadata. The single source of truth: CMake renders it into utils/buildinfo.hpp, and the About
# page reads it from there.
set(UDI_APP_NAME "USB Disk Imager")
set(UDI_APP_ID "usb-disk-imager")
set(UDI_ORG_NAME "Tihran Katolikian")
set(UDI_ORG_DOMAIN "tkatolikian.dev")
set(UDI_AUTHOR "Tihran Katolikian")
set(UDI_AUTHOR_EMAIL "tkatolikian@outlook.com")
set(UDI_COPYRIGHT_YEAR "2026")
set(UDI_HOMEPAGE "https://github.com/SleepingSoul/usb-disk-imager")
set(UDI_LICENSE "MIT")

option(UDI_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(UDI_USE_SYSTEM_XXHASH "Link a system xxHash instead of fetching it" OFF)
option(UDI_USE_SYSTEM_SPDLOG "Link a system spdlog instead of fetching it" OFF)

# The Qt this project is built, tested and released against. It is stated once here; CMakePresets.json
# points at kits of this version, and the Design Studio project declares the matching Quick version.
# An older Qt is rejected at configure time rather than left to fail later inside a QML import.
set(UDI_QT_MINIMUM_VERSION 6.8.3)

find_package(Qt6 ${UDI_QT_MINIMUM_VERSION} REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    Svg
    LinguistTools
)

message(STATUS "Using Qt ${Qt6_VERSION} from ${Qt6_DIR} (${UDI_QT_MINIMUM_VERSION} or newer required)")

include(cmake/Dependencies.cmake)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/buildinfo.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/utils/buildinfo.hpp
    @ONLY)

set(PROJECT_SOURCES
    src/main.cpp
    src/application.cpp
    src/app/manager.cpp
    src/app/managerconfig.cpp
    src/disk/disktypes.cpp
    src/disk/partitiontable.cpp
    src/disk/rawdevice.cpp
    src/imaging/hasher.cpp
    src/imaging/imagingjob.cpp
    src/imaging/imagingtypes.cpp
    src/managers/devicemanager.cpp
    src/managers/imagingmanager.cpp
    src/managers/localizationmanager.cpp
    src/managers/loggingmanager.cpp
    src/managers/uimanager.cpp
    src/ui/appviewmodel.cpp
    src/ui/devicelistmodel.cpp
    src/ui/imagingviewmodel.cpp
    src/ui/localizationviewmodel.cpp
    src/utils/crc32.cpp
    src/utils/formatting.cpp
    src/utils/usersettings.cpp
)

set(PROJECT_HEADERS
    src/application.hpp
    src/app/commonutils.hpp
    src/app/manager.hpp
    src/app/managerconfig.hpp
    src/configs/devicemanagerconfig.hpp
    src/configs/imagingmanagerconfig.hpp
    src/configs/localizationmanagerconfig.hpp
    src/configs/loggingmanagerconfig.hpp
    src/configs/uimanagerconfig.hpp
    src/disk/deviceenumerator.hpp
    src/disk/disktypes.hpp
    src/disk/filesystemresizer.hpp
    src/disk/partitiontable.hpp
    src/disk/rawdevice.hpp
    src/disk/volumecontrol.hpp
    src/imaging/hasher.hpp
    src/imaging/imagingjob.hpp
    src/imaging/imagingtypes.hpp
    src/managers/devicemanager.hpp
    src/managers/imagingmanager.hpp
    src/managers/localizationmanager.hpp
    src/managers/loggingmanager.hpp
    src/managers/uimanager.hpp
    src/ui/appviewmodel.hpp
    src/ui/devicelistmodel.hpp
    src/ui/imagingviewmodel.hpp
    src/ui/localizationviewmodel.hpp
    src/ui/viewmodel.hpp
    src/utils/crc32.hpp
    src/utils/formatting.hpp
    src/utils/privileges.hpp
    src/utils/usersettings.hpp
)

# Device enumeration, raw device access, taking volumes offline, gaining rights and resizing a
# filesystem are the five things that have no portable form, so each platform brings its own
# implementation of the same headers.
if (WIN32)
    list(APPEND PROJECT_SOURCES
        src/disk/windows/deviceenumerator.cpp
        src/disk/windows/filesystemresizer.cpp
        src/disk/windows/rawdevice.cpp
        src/disk/windows/volumecontrol.cpp
        src/disk/windows/windowsutils.cpp
        src/utils/windows/privileges.cpp
    )
    list(APPEND PROJECT_HEADERS
        src/disk/windows/windowsutils.hpp
    )
elseif (APPLE)
    list(APPEND PROJECT_SOURCES
        src/disk/macos/deviceenumerator.cpp
        src/disk/macos/filesystemresizer.cpp
        src/disk/macos/volumecontrol.cpp
        src/disk/unix/rawdevice.cpp
        src/utils/unix/privileges.cpp
    )
elseif (UNIX)
    list(APPEND PROJECT_SOURCES
        src/disk/linux/deviceenumerator.cpp
        src/disk/linux/filesystemresizer.cpp
        src/disk/linux/volumecontrol.cpp
        src/disk/unix/rawdevice.cpp
        src/utils/unix/privileges.cpp
    )
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

qt_add_executable(${PROJECT_NAME}
    WIN32
    MACOSX_BUNDLE
    ${PROJECT_SOURCES}
    ${PROJECT_HEADERS}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE QT_NO_KEYWORDS)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/generated
    src
)

# Managers look for configs/<name>.json next to the executable first and fall back to these, so a build
# needs no loose files to run.
qt_add_resources(${PROJECT_NAME} "udi_configs"
    PREFIX "/configs"
    BASE "configs"
    FILES
        configs/devicemanagerconfig.json
        configs/imagingmanagerconfig.json
        configs/localizationmanagerconfig.json
        configs/loggingmanagerconfig.json
        configs/uimanagerconfig.json
)

add_subdirectory(content)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Svg
    contentplugin
    udi::xxhash
    udi::spdlog
)

if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        UNICODE
        _UNICODE
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE advapi32 shell32)
elseif (APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework IOKit"
        "-framework CoreFoundation"
    )
endif()

if (MSVC)
    # /utf-8 is not optional: the sources carry Ukrainian text and typographic punctuation.
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
    if (UDI_WARNINGS_AS_ERRORS)
        target_compile_options(${PROJECT_NAME} PRIVATE /WX)
    endif()
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
    if (UDI_WARNINGS_AS_ERRORS)
        target_compile_options(${PROJECT_NAME} PRIVATE -Werror)
    endif()
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "${UDI_APP_NAME}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "dev.tkatolikian.usbdiskimager"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
    MACOSX_BUNDLE_COPYRIGHT "© ${UDI_COPYRIGHT_YEAR} ${UDI_AUTHOR}"
)

# lupdate has to see both targets: the C++ tr() calls live in the executable, the qsTr() calls in the QML
# module.
qt_add_translations(${PROJECT_NAME}
    SOURCE_TARGETS ${PROJECT_NAME} content
    TS_FILES
        i18n/usb-disk-imager_en.ts
        i18n/usb-disk-imager_uk.ts
    RESOURCE_PREFIX "/i18n"
    LUPDATE_OPTIONS -no-obsolete -locations none
)

include(cmake/Deploy.cmake)
