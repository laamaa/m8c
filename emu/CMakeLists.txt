add_subdirectory(m8emu)
target_compile_options(capstone PRIVATE "-Wno-error" "-w")

file(GLOB SRC "*.cpp")
add_library(emu ${SRC})
target_link_libraries(emu m8emu)
