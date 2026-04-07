cmake_minimum_required(VERSION 3.15)
project(MTPL-Audio VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Header-only library
add_library(mtpl INTERFACE)
target_include_directories(mtpl INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

add_library(mtpl_audio INTERFACE)
target_include_directories(mtpl_audio INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(mtpl_audio INTERFACE mtpl)

if(APPLE)
    target_link_libraries(mtpl_audio INTERFACE
        "-framework AudioToolbox"
        "-framework CoreAudio"
        "-framework CoreFoundation"
    )
endif()

# Tests (optional, only build if this is the main project)
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)

    function(add_mtpl_test test_name)
        add_executable(${test_name} tests/${test_name}.cpp)
        target_link_libraries(${test_name} PRIVATE mtpl)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endfunction()

    function(add_mtpl_audio_test test_name)
        add_executable(${test_name} tests/${test_name}.cpp)
        target_link_libraries(${test_name} PRIVATE mtpl_audio)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endfunction()

    function(add_mtpl_audio_demo demo_name)
        add_executable(${demo_name} tests/${demo_name}.cpp)
        target_link_libraries(${demo_name} PRIVATE mtpl_audio)
    endfunction()

    add_mtpl_test(test_mtpl_core)
    add_mtpl_test(test_signal_transform)
    add_mtpl_test(test_immutability)
    add_mtpl_test(test_lane_signal_sink)

    add_mtpl_audio_test(test_audio)
    add_mtpl_audio_test(test_import)

    if(APPLE)
        add_mtpl_audio_test(test_core_audio)
        add_mtpl_audio_demo(test_soundscape)
        add_mtpl_audio_demo(test_clickscape)
    endif()
endif()