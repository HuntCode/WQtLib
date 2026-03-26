cmake_minimum_required(VERSION 3.16)

project(esserver_test LANGUAGES CXX)

add_executable(esserver_test
    main.cpp
)

target_link_libraries(esserver_test
    PRIVATE
        esserver
)

target_compile_features(esserver_test PRIVATE cxx_std_17)