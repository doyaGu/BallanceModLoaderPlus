cmake_minimum_required(VERSION 3.15)

project(BML_Input VERSION 0.4.0 LANGUAGES CXX)

# C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(BML_INPUT_SOURCES
    src/InputMod.cpp
    src/InputExtension.cpp
    src/InputHook.cpp
)

set(BML_INPUT_HEADERS
    src/InputExtension.h
    src/InputHook.h
    src/VTables.h
    src/Macros.h
    src/HookUtils.h
)

# Create shared library (DLL)
add_library(BML_Input SHARED
    ${BML_INPUT_SOURCES}
    ${BML_INPUT_HEADERS}
)

# Find Virtools SDK
find_package(VirtoolsSDK REQUIRED)

# Include directories - only module-local and loader headers
target_include_directories(BML_Input PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${VIRTOOLS_SDK_INCLUDE_DIR}
)

# Link libraries (only Virtools SDK, no BMLPlus dependency)
target_link_libraries(BML_Input PRIVATE
    VirtoolsSDK::CK2
    VirtoolsSDK::VxMath
)

# Output configuration
set_target_properties(BML_Input PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Mods/BML_Input
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Mods/BML_Input
)

# Copy mod.toml to output directory
add_custom_command(TARGET BML_Input POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/mod.toml
        $<TARGET_FILE_DIR:BML_Input>/mod.toml
)

# Install target
install(TARGETS BML_Input
    RUNTIME DESTINATION Mods/BML_Input
    LIBRARY DESTINATION Mods/BML_Input
)

install(FILES mod.toml
    DESTINATION Mods/BML_Input
)
