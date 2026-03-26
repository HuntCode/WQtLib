cmake_minimum_required(VERSION 3.16)

project(esserver LANGUAGES CXX)

add_library(esserver STATIC
    src/ESPortManager.cpp
    src/ESServer.cpp
    src/ESSession.cpp
    src/ESUtils.cpp
)

target_include_directories(esserver
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(esserver PUBLIC cxx_std_17)

# 先把 libhv 接进来，后面真正用到网络代码时直接可用
find_package(libhv CONFIG QUIET)

if(TARGET hv_static)
    target_link_libraries(esserver PRIVATE hv_static)
elseif(TARGET hv)
    target_link_libraries(esserver PRIVATE hv)
else()
    find_library(LIBHV_LIBRARY NAMES hv hv_static)
    if(LIBHV_LIBRARY)
        target_link_libraries(esserver PRIVATE ${LIBHV_LIBRARY})
    else()
        message(WARNING "libhv was not resolved yet. Build can continue for the current skeleton, but networking code will need libhv linkage later.")
    endif()
endif()