cmake_minimum_required(VERSION 3.15)

project(BML_ObjectLoad VERSION 0.4.0 LANGUAGES CXX)

# C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(BML_OBJECTLOAD_SOURCES
    src/ObjectLoadMod.cpp
    src/ObjectLoadHook.cpp
)

set(BML_OBJECTLOAD_HEADERS
    src/ObjectLoadHook.h
)

# Create shared library (DLL)
add_library(BML_ObjectLoad SHARED
    ${BML_OBJECTLOAD_SOURCES}
    ${BML_OBJECTLOAD_HEADERS}
)

# Find Virtools SDK
find_package(VirtoolsSDK REQUIRED)

# Include directories
target_include_directories(BML_ObjectLoad PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/BML
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/Utils
    ${CMAKE_SOURCE_DIR}/deps
    ${VIRTOOLS_SDK_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(BML_ObjectLoad PRIVATE
    VirtoolsSDK::CK2
    VirtoolsSDK::VxMath
    BMLUtils
)

# Output configuration
set_target_properties(BML_ObjectLoad PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Mods/BML_ObjectLoad
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Mods/BML_ObjectLoad
)

# Copy mod.toml to output directory
add_custom_command(TARGET BML_ObjectLoad POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/mod.toml
        $<TARGET_FILE_DIR:BML_ObjectLoad>/mod.toml
)

# Install target
install(TARGETS BML_ObjectLoad
    RUNTIME DESTINATION Mods/BML_ObjectLoad
    LIBRARY DESTINATION Mods/BML_ObjectLoad
)

install(FILES mod.toml
    DESTINATION Mods/BML_ObjectLoad
)
