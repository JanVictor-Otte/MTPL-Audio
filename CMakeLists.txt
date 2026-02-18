cmake_minimum_required(VERSION 3.15)
project(MTPL-Audio VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add MTPL from submodule
add_subdirectory(lib/MTPL)

# MTPL-Audio library
add_library(mtpl_audio INTERFACE)
target_include_directories(mtpl_audio INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(mtpl_audio INTERFACE mtpl)

# macOS/iOS frameworks
if(APPLE)
    target_link_libraries(mtpl_audio INTERFACE
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework CoreFoundation"
    )
endif()

# Tests
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    add_executable(test_import tests/test_import.cpp)
    target_link_libraries(test_import mtpl_audio)
endif()