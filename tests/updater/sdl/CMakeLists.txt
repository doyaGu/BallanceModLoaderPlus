cmake_minimum_required(VERSION 3.25)

# This focused fixture builds the actual updater and utilities without Player
# or an engine checkout. The main BML test suite uses the same targets below.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(BMLUpdaterSdlTests LANGUAGES C CXX)
    get_filename_component(_bml_root "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
    set(PROJECT_SOURCE_DIR "${_bml_root}")
    set(BML_SOURCE_DIR "${_bml_root}/src")
    set(BML_RUNTIME_SDL TRUE)
    add_compile_definitions(BML_RUNTIME_SDL=1)
    set(BML_BUILD_TESTS TRUE)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
    if(WIN32)
        set(BML_TARGET_PLATFORM windows)
        add_compile_definitions(NOMINMAX)
        add_compile_options($<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>)
    elseif(APPLE)
        set(BML_TARGET_PLATFORM macos)
    else()
        set(BML_TARGET_PLATFORM linux)
    endif()
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _bml_processor)
    if(_bml_processor MATCHES "^(aarch64|arm64)$")
        set(BML_TARGET_ARCH arm64)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(BML_TARGET_ARCH x86)
    else()
        set(BML_TARGET_ARCH x64)
    endif()
    find_package(SDL3 3.4.8 CONFIG REQUIRED)
    add_subdirectory("${_bml_root}/deps" deps)
    add_subdirectory("${_bml_root}/src/Utils" utils)
    add_subdirectory("${_bml_root}/tools/updater" updater)
    enable_testing()
    find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
    if(BML_GTEST_SOURCE)
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        add_subdirectory("${BML_GTEST_SOURCE}" googletest)
    else()
        find_package(GTest CONFIG REQUIRED)
    endif()
endif()

add_library(BMLUpdaterDoctorBinaryFixture SHARED DoctorBinaryFixture.cpp)
add_executable(BMLUpdaterPlayerFixture PlayerProcessFixture.cpp)
set_target_properties(BMLUpdaterPlayerFixture PROPERTIES OUTPUT_NAME Player
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fixtures")
add_executable(BMLUpdaterSdlTest UpdaterSdlTest.cpp)
target_link_libraries(BMLUpdaterSdlTest PRIVATE BMLUpdaterTestCore GTest::gtest BMLUpdateSigning)
target_include_directories(BMLUpdaterSdlTest PRIVATE "${BML_SOURCE_DIR}")
add_test(NAME BMLUpdaterSdlTest COMMAND BMLUpdaterSdlTest)
set_tests_properties(BMLUpdaterSdlTest PROPERTIES LABELS "updater;sdl;integration" TIMEOUT 120 RUN_SERIAL TRUE)

set(BML_TEST_RUNTIME_ARCHIVE "" CACHE PATH "Extracted runtime-only archive for updater acceptance")
set(BML_TEST_RELEASE_PLAYER "" CACHE PATH "Complete Player with the extracted runtime overlaid")
set(_bml_production_updater "$<TARGET_FILE:Updater>")
if(BML_TEST_RUNTIME_ARCHIVE AND BML_TEST_RELEASE_PLAYER)
    set(_bml_production_updater "${BML_TEST_RELEASE_PLAYER}/Bin/Updater${CMAKE_EXECUTABLE_SUFFIX}")
    set(_bml_runtime_test_args --runtime-archive "${BML_TEST_RUNTIME_ARCHIVE}"
        --player-runtime "${BML_TEST_RELEASE_PLAYER}")
endif()
add_test(NAME BMLUpdaterSdlIntegrationTest
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/UpdaterIntegrationTest.py"
        --updater "$<TARGET_FILE:UpdaterSdlTestRunner>"
        --production-updater "${_bml_production_updater}"
        --shared-fixture "$<TARGET_FILE:BMLUpdaterDoctorBinaryFixture>"
        --player-fixture "$<TARGET_FILE:BMLUpdaterPlayerFixture>"
        --platform "${BML_TARGET_PLATFORM}" --architecture "${BML_TARGET_ARCH}"
        --packager "${PROJECT_SOURCE_DIR}/scripts/package_sdl_update.py"
        --signer "$<TARGET_FILE:BMLUpdateSigner>" ${_bml_runtime_test_args})
set_tests_properties(BMLUpdaterSdlIntegrationTest PROPERTIES LABELS "updater;sdl;network;integration" TIMEOUT 180 RUN_SERIAL TRUE)
