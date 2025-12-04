# apps/qtgui/CMakeLists.txt

set(QTGUI_PROJECT_NAME "QtGui")
project(${QTGUI_PROJECT_NAME} VERSION 0.1 LANGUAGES C CXX)

# Qt 自动处理 moc/uic/rcc
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(QTGUI_SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/mainwindow.ui
)
source_group(src FILES ${QTGUI_SOURCES})

set(PROJECT_SOURCES
    ${QTGUI_SOURCES}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(QtGui
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(QtGui SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(QtGui
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(QtGui PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

# 保留你原来这堆属性 & install & qt_finalize_executable
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.QtGui)
endif()
set_target_properties(QtGui PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS QtGui
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(QtGui)
endif()
