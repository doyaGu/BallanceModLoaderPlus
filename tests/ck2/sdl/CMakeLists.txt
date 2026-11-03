cmake_minimum_required(VERSION 3.24)
project(BMLSdlEngineTests LANGUAGES C CXX)

# These component tests can run while the complete SDL Loader is gated on
# integration work. They do not load BML or modify a Player installation.
get_filename_component(BML_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(BALLANCED_ROOT "${BML_ROOT}/../Ballanced" CACHE PATH "Ballanced source tree")
set(CKRE_BUILD_TESTS ON CACHE BOOL "" FORCE)
set(CKRE_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(CKRE_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(CKRE_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(CKRE_BUILD_SDL_GPU_RASTERIZER OFF CACHE BOOL "" FORCE)
set(CKRE_INSTALL OFF CACHE BOOL "" FORCE)
set(CK2_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CK2_INSTALL OFF CACHE BOOL "" FORCE)
set(VXMATH_INSTALL OFF CACHE BOOL "" FORCE)
find_package(SDL3 CONFIG REQUIRED)
add_subdirectory("${BALLANCED_ROOT}/Source/RenderEngine" engine)

include(FetchContent)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
set(ZIP_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(ZIP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZIP_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory("${BML_ROOT}/deps/zip" zip)

add_executable(BMLSdlEngineTest
    SdlEngineTest.cpp
    "${BML_ROOT}/src/Hooks/RenderHook.cpp"
    "${BML_ROOT}/src/Behavior/Edit/Journal.cpp"
    "${BML_ROOT}/src/Behavior/Engine/Message.cpp"
    "${BML_ROOT}/src/Behavior/PrototypeCatalog.cpp"
    "${BML_ROOT}/src/Behavior/CKPrototypeSource.cpp"
    "${BML_ROOT}/src/Behavior/Core/Parameter.cpp"
    "${BML_ROOT}/src/Behavior/Core/Value.cpp"
    "${BML_ROOT}/src/UI/imgui_impl_ck2.cpp"
    "${BML_ROOT}/src/UI/ImGuiFileSdl.cpp"
    "${BML_ROOT}/src/Utils/PathUtilsSdl.cpp"
    "${BML_ROOT}/src/Utils/StringUtils.cpp"
    "${BML_ROOT}/src/Platform/FileSystemNative.cpp"
    "${BML_ROOT}/src/Platform/DebugOutput.cpp"
    "${BML_ROOT}/deps/imgui/imgui.cpp"
    "${BML_ROOT}/deps/imgui/imgui_draw.cpp"
    "${BML_ROOT}/deps/imgui/imgui_tables.cpp"
    "${BML_ROOT}/deps/imgui/imgui_widgets.cpp")
target_compile_features(BMLSdlEngineTest PRIVATE cxx_std_20)
target_include_directories(BMLSdlEngineTest PRIVATE
    "${BML_ROOT}/include" "${BML_ROOT}/src" "${BML_ROOT}/src/Utils"
    "${BML_ROOT}/deps/imgui" "${BML_ROOT}/deps/utf8.h"
    "${BALLANCED_ROOT}/Source/RenderEngine/tests")
target_compile_definitions(BMLSdlEngineTest PRIVATE
    BML_RUNTIME_SDL=1 BML_TEST BML_EXPORTS BML_ENABLE_ANGELSCRIPT=0
    IMGUI_USER_CONFIG="BMLImGuiConfig.h")
target_precompile_headers(BMLSdlEngineTest PRIVATE <CKAll.h>)
target_link_libraries(BMLSdlEngineTest PRIVATE CK2_3DStatic ckre_recording_support gtest_main SDL3::SDL3 zip)

# Compile the graph reader and delayed-link adapter against the SDL headers.
add_library(BMLSdlBehaviorCompile OBJECT
    "${BML_ROOT}/src/Behavior/CKGraph.cpp"
    "${BML_ROOT}/src/Behavior/Engine/Access.cpp")
target_compile_features(BMLSdlBehaviorCompile PRIVATE cxx_std_20)
target_include_directories(BMLSdlBehaviorCompile PRIVATE "${BML_ROOT}/include" "${BML_ROOT}/src")
target_compile_definitions(BMLSdlBehaviorCompile PRIVATE BML_RUNTIME_SDL=1 BML_EXPORTS)
target_precompile_headers(BMLSdlBehaviorCompile PRIVATE <CKAll.h>)
target_link_libraries(BMLSdlBehaviorCompile PRIVATE CK2_3DStatic)

enable_testing()
add_test(NAME BMLSdlEngineTest COMMAND BMLSdlEngineTest)

# Input tests exercise the real manager and BML's SDL adapter without a Player.
set(SDLINPUT_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(SDLINPUT_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(SDLINPUT_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory("${BALLANCED_ROOT}/Source/Managers/SdlInputManager" input)
add_executable(BMLSdlInputTest
    SdlInputTest.cpp
    "${BML_ROOT}/src/Hooks/InputHookSdl.cpp")
target_compile_features(BMLSdlInputTest PRIVATE cxx_std_20)
target_include_directories(BMLSdlInputTest PRIVATE
    "${BML_ROOT}/include" "${BML_ROOT}/src"
    "${BALLANCED_ROOT}/Source/Managers/SdlInputManager")
target_compile_definitions(BMLSdlInputTest PRIVATE BML_RUNTIME_SDL=1 BML_EXPORTS)
target_link_libraries(BMLSdlInputTest PRIVATE SdlInputManagerStatic CK2Static gtest_main SDL3::SDL3)
add_test(NAME BMLSdlInputTest COMMAND BMLSdlInputTest)
set_tests_properties(BMLSdlInputTest PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
