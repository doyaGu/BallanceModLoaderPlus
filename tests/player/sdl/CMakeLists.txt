cmake_minimum_required(VERSION 3.24)
project(BMLSdlPostProcessRuntimeTest LANGUAGES CXX)

# Build this probe as an independent consumer of an installed SDK.
find_package(BML CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
if(NOT BML_RUNTIME STREQUAL "sdl" OR NOT BML_HOST_LINKAGE STREQUAL "dynamic")
    message(FATAL_ERROR "This test requires the SDK for a dynamic SDL Player")
endif()
bml_add_mod(SdlPostProcessProbe PostProcessProbe.cpp)
target_link_libraries(SdlPostProcessProbe PRIVATE SDL3::SDL3)

set(BML_SDL_PLAYER_ROOT "" CACHE PATH "Prepared SDL Player runtime with matching BML and game assets")
if(BML_SDL_PLAYER_ROOT)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    enable_testing()
    add_test(NAME BMLSdlPostProcessRuntimeTest
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/RunPostProcessTest.py"
            --runtime "${BML_SDL_PLAYER_ROOT}"
            --probe "$<TARGET_FILE:SdlPostProcessProbe>"
            --output "${CMAKE_CURRENT_BINARY_DIR}/runs"
            --repeat 3)
    set_tests_properties(BMLSdlPostProcessRuntimeTest PROPERTIES TIMEOUT 240 LABELS "integration;player;sdl")
endif()
