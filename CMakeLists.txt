cmake_minimum_required(VERSION 3.25)

project(BallanceModLoaderPlus
        VERSION 0.3.14
        LANGUAGES C CXX
        DESCRIPTION "Renovated Mod Loader for Ballance"
        HOMEPAGE_URL https://github.com/doyaGu/BallanceModLoaderPlus)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

# Add path for custom modules
list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(GenerateExportHeader)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Use folders to organize targets in an IDE
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMakeTargets")

include(BMLRuntime)

# Use relative paths
if (WIN32)
    set(CMAKE_SUPPRESS_REGENERATION TRUE)
endif ()

# Define build type if not specified
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Release' as no build type was specified")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the build type (Debug/Release)" FORCE)
    # Define standard build type options for GUI
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif ()

# Set default install path
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/install" CACHE PATH "The root directory of the installation" FORCE)
    message(STATUS "Setting default install directory to ${CMAKE_INSTALL_PREFIX} as no install directory was specified")
endif ()

# Disable msvc unsafe warnings globally
add_compile_definitions(
        $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
        $<$<CXX_COMPILER_ID:MSVC>:_CRT_NONSTDC_NO_WARNINGS>
)

# Disable char8_t for MSVC to maintain compatibility with C++20
add_compile_options(
        $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/Zc:char8_t->
)

include(CMakeDependentOption)

option(BML_BUILD_TESTS "Build the BML test suite" OFF)
option(BML_BUILD_PLATFORM_TESTS "Build filesystem, crypto and module contract tests" OFF)
option(BML_BUILD_BENCHMARKS "Build standalone BML microbenchmarks" OFF)
cmake_dependent_option(
        BML_ENABLE_BEHAVIOR_TEST_INTERFACE
        "Expose the private Behavior interface used by real Player tests"
        OFF
        "BML_BUILD_TESTS"
        OFF
)
cmake_dependent_option(
        BML_BUILD_UI_AUTOMATION
        "Build the ImGui UI automation suite"
        OFF
        "BML_BUILD_TESTS"
        OFF
)
option(BML_BUILD_UPDATER "Build the standalone BML updater" ON)
option(BML_BUILD_MODS "Build the in-tree native Mods" OFF)
option(BML_ENABLE_ANGELSCRIPT "Build AngelScript support (disable for the script-free build)" ON)
set(CKANGELSCRIPT_ROOT "${PROJECT_SOURCE_DIR}/../CKAngelScript" CACHE PATH "Optional CKAngelScript source or install root")
option(BML_BUILD_SCRIPT_RUNTIME "Build the matching SDL CKAngelScript plugin from CKANGELSCRIPT_ROOT" ${BML_RUNTIME_SDL})

set(BML_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include" CACHE INTERNAL "")
set(BML_SOURCE_DIR "${PROJECT_SOURCE_DIR}/src" CACHE INTERNAL "")

# Curated inventories drive source-tree validation, installation, and
# installed-consumer tests in every subdirectory.
include("${PROJECT_SOURCE_DIR}/cmake/BMLBehaviorBlocksPublicSurface.cmake")

bml_find_engine()

if (BML_ENABLE_ANGELSCRIPT)
    if(BML_RUNTIME_SDL AND BML_BUILD_SCRIPT_RUNTIME AND NOT TARGET CKAngelScript)
        if(NOT EXISTS "${CKANGELSCRIPT_ROOT}/CMakeLists.txt")
            message(FATAL_ERROR "BML_BUILD_SCRIPT_RUNTIME requires the CKAngelScript source tree; set CKANGELSCRIPT_ROOT")
        endif()
        # Keep the child's install/test settings scoped. All runtime plugins
        # share these exact CK2/VxMath targets and the same SDL library.
        function(bml_build_script_runtime)
            set(CKAS_RUNTIME sdl)
            set(CKAS_BUILD_CORE_TESTS OFF)
            set(CKAS_INSTALL_DOCS OFF)
            set(SKIP_INSTALL_ALL ON)
            add_subdirectory("${CKANGELSCRIPT_ROOT}" "script-runtime" EXCLUDE_FROM_ALL)
        endfunction()
        bml_build_script_runtime()
    endif()
    find_path(CKANGELSCRIPT_INCLUDE_DIR
            NAMES CKAngelScript.h
            HINTS
            "${CKANGELSCRIPT_ROOT}/include"
            "${CKANGELSCRIPT_ROOT}")
    find_path(CKANGELSCRIPT_ANGELSCRIPT_INCLUDE_DIR
            NAMES angelscript.h
            HINTS
            "${CKANGELSCRIPT_ROOT}/deps/angelscript/angelscript/include"
            "${CKANGELSCRIPT_ROOT}/include"
            "${CKANGELSCRIPT_ROOT}")
    if (CKANGELSCRIPT_INCLUDE_DIR AND CKANGELSCRIPT_ANGELSCRIPT_INCLUDE_DIR)
        file(STRINGS "${CKANGELSCRIPT_INCLUDE_DIR}/CKAngelScript.h"
                CKANGELSCRIPT_API_VERSION_LINE
                REGEX "^#define[ \t]+CKAS_API_VERSION[ \t]+[0-9]+")
        if (NOT CKANGELSCRIPT_API_VERSION_LINE)
            message(FATAL_ERROR "BML AngelScript script support requires CKAngelScript with CKAS_API_VERSION")
        endif ()
        string(REGEX REPLACE ".*CKAS_API_VERSION[ \t]+([0-9]+).*" "\\1"
                CKANGELSCRIPT_API_VERSION "${CKANGELSCRIPT_API_VERSION_LINE}")
        # BML's required public surface landed in API 6. Current CKAngelScript
        # headers may be 7 (async host-call flag only); do not treat the header
        # version as BML's runtime floor.
        if (CKANGELSCRIPT_API_VERSION LESS 6)
            message(FATAL_ERROR "BML AngelScript script support requires CKAngelScript API version 6 or newer")
        endif ()

        file(STRINGS "${CKANGELSCRIPT_INCLUDE_DIR}/CKAngelScript.h"
                CKANGELSCRIPT_REQUIRED_FEATURES
                REGEX "CKAS_FEATURE_(OBJECT_TYPE_NAMESPACE|OBJECT_METHOD_CONTEXT_ACCESS|SCRIPT_ARRAY_ACCESS|SOURCE_SECTIONS|OBJECT_HANDLE_ARGS|MODULE_IMPORTS|MODULE_BYTECODE|MODULE_REPLACE_TRANSACTION|MODULE_GRAPH|MODULE_FINGERPRINT)")
        foreach (required_feature
                CKAS_FEATURE_OBJECT_TYPE_NAMESPACE
                CKAS_FEATURE_OBJECT_METHOD_CONTEXT_ACCESS
                CKAS_FEATURE_SCRIPT_ARRAY_ACCESS
                CKAS_FEATURE_SOURCE_SECTIONS
                CKAS_FEATURE_OBJECT_HANDLE_ARGS
                CKAS_FEATURE_MODULE_IMPORTS
                CKAS_FEATURE_MODULE_BYTECODE
                CKAS_FEATURE_MODULE_REPLACE_TRANSACTION
                CKAS_FEATURE_MODULE_GRAPH
                CKAS_FEATURE_MODULE_FINGERPRINT)
            string(FIND "${CKANGELSCRIPT_REQUIRED_FEATURES}" "${required_feature}" required_feature_index)
            if (required_feature_index EQUAL -1)
                message(FATAL_ERROR "BML AngelScript script support requires CKAngelScript with ${required_feature}")
            endif ()
        endforeach ()
        set(BML_WITH_ANGELSCRIPT TRUE)
        message(STATUS "BML AngelScript script support enabled")
    else ()
        message(FATAL_ERROR "BML AngelScript script support requires CKAngelScript and AngelScript headers")
    endif ()
else ()
    set(BML_WITH_ANGELSCRIPT FALSE)
    message(STATUS "BML AngelScript script support disabled")
endif ()

if (BML_BUILD_TESTS OR BML_BUILD_MODS OR BML_BUILD_PLATFORM_TESTS)
    enable_testing()
endif ()

add_subdirectory(deps)
if (BML_BUILD_UI_AUTOMATION)
    include("${PROJECT_SOURCE_DIR}/cmake/ImGuiTestEngine.cmake")
endif ()
include(BMLAbi)
add_subdirectory(src)
include(BMLRuntimePackage)
if (BML_BUILD_PLATFORM_TESTS OR BML_BUILD_TESTS)
    add_subdirectory(tests/platform)
endif ()
if (BML_BUILD_MODS)
    add_library(BML::BML ALIAS BML)
    include("${PROJECT_SOURCE_DIR}/cmake/BMLMod.cmake")
    add_subdirectory(mods)
endif ()
if (BML_BUILD_UPDATER)
    add_subdirectory(tools/updater)
endif ()

# Relocate and sign installed copies after all runtime targets, including the
# standalone Updater, have been installed.
if(BML_RUNTIME_SDL AND APPLE)
    install(SCRIPT "${PROJECT_SOURCE_DIR}/cmake/RelocateBMLRuntime.cmake"
        COMPONENT BMLRuntime EXCLUDE_FROM_ALL)
endif()
if (BML_BUILD_BENCHMARKS)
    add_subdirectory(tools/benchmarks)
endif ()
if (BML_BUILD_TESTS)
    add_subdirectory(tests)
endif ()
