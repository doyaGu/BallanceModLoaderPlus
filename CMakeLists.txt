cmake_minimum_required(VERSION 3.14)

project(BallanceModLoaderPlus
        VERSION 0.4.0
        LANGUAGES CXX
        DESCRIPTION "Renovated Mod Loader for Ballance"
        HOMEPAGE_URL https://github.com/doyaGu/BallanceModLoaderPlus)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

# Add path for custom modules
list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(GenerateExportHeader)
include(BMLMod OPTIONAL RESULT_VARIABLE BMLMOD_MODULE)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Use folders to organize targets in an IDE
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMakeTargets")

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

option(BML_BUILD_TESTS "Build the BML test suite" ON)
option(BML_BUILD_BENCHMARKS "Build the BML benchmark suite" ${WIN32})
option(BML_BUILD_MODULES "Build the standard BML modules" ${WIN32})
option(BML_BUILD_EXAMPLES "Build the BML example modules" ${WIN32})
option(BML_BUILD_ENGINE_BRIDGE "Build the Windows engine integration target (ModLoader)" ${WIN32})

if (NOT WIN32)
    message(STATUS "Configuring cross-platform Core build (legacy runtime disabled).")
    set(BML_BUILD_BENCHMARKS OFF CACHE BOOL "Build the BML benchmark suite" FORCE)
    set(BML_BUILD_MODULES OFF CACHE BOOL "Build the standard BML modules" FORCE)
    set(BML_BUILD_EXAMPLES OFF CACHE BOOL "Build the BML example modules" FORCE)
    set(BML_BUILD_ENGINE_BRIDGE OFF CACHE BOOL "Build the Windows engine integration target (ModLoader)" FORCE)
endif ()

if ((BML_BUILD_MODULES OR BML_BUILD_EXAMPLES) AND NOT BMLMOD_MODULE)
    message(WARNING "cmake/BMLMod.cmake not found; disabling modules/examples.")
    set(BML_BUILD_MODULES OFF CACHE BOOL "Build the standard BML modules" FORCE)
    set(BML_BUILD_EXAMPLES OFF CACHE BOOL "Build the BML example modules" FORCE)
endif ()

set(BML_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include" CACHE INTERNAL "")
set(BML_SOURCE_DIR "${PROJECT_SOURCE_DIR}/src" CACHE INTERNAL "")

if (BML_BUILD_ENGINE_BRIDGE OR BML_BUILD_MODULES OR BML_BUILD_EXAMPLES)
    find_package(VirtoolsSDK REQUIRED)
endif ()

add_subdirectory(deps)

if (BML_BUILD_MODULES)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    file(GLOB BML_IMGUI_GENERATOR_INPUTS CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/deps/cimgui/imgui/*.h"
        "${PROJECT_SOURCE_DIR}/deps/cimgui/imgui/*.hpp"
    )

    list(APPEND BML_IMGUI_GENERATOR_INPUTS
        "${PROJECT_SOURCE_DIR}/deps/cimgui/generator/generator.lua"
        "${PROJECT_SOURCE_DIR}/deps/cimgui/generator/cpp2ffi.lua"
        "${PROJECT_SOURCE_DIR}/deps/cimgui/generator/config_generator.lua"
        "${LUAJIT_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/src/Utils/imconfig.h"
        "${PROJECT_SOURCE_DIR}/tools/generate_bml_imgui_api.py"
    )

    set(BML_IMGUI_API_STAMP
        "${CMAKE_CURRENT_BINARY_DIR}/generate_bml_imgui_api.stamp"
    )

    add_custom_command(
        OUTPUT "${BML_IMGUI_API_STAMP}"
        COMMAND ${Python3_EXECUTABLE}
                "${PROJECT_SOURCE_DIR}/tools/generate_bml_imgui_api.py"
                --repo-root "${PROJECT_SOURCE_DIR}"
                --luajit "${LUAJIT_EXECUTABLE}"
        COMMAND ${CMAKE_COMMAND} -E touch "${BML_IMGUI_API_STAMP}"
        DEPENDS ${BML_IMGUI_GENERATOR_INPUTS}
        VERBATIM
    )

    add_custom_target(generate_bml_imgui_api ALL DEPENDS "${BML_IMGUI_API_STAMP}")
    if (TARGET ${LUAJIT_TARGET})
        add_dependencies(generate_bml_imgui_api ${LUAJIT_TARGET})
    endif ()
endif ()

add_subdirectory(src)
if (BML_BUILD_MODULES)
    add_subdirectory(modules)
endif ()
if (BML_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()
if (BML_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif ()
if (BML_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif ()

# =============================================================================
# SDK Install Rules
# =============================================================================

# Public headers
install(DIRECTORY include/
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT sdk
    FILES_MATCHING
        PATTERN "*.h"
        PATTERN "*.hpp"
        PATTERN "*.inc"
        PATTERN "*.inl"
)

# CMake modules for mod authors
install(FILES cmake/BMLMod.cmake
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/BML"
    COMPONENT sdk
)

# Package config for find_package(BML)
set(BML_INSTALL_INCLUDE_DIR "${CMAKE_INSTALL_INCLUDEDIR}")
include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/BMLConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/BMLConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/BML"
    PATH_VARS BML_INSTALL_INCLUDE_DIR
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/BMLConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/BMLConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/BMLConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/BML"
    COMPONENT sdk
)

# Templates and examples for mod authors
install(DIRECTORY templates/
    DESTINATION "${CMAKE_INSTALL_DATADIR}/BML/templates"
    COMPONENT sdk
)
install(DIRECTORY examples/
    DESTINATION "${CMAKE_INSTALL_DATADIR}/BML/examples"
    COMPONENT sdk
    PATTERN "CMakeFiles" EXCLUDE
    PATTERN "*.dir" EXCLUDE
)

# Mod manifest schema doc
install(FILES docs/mod-manifest-schema.md
    DESTINATION "${CMAKE_INSTALL_DATADIR}/BML/docs"
    COMPONENT sdk
)
