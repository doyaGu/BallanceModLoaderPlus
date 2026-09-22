cmake_minimum_required(VERSION 3.15)

project(IVP
        VERSION 0.1.0
        LANGUAGES C CXX
        DESCRIPTION "Retail-compatible IVP API base Mod for Ballance")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(NOT WIN32)
    message(FATAL_ERROR "IVP supports only Windows")
endif()
if(NOT MSVC)
    message(FATAL_ERROR
            "IVP requires an MSVC-compatible Windows ABI (MSVC or clang-cl)")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR
            "IVP is ABI-locked to Ballance's 32-bit physics_RT.dll; configure with -A Win32")
endif()

# The BML+ root provides BML::BML when IVP builds in-tree. Retain the
# standalone path for IVP SDK builds against an installed BML+ SDK.
if(NOT TARGET BML::BML)
    find_package(BML CONFIG REQUIRED)
endif()
if(NOT COMMAND bml_add_mod)
    message(FATAL_ERROR
            "IVP requires BML+ with bml_add_mod")
endif()

if(TARGET BML)
    set(IVP_BML_INCLUDE_DIR "${BML_INCLUDE_DIR}")
else()
    get_target_property(IVP_BML_INCLUDE_DIRS
            BML::BML INTERFACE_INCLUDE_DIRECTORIES)
    list(GET IVP_BML_INCLUDE_DIRS 0 IVP_BML_INCLUDE_DIR)
endif()
if(NOT EXISTS "${IVP_BML_INCLUDE_DIR}/BML/Interface.h")
    message(FATAL_ERROR
            "BML::BML does not expose the required BML/Interface.h: "
            "${IVP_BML_INCLUDE_DIR}")
endif()

set(IVP_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include")
include("${PROJECT_SOURCE_DIR}/cmake/IVPPublicSurface.cmake")

option(IVP_BUILD_TESTS "Build the IVP ABI and behavior tests" OFF)
option(IVP_BUILD_PLAYER_TESTS "Build the Ballance Player IVP scenario Mods" OFF)
set(IVP_BALLANCE_ROOT "$ENV{BML_BALLANCE_ROOT}" CACHE PATH
        "Ballance installation containing the retail physics_RT.dll")
set(IVP_REFERENCE_ROOT
        "$ENV{IVP_REFERENCE_ROOT}" CACHE PATH
        "Optional absolute path to IVP source used by evidence audits")

set(IVP_RETAIL_TESTS_DEFAULT OFF)
if(EXISTS "${IVP_BALLANCE_ROOT}/BuildingBlocks/physics_RT.dll")
    set(IVP_RETAIL_TESTS_DEFAULT ON)
endif()
option(IVP_BUILD_RETAIL_TESTS "Build tests that load the retail physics_RT.dll"
        ${IVP_RETAIL_TESTS_DEFAULT})
option(IVP_BUILD_EVIDENCE_TESTS "Register the IVP evidence audits" ON)

if(IVP_BUILD_PLAYER_TESTS AND NOT IVP_BUILD_TESTS)
    message(FATAL_ERROR
            "IVP_BUILD_PLAYER_TESTS requires IVP_BUILD_TESTS=ON")
endif()
if(IVP_BUILD_TESTS AND (IVP_BUILD_RETAIL_TESTS OR IVP_BUILD_PLAYER_TESTS) AND
        NOT EXISTS "${IVP_BALLANCE_ROOT}/BuildingBlocks/physics_RT.dll")
    message(FATAL_ERROR
            "Retail and Player tests require the retail DLL at "
            "${IVP_BALLANCE_ROOT}/BuildingBlocks/physics_RT.dll")
endif()

add_subdirectory(src)

if(IVP_BUILD_TESTS)
    if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
        enable_testing()
    endif()
    add_subdirectory(tests)
endif()

configure_package_config_file(
        "${PROJECT_SOURCE_DIR}/cmake/IVPConfig.cmake.in"
        "${PROJECT_BINARY_DIR}/IVPConfig.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/IVP")
write_basic_package_version_file(
        "${PROJECT_BINARY_DIR}/IVPConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY SameMajorVersion)

install(EXPORT IVPTargets
        NAMESPACE IVP::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/IVP")
install(FILES
        "${PROJECT_BINARY_DIR}/IVPConfig.cmake"
        "${PROJECT_BINARY_DIR}/IVPConfigVersion.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/IVP")

foreach(IVP_PUBLIC_HEADER IN LISTS IVP_PUBLIC_HEADERS)
    file(RELATIVE_PATH IVP_PUBLIC_HEADER_RELATIVE
            "${IVP_INCLUDE_DIR}" "${IVP_PUBLIC_HEADER}")
    get_filename_component(IVP_PUBLIC_HEADER_DIRECTORY
            "${IVP_PUBLIC_HEADER_RELATIVE}" DIRECTORY)
    install(FILES "${IVP_PUBLIC_HEADER}"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${IVP_PUBLIC_HEADER_DIRECTORY}")
endforeach()

install(DIRECTORY docs/
        DESTINATION "${CMAKE_INSTALL_DATADIR}/IVP/docs"
        FILES_MATCHING PATTERN "*.md")
install(FILES README.md DESTINATION "${CMAKE_INSTALL_DATADIR}/IVP")
