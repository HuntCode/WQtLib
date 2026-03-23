cmake_minimum_required(VERSION 3.21)

project(EshareSourceSelfTest  LANGUAGES C CXX)

find_package(Qt6 REQUIRED COMPONENTS Core Network)
find_package(unofficial-libplist CONFIG REQUIRED)

add_executable(eshare_source_self_test
    self_test/eshare_source_self_test/EshareSessionSelfTest.cpp

    source/session/EshareSessionClient.cpp

    source/control/Eshare8700ProbeClient.cpp
    source/control/Eshare57395HeartbeatClient.cpp
    source/control/Eshare8121CommandClient.cpp
    source/control/Eshare8600CameraClient.cpp
    source/control/Eshare51040RtspClient.cpp

    protocol/EshareRtspLiteMessage.cpp

    parser/EshareJsonLineCodec.cpp
    parser/EsharePlistExtract.cpp

    source/media/EshareVideoPacketizer.cpp
    source/media/EshareH264AnnexB.cpp
    source/media/EshareH264FileMirrorSender.cpp
)

target_include_directories(eshare_source_self_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol
        ${CMAKE_CURRENT_SOURCE_DIR}/parser
        ${CMAKE_CURRENT_SOURCE_DIR}/source/session
        ${CMAKE_CURRENT_SOURCE_DIR}/source/control
        ${CMAKE_CURRENT_SOURCE_DIR}/source/media
        ${CMAKE_CURRENT_SOURCE_DIR}/self_test/eshare_source_self_test
)

target_compile_definitions(eshare_source_self_test
    PRIVATE
        ESHARE_TESTDATA_DIR="${CMAKE_SOURCE_DIR}/res/eshare"
)

target_link_libraries(eshare_source_self_test
    PRIVATE
        Qt6::Core
        Qt6::Network
        unofficial::libplist::libplist
)