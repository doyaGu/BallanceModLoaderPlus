cmake_minimum_required(VERSION 3.15)

project(BML_Physics VERSION 0.4.0 LANGUAGES CXX)

# C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(BML_PHYSICS_SOURCES
    src/PhysicsMod.cpp
    src/PhysicsHook.cpp
)

set(BML_PHYSICS_HEADERS
    src/PhysicsHook.h
)

# Create shared library (DLL)
add_library(BML_Physics SHARED
    ${BML_PHYSICS_SOURCES}
    ${BML_PHYSICS_HEADERS}
)

# Find Virtools SDK
find_package(VirtoolsSDK REQUIRED)

# Include directories
target_include_directories(BML_Physics PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/BML
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/Utils
    ${CMAKE_SOURCE_DIR}/deps
    ${VIRTOOLS_SDK_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(BML_Physics PRIVATE
    VirtoolsSDK::CK2
    VirtoolsSDK::VxMath
    BMLUtils
)

# Output configuration
set_target_properties(BML_Physics PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Mods/BML_Physics
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Mods/BML_Physics
)

# Copy mod.toml to output directory
add_custom_command(TARGET BML_Physics POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/mod.toml
        $<TARGET_FILE_DIR:BML_Physics>/mod.toml
)

# Install target
install(TARGETS BML_Physics
    RUNTIME DESTINATION Mods/BML_Physics
    LIBRARY DESTINATION Mods/BML_Physics
)

install(FILES mod.toml
    DESTINATION Mods/BML_Physics
)
