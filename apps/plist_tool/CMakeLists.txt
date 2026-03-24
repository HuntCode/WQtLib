# apps/plist_tool/CMakeLists.txt

add_executable(plist_tool
    main.cpp
)

find_package(unofficial-libplist CONFIG REQUIRED)

target_link_libraries(plist_tool PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    unofficial::libplist::libplist
)