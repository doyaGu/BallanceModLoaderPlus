cmake_minimum_required(VERSION 3.14)

project(BallanceModLoaderPlus
        VERSION 0.3.12
        LANGUAGES CXX
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

if (NOT WIN32)
    message(FATAL_ERROR "Only support Windows.")
endif ()

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
        $<$<CXX_COMPILER_ID:MSVC>:/Zc:char8_t->
)

option(BML_BUILD_TESTS "Build the BML test suite" OFF)
option(BML_BUILD_SCRIPT_TOOLS "Build script mod authoring tools" ON)
option(BML_ENABLE_ANGELSCRIPT "Build optional AngelScript script support when CKAngelScript headers are available" OFF)
set(CKANGELSCRIPT_ROOT "${PROJECT_SOURCE_DIR}/../CKAngelScript" CACHE PATH "Optional CKAngelScript source or install root")

set(BML_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include" CACHE INTERNAL "")
set(BML_SOURCE_DIR "${PROJECT_SOURCE_DIR}/src" CACHE INTERNAL "")

find_package(VirtoolsSDK REQUIRED)

if (BML_ENABLE_ANGELSCRIPT)
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
                CKANGELSCRIPT_ARRAY_FEATURE
                REGEX "CKAS_FEATURE_SCRIPT_ARRAY_ACCESS")
        if (NOT CKANGELSCRIPT_ARRAY_FEATURE)
            message(FATAL_ERROR "BML AngelScript script support requires CKAngelScript with CKAS_FEATURE_SCRIPT_ARRAY_ACCESS")
        endif ()
        set(BML_WITH_ANGELSCRIPT TRUE)
        message(STATUS "BML AngelScript script support enabled")
    else ()
        message(FATAL_ERROR "BML AngelScript script support requires CKAngelScript and AngelScript headers")
    endif ()
else ()
    set(BML_WITH_ANGELSCRIPT FALSE)
    message(STATUS "BML AngelScript script support disabled")
endif ()

add_subdirectory(deps)
add_subdirectory(src)
if (BML_BUILD_SCRIPT_TOOLS)
    add_subdirectory(tools)
endif ()
if (BML_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()
