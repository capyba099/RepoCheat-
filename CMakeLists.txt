cmake_minimum_required(VERSION 3.16)
project(csdecomp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(csdecomp
    src/main.cpp
    src/binary_reader.cpp
    src/pe_reader.cpp
    src/cli_metadata.cpp
    src/il_opcode.cpp
    src/il_reader.cpp
    src/decompiler.cpp
)

target_include_directories(csdecomp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(MSVC)
    target_compile_options(csdecomp PRIVATE /W4 /permissive-)
else()
    target_compile_options(csdecomp PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS csdecomp RUNTIME DESTINATION bin)
