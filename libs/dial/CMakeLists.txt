set(LIBS_DIAL_SOURCES
    src/wqt_dial_server.cpp
    src/wqt_dial_server.h
)
source_group(src FILES ${LIBS_DIAL_SOURCES})

set(PROJECT_SOURCES
    ${LIBS_DIAL_SOURCES}
)

add_library(wqt_dial STATIC
    ${PROJECT_SOURCES}
)

# 让别人只需要 #include "wqt_dial_server.h" 就行
target_include_directories(wqt_dial
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(wqt_dial
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        dial
)

target_compile_features(wqt_dial PUBLIC cxx_std_17)
