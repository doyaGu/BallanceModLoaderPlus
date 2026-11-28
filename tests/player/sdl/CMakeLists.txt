cmake_minimum_required(VERSION 3.24)
project(BMLSdlPostProcessRuntimeTest LANGUAGES CXX)

# Build this probe as an independent consumer of an installed SDK.
find_package(BML CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
if(NOT BML_RUNTIME STREQUAL "sdl" OR NOT BML_HOST_LINKAGE STREQUAL "dynamic")
    message(FATAL_ERROR "This test requires the SDK for a dynamic SDL Player")
endif()
bml_add_mod(SdlPostProcessProbe PostProcessProbe.cpp)
target_link_libraries(SdlPostProcessProbe PRIVATE BML::Engine BML::ImGui SDL3::SDL3)

get_filename_component(_workspace "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
find_path(BML_TEST_CKAS_INCLUDE CKAngelScript.h HINTS "${_workspace}/../CKAngelScript/include")
find_path(BML_TEST_AS_INCLUDE angelscript.h HINTS "${_workspace}/../CKAngelScript/deps/angelscript/angelscript/include")
if(BML_TEST_CKAS_INCLUDE AND BML_TEST_AS_INCLUDE)
    target_include_directories(SdlPostProcessProbe PRIVATE "${BML_TEST_CKAS_INCLUDE}" "${BML_TEST_AS_INCLUDE}")
    target_compile_definitions(SdlPostProcessProbe PRIVATE BML_TEST_SCRIPT_REFLECTION=1)
endif()

set(BML_SDL_PLAYER_ROOT "" CACHE PATH "Prepared SDL Player runtime with matching BML and game assets")
option(BML_TEST_CKAS_STARTUP_SELFTESTS "Test CKAngelScript startup self-tests with BML in Player" OFF)
if(BML_SDL_PLAYER_ROOT)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    enable_testing()
    add_test(NAME BMLSdlPostProcessRuntimeTest
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/RunPostProcessTest.py"
            --runtime "${BML_SDL_PLAYER_ROOT}"
            --probe "$<TARGET_FILE:SdlPostProcessProbe>"
            --output "${CMAKE_CURRENT_BINARY_DIR}/runs"
            --repeat 3)
    add_test(NAME BMLSdlScriptRegistrationLifecycleTest
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/RunPostProcessTest.py"
            --runtime "${BML_SDL_PLAYER_ROOT}"
            --probe "$<TARGET_FILE:SdlPostProcessProbe>"
            --output "${CMAKE_CURRENT_BINARY_DIR}/registration-runs"
            --without-script-consumers --repeat 3)
    set_tests_properties(BMLSdlScriptRegistrationLifecycleTest PROPERTIES TIMEOUT 240 LABELS "integration;player;sdl;script")
    file(READ "${BML_SDL_PLAYER_ROOT}/ModLoader/BML.abi.json" _runtime_abi)
    string(JSON _runtime_scripts GET "${_runtime_abi}" scripts)
    if(_runtime_scripts)
        add_test(NAME BMLSdlScriptExamplesTest
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/RunPostProcessTest.py"
                --runtime "${BML_SDL_PLAYER_ROOT}"
                --probe "$<TARGET_FILE:SdlPostProcessProbe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/example-runs"
                --script-examples --repeat 1 --timeout 90)
        set_tests_properties(BMLSdlScriptExamplesTest PROPERTIES TIMEOUT 120 LABELS "integration;player;sdl;script")
        if(BML_TEST_CKAS_STARTUP_SELFTESTS)
            add_test(NAME BMLSdlCkasStartupSelfTest
                COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/RunPostProcessTest.py"
                    --runtime "${BML_SDL_PLAYER_ROOT}"
                    --probe "$<TARGET_FILE:SdlPostProcessProbe>"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/ckas-self-test-runs"
                    --ckas-self-test --without-script-consumers --repeat 1)
            set_tests_properties(BMLSdlCkasStartupSelfTest PROPERTIES
                RUN_SERIAL TRUE TIMEOUT 90 LABELS "integration;player;sdl;script;behavior")
        endif()
    endif()
    set_tests_properties(BMLSdlPostProcessRuntimeTest PROPERTIES TIMEOUT 240 LABELS "integration;player;sdl")
endif()
