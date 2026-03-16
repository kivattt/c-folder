cmake_minimum_required(VERSION 3.19)
cmake_policy(SET CMP0074 NEW)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project("ftinspect")
set(FTINSPECT_SRC
    "engine/charmap.cpp"
    "engine/engine.cpp"
    "engine/fontfilemanager.cpp"
    "engine/fontinfo.cpp"
    "engine/fontinfonamesmapping.cpp"
    "engine/mmgx.cpp"
    "engine/paletteinfo.cpp"
    "engine/rendering.cpp"
    "engine/stringrenderer.cpp"

    "glyphcomponents/glyphbitmap.cpp"
    "glyphcomponents/glyphcontinuous.cpp"
    "glyphcomponents/glyphoutline.cpp"
    "glyphcomponents/glyphpointnumbers.cpp"
    "glyphcomponents/glyphpoints.cpp"
    "glyphcomponents/graphicsdefault.cpp"
    "glyphcomponents/grid.cpp"

    "models/customcomboboxmodels.cpp"
    "models/fontinfomodels.cpp"

    "panels/comparator.cpp"
    "panels/continuous.cpp"
    "panels/glyphdetails.cpp"
    "panels/info.cpp"
    "panels/settingpanel.cpp"
    "panels/settingpanelmmgx.cpp"
    "panels/singular.cpp"

    "widgets/charmapcombobox.cpp"
    "widgets/customwidgets.cpp"
    "widgets/fontsizeselector.cpp"
    "widgets/glyphindexselector.cpp"
    "widgets/tripletselector.cpp"

    "ftinspect.cpp"
    "maingui.cpp"
    "uihelper.cpp"
)

set(USE_QT_VERSION 6 CACHE STRING "Qt major version to use (5 or 6 supported).")
if(NOT USE_QT_VERSION MATCHES "^(5|6)$")
    message(FATAL_ERROR "USE_QT_VERSION must be set to either 5 or 6. Found: '${USE_QT_VERSION}'")
endif()
if(USE_QT_VERSION EQUAL 6)
    set(QT_MIN_VERSION "6.8")
elseif(USE_QT_VERSION EQUAL 5)
    set(QT_MIN_VERSION "5.15")
endif()

find_package(Freetype REQUIRED)
find_package(Iconv)
find_package(Qt${USE_QT_VERSION} ${QT_MIN_VERSION} COMPONENTS Widgets REQUIRED)
if(USE_QT_VERSION EQUAL 6)
    qt_standard_project_setup()
    qt_add_executable(ftinspect ${FTINSPECT_SRC})
else()
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)
    add_executable(ftinspect ${FTINSPECT_SRC})
endif()

target_link_libraries(ftinspect PRIVATE
  Freetype::Freetype
  Qt${USE_QT_VERSION}::Core
  Qt${USE_QT_VERSION}::Widgets
)
if(Iconv_FOUND)
  target_link_libraries(ftinspect PRIVATE Iconv::Iconv)
endif()

if (MSVC)
  target_compile_options(ftinspect
    PRIVATE "/W4" "/wd4100" "/utf-8")
else (CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
      OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  target_compile_options(ftinspect
    PRIVATE "-Wall" "-Wextra" "-Wpedantic" "-Wno-unused-parameter")
endif ()
