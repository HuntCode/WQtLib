# WQtLib/cmake/ThirdPartyFFmpeg.cmake
#
# Usage (in root CMakeLists.txt):
#   list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
#   include(ThirdPartyFFmpeg)
#   set(FFMPEG_ROOT "${CMAKE_SOURCE_DIR}/third_party/ffmpeg/win64")
#   wqt_import_ffmpeg("${FFMPEG_ROOT}")

# Find first file matching patterns in a directory (for safety)
function(_wqt_find_first out_var dir)
  set(patterns ${ARGN})
  foreach(p IN LISTS patterns)
    file(GLOB _candidates "${dir}/${p}")
    list(LENGTH _candidates _n)
    if(_n GREATER 0)
      list(GET _candidates 0 _first)
      set(${out_var} "${_first}" PARENT_SCOPE)
      return()
    endif()
  endforeach()
  message(FATAL_ERROR "FFmpeg: cannot find file in ${dir} with patterns: ${patterns}")
endfunction()

# Import one FFmpeg shared library as an IMPORTED target.
# - For MSVC: uses .lib as IMPORTED_IMPLIB
# - Also sets IMPORTED_LOCATION to the dll in bin/ (not strictly required for linking, but useful)
function(_wqt_import_ffmpeg_one target base root)
  set(_inc "${root}/include")
  set(_lib "${root}/lib")
  set(_bin "${root}/bin")

  if(NOT EXISTS "${_inc}")
    message(FATAL_ERROR "FFmpeg include dir not found: ${_inc}")
  endif()
  if(NOT EXISTS "${_lib}")
    message(FATAL_ERROR "FFmpeg lib dir not found: ${_lib}")
  endif()
  if(NOT EXISTS "${_bin}")
    message(FATAL_ERROR "FFmpeg bin dir not found: ${_bin}")
  endif()

  # MSVC import library: avcodec.lib / avutil.lib / swresample.lib ...
  set(_implib "${_lib}/${base}.lib")
  if(NOT EXISTS "${_implib}")
    message(FATAL_ERROR "FFmpeg MSVC import lib not found: ${_implib}")
  endif()

  # DLL name is versioned: avcodec-61.dll / avutil-59.dll / swresample-5.dll ...
  _wqt_find_first(_dll "${_bin}"
    "${base}-*.dll"
    "${base}.dll"
  )

  add_library(${target} SHARED IMPORTED GLOBAL)
  set_target_properties(${target} PROPERTIES
    IMPORTED_IMPLIB "${_implib}"
    IMPORTED_LOCATION "${_dll}"
    INTERFACE_INCLUDE_DIRECTORIES "${_inc}"
  )
endfunction()

# Public entry: creates ffmpeg::avutil ffmpeg::avcodec ffmpeg::swresample (and optionally others)
function(wqt_import_ffmpeg FFMPEG_ROOT)
  if(NOT MSVC)
    message(FATAL_ERROR "This FFmpeg import file is intended for MSVC toolchain. Current toolchain is not MSVC.")
  endif()

  # Minimal set for AAC decode experiments:
  _wqt_import_ffmpeg_one(ffmpeg::avutil     avutil     "${FFMPEG_ROOT}")
  _wqt_import_ffmpeg_one(ffmpeg::avcodec    avcodec    "${FFMPEG_ROOT}")
  _wqt_import_ffmpeg_one(ffmpeg::swresample swresample "${FFMPEG_ROOT}")
  
  # Optional:
  _wqt_import_ffmpeg_one(ffmpeg::avformat  avformat  "${FFMPEG_ROOT}")
  _wqt_import_ffmpeg_one(ffmpeg::avfilter  avfilter  "${FFMPEG_ROOT}")
  _wqt_import_ffmpeg_one(ffmpeg::swscale   swscale   "${FFMPEG_ROOT}")
  _wqt_import_ffmpeg_one(ffmpeg::avdevice  avdevice  "${FFMPEG_ROOT}")
endfunction()
