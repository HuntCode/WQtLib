cmake_minimum_required(VERSION 3.16)
project(esserver LANGUAGES CXX)

add_library(esserver STATIC
    src/ESPortManager.cpp
    src/ESServer.cpp
    src/ESSession.cpp
    src/ESUtils.cpp
    src/ESRtspLite.cpp
    src/ESVideoDepacketizer.cpp
    src/ESAudioDatagramParser.cpp
    src/ESAudioRtpParser.cpp
)

target_include_directories(esserver
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(esserver PUBLIC cxx_std_17)

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

find_package(unofficial-libplist CONFIG QUIET)
if(TARGET unofficial::libplist::libplist)
    target_link_libraries(esserver PRIVATE unofficial::libplist::libplist)
else()
    find_path(LIBPLIST_INCLUDE_DIR NAMES plist/plist.h)
    find_library(LIBPLIST_LIBRARY NAMES plist-2.0 plist)
    if(LIBPLIST_INCLUDE_DIR AND LIBPLIST_LIBRARY)
        target_include_directories(esserver PRIVATE ${LIBPLIST_INCLUDE_DIR})
        target_link_libraries(esserver PRIVATE ${LIBPLIST_LIBRARY})
    else()
        message(FATAL_ERROR "libplist is required for 51040 SETUP responses")
    endif()
endif()