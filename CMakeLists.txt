cmake_minimum_required(VERSION 3.16)

project(WQtLib VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1) 先把第三方库加进来
add_subdirectory(third_party)

# 2) 再编自己的库和应用
add_subdirectory(libs)
add_subdirectory(apps/qtgui)

