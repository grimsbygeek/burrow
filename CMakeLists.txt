cmake_minimum_required(VERSION 3.21)
project(wgtray VERSION 0.1.0 LANGUAGES CXX)
add_compile_definitions(PROJECT_VERSION="${PROJECT_VERSION}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(WGTRAY_BUILD_TESTS "Build the tests" ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core DBus Gui Network Widgets)
if(WGTRAY_BUILD_TESTS)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
endif()

# Reverse-DNS application ID. Used for the desktop file, icon and AppStream names and the window class.
# Change it with packaging/set-app-id.sh (it keeps the Flatpak manifest in step).
set(WGTRAY_APP_ID "io.github.grimsbygeek.Burrow" CACHE STRING "Application ID")
add_compile_definitions(WGTRAY_APP_ID="${WGTRAY_APP_ID}")

add_subdirectory(src)
if(WGTRAY_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
