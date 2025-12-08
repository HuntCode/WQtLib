set(LIBS_BLE_SOURCES
    src/wqt_ble.h
    src/wqt_ble.c
)
source_group(src FILES ${LIBS_BLE_SOURCES})

set(PROJECT_SOURCES
    ${LIBS_BLE_SOURCES}
)

add_library(wqt_ble STATIC
    ${PROJECT_SOURCES}
)

# 让别人只需要 #include "wqt_dial_server.h" 就行
target_include_directories(wqt_ble
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

