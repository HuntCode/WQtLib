cmake_minimum_required(VERSION 3.21)

project(EshareSessionSelfTest LANGUAGES C CXX)

find_package(Qt6 REQUIRED COMPONENTS Core Network)
find_package(unofficial-libplist CONFIG REQUIRED)

add_executable(eshare_session_self_test
    src/EshareSessionSelfTest.cpp
    src/EshareSessionClient.cpp
    src/EshareRtspLiteMessage.cpp
    src/Eshare8700ProbeClient.cpp
    src/EshareJsonLineCodec.cpp
    src/Eshare57395HeartbeatClient.cpp
    src/Eshare8121CommandClient.cpp
    src/Eshare8600CameraClient.cpp
    src/EsharePlistExtract.cpp
    src/Eshare51040RtspClient.cpp
)

target_include_directories(eshare_session_self_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(eshare_session_self_test
    PRIVATE
        ESHARE_TESTDATA_DIR="${CMAKE_SOURCE_DIR}/res/eshare"
)

target_link_libraries(eshare_session_self_test
    PRIVATE
        Qt6::Core
        Qt6::Network
        unofficial::libplist::libplist
)