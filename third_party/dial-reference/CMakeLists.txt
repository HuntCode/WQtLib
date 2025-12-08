# third_party/dial-reference/CMakeLists.txt

# 只处理 server 目录，先不管 client
set(DIALSERVER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/server)

set(DIALSERVER_SOURCES
    ${DIALSERVER_SRC_DIR}/dial_data.c
    ${DIALSERVER_SRC_DIR}/dial_data.h
    ${DIALSERVER_SRC_DIR}/dial_options.h
    ${DIALSERVER_SRC_DIR}/dial_server.c
    ${DIALSERVER_SRC_DIR}/dial_server.h
    ${DIALSERVER_SRC_DIR}/main.c
    ${DIALSERVER_SRC_DIR}/mongoose.c
    ${DIALSERVER_SRC_DIR}/mongoose.h
    ${DIALSERVER_SRC_DIR}/nf_callbacks.c
    ${DIALSERVER_SRC_DIR}/nf_callbacks.h
    ${DIALSERVER_SRC_DIR}/quick_ssdp.c
    ${DIALSERVER_SRC_DIR}/system_callbacks.c
    ${DIALSERVER_SRC_DIR}/system_callbacks.h
    ${DIALSERVER_SRC_DIR}/url_lib.c
    ${DIALSERVER_SRC_DIR}/url_lib.h
)
source_group(src FILES ${DIALSERVER_SOURCES})

add_executable(dialserver
    ${DIALSERVER_SOURCES}
)

target_include_directories(dialserver
    PRIVATE
        ${DIALSERVER_SRC_DIR}
)

# C 标准
set_property(TARGET dialserver PROPERTY C_STANDARD 99)
set_property(TARGET dialserver PROPERTY C_STANDARD_REQUIRED ON)

# 常用编译选项
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(dialserver PRIVATE
        -Wall
        -g
        -fPIC
        # -Werror    # 先别开，等移植稳定了再说
    )
elseif (MSVC)
    # Windows / MSVC 下建议关掉一些烦人的 CRT 警告
    target_compile_definitions(dialserver PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_DEPRECATE
    )
    # 你也可以在这里加 /W3 之类的 warning level
endif()

# Debug 配置下等价 Makefile 的 -DDEBUG
target_compile_definitions(dialserver PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
)

# 平台相关链接
if (UNIX AND NOT APPLE)
    target_link_libraries(dialserver PRIVATE
        dl
        pthread
    )
elseif (WIN32)
    # 先不链接 pthread，等源码移植完再看是否需要 vcpkg pthread
    # 如果之后决定用 vcpkg pthread，可以改成：
    #   find_package(pthreads CONFIG REQUIRED)
    #   target_link_libraries(dialserver PRIVATE pthreads::pthreads)
endif()

