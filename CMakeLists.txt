cmake_minimum_required(VERSION 3.15)

project(m8c LANGUAGES C)

set(CMAKE_C_FLAGS "-O2 -Wall -Wextra")

set(APP_NAME m8c)

set(USE_LIBSERIALPORT 1)

find_package(PkgConfig REQUIRED)
find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3)

pkg_check_modules(SDL3 REQUIRED sdl3)
if (USE_LIBSERIALPORT)
    pkg_check_modules(LIBSERIALPORT REQUIRED libserialport)
    link_directories(${SDL3_LIBRARY_DIRS} ${LIBSERIALPORT_LIBRARY_DIRS})
    add_compile_definitions(USE_LIBSERIALPORT)
endif (USE_LIBSERIALPORT)
if (USE_LIBUSB)
    pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
    link_directories(${SDL3_LIBRARY_DIRS} ${LIBUSB_LIBRARY_DIRS})
    add_compile_definitions(USE_LIBUSB)
endif (USE_LIBUSB)
if (USE_RTMIDI)
    pkg_check_modules(RTMIDI REQUIRED rtmidi)
    link_directories(${SDL3_LIBRARY_DIRS} ${RTMIDI_LIBRARY_DIRS})
    add_compile_definitions(USE_RTMIDI)
endif (USE_RTMIDI)


file(GLOB m8c_SRC "src/*.h" "src/*.c")

set(MACOS_CONTENTS "${CMAKE_CURRENT_SOURCE_DIR}/package/macos/m8c.app/Contents")

set(APP_ICON ${MACOS_CONTENTS}/Resources/m8c.icns)
set_source_files_properties(${APP_ICON} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")

add_executable(${APP_NAME} WIN32 MACOSX_BUNDLE ${APP_ICON} ${m8c_SRC})

if (USE_LIBSERIALPORT)
    target_link_libraries(${APP_NAME} ${SDL3_LIBRARIES} ${LIBSERIALPORT_LIBRARIES})
    target_include_directories(${APP_NAME} PUBLIC ${SDL3_INCLUDE_DIRS} ${LIBSERIALPORT_INCLUDE_DIRS})
    target_compile_options(${APP_NAME} PUBLIC ${SDL3_CFLAGS_OTHER} ${LIBSERIALPORT_CFLAGS_OTHER})
endif ()

if (USE_LIBUSB)
    target_link_libraries(${APP_NAME} ${SDL3_LIBRARIES} ${LIBUSB_LIBRARIES})
    target_include_directories(${APP_NAME} PUBLIC ${SDL3_INCLUDE_DIRS} ${LIBUSB_INCLUDE_DIRS})
    target_compile_options(${APP_NAME} PUBLIC ${SDL3_CFLAGS_OTHER} ${LIBUSB_CFLAGS_OTHER})
endif ()

if (USE_RTMIDI)
    target_link_libraries(${APP_NAME} ${SDL3_LIBRARIES} ${RTMIDI_LIBRARIES})
    target_include_directories(${APP_NAME} PUBLIC ${SDL3_INCLUDE_DIRS} ${RTMIDI_INCLUDE_DIRS})
    target_compile_options(${APP_NAME} PUBLIC ${SDL3_CFLAGS_OTHER} ${RTMIDI_CFLAGS_OTHER})
endif ()

if (APPLE)
    # Destination paths below are relative to ${CMAKE_INSTALL_PREFIX}
    install(TARGETS ${APP_NAME}
            BUNDLE DESTINATION . COMPONENT Runtime
            RUNTIME DESTINATION bin COMPONENT Runtime
    )

    set_target_properties(${APP_NAME} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/package/macos/m8c.app/Contents/Info.plist"
            MACOSX_BUNDLE_BUNDLE_NAME "m8c"
            MACOSX_BUNDLE_BUNDLE_VERSION "1"
            MACOSX_BUNDLE_COPYRIGHT "Copyright © 2021 laamaa. All rights reserved."
            MACOSX_BUNDLE_GUI_IDENTIFIER "fi.laamaa.m8c"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "2.0.0"
            MACOSX_BUNDLE_ICON_FILE "m8c.icns")

    set(APPS "\${CMAKE_INSTALL_PREFIX}/${APP_NAME}.app")

    install(CODE "include(BundleUtilities)
    fixup_bundle(\"${APPS}\" \"\" \"\")
    execute_process(COMMAND codesign --force --deep --sign - \${CMAKE_INSTALL_PREFIX}/${APP_NAME}.app)")
    set(CPACK_GENERATOR "DragNDrop")
    include(CPack)
endif ()

