add_subdirectory(m8emu)
target_compile_options(capstone PRIVATE "-Wno-error" "-w")

file(GLOB SRC "*.cpp")
add_library(emu ${SRC})
target_link_libraries(emu PUBLIC m8emu)
target_include_directories(emu PRIVATE ${SDL2_INCLUDE_DIRS})
