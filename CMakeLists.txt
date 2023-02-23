cmake_minimum_required(VERSION 3.15)

project(m8c C)

find_package(PkgConfig REQUIRED)

pkg_check_modules(SDL2 REQUIRED sdl2)
if (USE_LIBUSB)
    pkg_check_modules(LIBUSB REQUIRED libusb)
    link_directories(${SDL2_LIBRARY_DIRS} ${LIBUSB_LIBRARY_DIRS})
    add_compile_definitions(USE_LIBUSB)
else ()
    pkg_check_modules(LIBSERIALPORT REQUIRED libserialport)
    link_directories(${SDL2_LIBRARY_DIRS} ${LIBSERIALPORT_LIBRARY_DIRS})
endif (USE_LIBUSB)

string(CONCAT FILE_CONTENT "#include <SDL.h>")

file(READ inline_font.h INLINE_FONT_HEADER)
string(CONCAT FILE_CONTENT ${FILE_CONTENT} "${INLINE_FONT_HEADER}")

file(STRINGS inprint2.c INPRINT_C)

unset(ModifiedContents)
foreach(Line ${INPRINT_C})
    if(NOT "${Line}" MATCHES "^#include")
        set(ModifiedContents "${ModifiedContents}${Line}\n")
    endif()
endforeach()

string(CONCAT FILE_CONTENT "${FILE_CONTENT}" "${ModifiedContents}")

file(WRITE font.c "${FILE_CONTENT}")

file(GLOB m8c_SRC "*.h" "*.c")

list(FILTER m8c_SRC EXCLUDE REGEX ".*inprint2\\.c$")

add_executable(m8c ${m8c_SRC})

target_link_libraries(m8c ${SDL2_LIBRARIES})
target_include_directories(m8c PUBLIC ${SDL2_INCLUDE_DIRS})
target_compile_options(m8c PUBLIC ${SDL2_CFLAGS_OTHER})

if (USE_LIBUSB)
    target_link_libraries(m8c ${LIBUSB_LIBRARIES})
    target_include_directories(m8c PUBLIC ${LIBUSB_INCLUDE_DIRS})
    target_compile_options(m8c PUBLIC ${LIBUSB_CFLAGS_OTHER})
else ()
    target_link_libraries(m8c ${LIBSERIALPORT_LIBRARIES})
    target_include_directories(m8c PUBLIC ${LIBSERIALPORT_INCLUDE_DIRS})
    target_compile_options(m8c PUBLIC ${LIBSERIALPORT_CFLAGS_OTHER})
endif ()

