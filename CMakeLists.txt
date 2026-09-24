cmake_minimum_required(VERSION 3.15)
project(OllamaSuperAgent VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -static -static-libgcc -static-libstdc++")
    add_definitions(-DWIN32_LEAN_AND_MEAN)
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -pthread")
endif()

# Sources
set(SOURCES
    src/main.cpp
)

# Include
include_directories(src)

add_executable(ollama-agent ${SOURCES})

# Platform libs
if (WIN32)
    target_link_libraries(ollama-agent winhttp ws2_32)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(ollama-agent Threads::Threads)
    # Try to find filesystem for older GCC
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
        target_link_libraries(ollama-agent stdc++fs)
    endif()
endif()

# Install
install(TARGETS ollama-agent DESTINATION bin)

# For Windows exe naming
if (WIN32)
    set_target_properties(ollama-agent PROPERTIES SUFFIX ".exe")
endif()
