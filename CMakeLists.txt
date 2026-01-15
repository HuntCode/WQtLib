cmake_minimum_required(VERSION 3.16)

project(WQtLib VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets)

# ===== FFmpeg (prebuilt) import =====
# Make CMake find modules under WQtLib/cmake/
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(ThirdPartyFFmpeg)

# FFmpeg root directory
set(FFMPEG_ROOT "${CMAKE_SOURCE_DIR}/third_party/ffmpeg/win64")

# Create imported targets: ffmpeg::avutil ffmpeg::avcodec ffmpeg::swresample
wqt_import_ffmpeg("${FFMPEG_ROOT}")
# ====================================

# 1) 先把第三方库加进来
add_subdirectory(third_party)

# 2) 再编自己的库和应用
add_subdirectory(libs)
add_subdirectory(apps/qtgui)
add_subdirectory(apps/ffmpeg_test)
