cmake_minimum_required(VERSION 3.25)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(BMLUiFrameworkTests LANGUAGES CXX)
    enable_testing()
    if(BML_BUILD_UI_AUTOMATION)
        message(FATAL_ERROR "Configure BML_BUILD_UI_AUTOMATION from the BML project root")
    endif()
endif()

set(BML_UI_TEST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(BML_UI_TEST_BINARY_ROOT "${CMAKE_CURRENT_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${BML_UI_TEST_BINARY_ROOT}")
set(BML_BALLANCE_ROOT "$ENV{BML_BALLANCE_ROOT}" CACHE PATH
        "Optional Ballance installation used by real Player UI automation")

if(BML_BUILD_UI_AUTOMATION)
    add_subdirectory(headless)
endif()
add_subdirectory(session)
add_subdirectory(runner)
