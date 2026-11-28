cmake_minimum_required(VERSION 3.25)
project(VirtoolsPlayerProbe LANGUAGES CXX)
find_package(BML CONFIG REQUIRED)
if(NOT BML_RUNTIME STREQUAL "virtools" OR NOT BML_TARGET_ARCH STREQUAL "x86")
    message(FATAL_ERROR "Use the extracted Virtools Win32 BML SDK")
endif()
bml_add_mod(Win32Probe PlayerProbe.cpp)
target_link_libraries(Win32Probe PRIVATE BML::Engine)
bml_install_mod(Win32Probe)

add_library(VirtoolsPrototypeFixture SHARED PrototypeFixture.cpp)
target_link_libraries(VirtoolsPrototypeFixture PRIVATE BML::Engine)
install(TARGETS VirtoolsPrototypeFixture RUNTIME DESTINATION ../BuildingBlocks)

option(BML_TEST_VIRTOOLS_IME "Build the foreground Microsoft Pinyin acceptance Mod" OFF)
if(BML_TEST_VIRTOOLS_IME)
    bml_add_mod(VirtoolsImeProbe ImeProbe.cpp)
    target_link_libraries(VirtoolsImeProbe PRIVATE BML::Engine BML::ImGui ole32 uuid user32 comctl32)
    bml_install_mod(VirtoolsImeProbe)
endif()

set(BML_VIRTOOLS_PLAYER_ROOT "" CACHE PATH "Prepared retail Win32 Player runtime")
if(BML_VIRTOOLS_PLAYER_ROOT)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    enable_testing()
    add_test(NAME BMLVirtoolsProjectionTest
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/RunProjectionTest.py"
            --runtime "${BML_VIRTOOLS_PLAYER_ROOT}"
            --probe "$<TARGET_FILE:Win32Probe>"
            --prototype "$<TARGET_FILE:VirtoolsPrototypeFixture>"
            --output "${CMAKE_CURRENT_BINARY_DIR}/projection-runs")
    set_tests_properties(BMLVirtoolsProjectionTest PROPERTIES
        RUN_SERIAL TRUE TIMEOUT 120 LABELS "integration;player;virtools")
endif()
