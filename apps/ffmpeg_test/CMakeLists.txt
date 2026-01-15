# apps/ffmpeg_test/CMakeLists.txt
add_executable(ffmpeg_test
    main.cpp
)

target_link_libraries(ffmpeg_test PRIVATE
    ffmpeg::avutil
    ffmpeg::avcodec
    ffmpeg::swresample
)

# 运行时需要 FFmpeg 的 DLL 以及你 bin 里的依赖 DLL（sdl2/libopenh264/mingw runtimes等）
add_custom_command(TARGET ffmpeg_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${FFMPEG_ROOT}/bin"
            "$<TARGET_FILE_DIR:ffmpeg_test>"
)
