cmake_minimum_required(VERSION 3.15)

project(m8c LANGUAGES C)

set(APP_NAME m8c)

find_package(PkgConfig REQUIRED)

pkg_check_modules(SDL2 REQUIRED sdl2)
if (USE_LIBUSB)
    pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
    link_directories(${SDL2_LIBRARY_DIRS} ${LIBUSB_LIBRARY_DIRS})
    add_compile_definitions(USE_LIBUSB)
else ()
    pkg_check_modules(LIBSERIALPORT REQUIRED libserialport)
    link_directories(${SDL2_LIBRARY_DIRS} ${LIBSERIALPORT_LIBRARY_DIRS})
endif (USE_LIBUSB)

file(GLOB m8c_SRC "*.h" "*.c")

set(MACOS_CONTENTS "${CMAKE_CURRENT_SOURCE_DIR}/macos/m8c.app/Contents")

set(APP_ICON ${MACOS_CONTENTS}/Resources/m8c.icns)
set_source_files_properties(${APP_ICON} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")

add_executable(${APP_NAME} WIN32 MACOSX_BUNDLE ${APP_ICON} ${m8c_SRC})

if (USE_LIBUSB)
    target_link_libraries(${APP_NAME} ${SDL2_LIBRARIES} ${LIBUSB_LIBRARIES})
    target_include_directories(${APP_NAME} PUBLIC ${SDL2_INCLUDE_DIRS} ${LIBUSB_INCLUDE_DIRS})
    target_compile_options(${APP_NAME} PUBLIC ${SDL2_CFLAGS_OTHER} ${LIBUSB_CFLAGS_OTHER})
else ()
    target_link_libraries(${APP_NAME} ${SDL2_LIBRARIES} ${LIBSERIALPORT_LIBRARIES})
    target_include_directories(${APP_NAME} PUBLIC ${SDL2_INCLUDE_DIRS} ${LIBSERIALPORT_INCLUDE_DIRS})
    target_compile_options(${APP_NAME} PUBLIC ${SDL2_CFLAGS_OTHER} ${LIBSERIALPORT_CFLAGS_OTHER})
endif ()

if (APPLE)
    # Destination paths below are relative to ${CMAKE_INSTALL_PREFIX}
    install(TARGETS ${APP_NAME}
            BUNDLE DESTINATION . COMPONENT Runtime
            RUNTIME DESTINATION bin COMPONENT Runtime
            )

    set_target_properties(${APP_NAME} PROPERTIES
            MACOSX_BUNDLE_BUNDLE_NAME "m8c"
            MACOSX_BUNDLE_BUNDLE_VERSION "1"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.laamaa.m8c"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
            MACOSX_BUNDLE_COPYRIGHT "Copyright © 2021 laamaa. All rights reserved."
            MACOSX_BUNDLE_ICON_FILE "m8c.icns")

    set(APPS "\${CMAKE_INSTALL_PREFIX}/${APP_NAME}.app")

    install(CODE "include(BundleUtilities)
    fixup_bundle(\"${APPS}\" \"\" \"\")")
endif ()

