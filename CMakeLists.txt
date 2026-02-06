cmake_minimum_required(VERSION 3.10)

# Project name and language
project(GPR_CPU_Emulator LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optional: Build type
# TODO: Release but there is no .json file assoicated with it to set release or debug flags
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# NOTE: this is okay for now, but good tip
# Add executable
# This is okay for now though but I would explore static libs because everytime you change
# a single src file you have to recompile the entire binary. You can take advantage of
# static libs my compiling each src->.o then just link it to the final build that way if
# you need to edit one src the whole binary does not have to be rebuilt only that src file 
# and the obj associated with it
add_executable(gpr_emulator
    main.cpp
    cpu/gpr_cpu.cpp
    assembler.cpp
)

# Include current directory for headers
target_include_directories(gpr_emulator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Optional: Enable warnings
if(MSVC)
    target_compile_options(gpr_emulator PRIVATE /W4 /permissive-)
else()
    target_compile_options(gpr_emulator PRIVATE -Wall -Wextra -pedantic)
endif()

#NOTES:
 # Does not build it seems to be there is a missing file that was once there gpr_cpu.cpp
 # ERROR: no srcs given to target: gpr emulator
 # Correction: Path needs to be fixed
