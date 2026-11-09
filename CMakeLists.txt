# QFontIcon: QIcons from icon fonts such as Font Awesome, for Qt 6 (and Qt 5).
#
# In your project:
#   add_subdirectory(QFontIcon-repo)              # or FetchContent
#   target_link_libraries(myapp PRIVATE QFontIcon::QFontIcon)
#
# Standalone, with the example:
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=<Qt> && cmake --build build
cmake_minimum_required(VERSION 3.16)
project(QFontIcon VERSION 2.0 LANGUAGES CXX)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

# The library: QFontIcon/qfonticon.h + .cpp
add_library(QFontIcon STATIC
    QFontIcon/qfonticon.cpp
    QFontIcon/qfonticon.h)
add_library(QFontIcon::QFontIcon ALIAS QFontIcon)
set_target_properties(QFontIcon PROPERTIES AUTOMOC ON POSITION_INDEPENDENT_CODE ON)
target_compile_features(QFontIcon PUBLIC cxx_std_17)
target_include_directories(QFontIcon PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/QFontIcon")
target_link_libraries(QFontIcon PUBLIC Qt${QT_VERSION_MAJOR}::Widgets)

# The example: Font Awesome 5 icons on buttons (on by default only when this
# is the top-level project)
if(PROJECT_IS_TOP_LEVEL OR CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(_example_default ON)
else()
    set(_example_default OFF)
endif()
option(QFONTICON_BUILD_EXAMPLE "Build the QFontIcon example" ${_example_default})
if(QFONTICON_BUILD_EXAMPLE)
    add_executable(QFontIconExample main.cpp resource.qrc)
    set_target_properties(QFontIconExample PROPERTIES AUTORCC ON)
    target_link_libraries(QFontIconExample PRIVATE QFontIcon::QFontIcon)
endif()
