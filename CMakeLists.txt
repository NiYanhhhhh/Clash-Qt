cmake_minimum_required(VERSION 3.9)
project(clash_qt)

set(CMAKE_CXX_STANDARD 17)

IF (CMAKE_BUILD_TYPE STREQUAL Debug)
    ADD_DEFINITIONS(-DDEBUG)
ENDIF()

include(FeatureSummary)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Network)
find_package(yaml-cpp CONFIG REQUIRED)


feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)

file(COPY res/default/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/res/)
file(COPY res/bin/clash DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)

include_directories(src)

file(GLOB_RECURSE src_files src/*.h src/*.cpp)

set(res_files res/res.qrc)

add_executable(clash-qt ${src_files} ${res_files})

target_link_libraries(clash-qt
        Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network
        yaml-cpp
        )

    set(CMAKE_INSTALL_PREFIX $ENV{HOME}/.local)
configure_file(res/clash-qt.desktop clash-qt.desktop)
install(TARGETS clash-qt DESTINATION bin)
if(NOT EXISTS "/usr/bin/clash")
    install(PROGRAMS res/bin/clash DESTINATION bin)
endif()
install(FILES res/icon/clash.png DESTINATION share/clash-qt)
install(DIRECTORY res/default/ DESTINATION share/clash-qt)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/clash-qt.desktop" DESTINATION share/applications)
