cmake_minimum_required(VERSION 3.15)

project(BML_Render VERSION 0.4.0 LANGUAGES CXX)

set(BML_RENDER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/Mods/BML_Render")

# C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(BML_RENDER_SOURCES
    src/RenderMod.cpp
    src/RenderHook.cpp
)

set(BML_RENDER_HEADERS
    src/RenderMod.h
    src/RenderHook.h
    src/Macros.h
)

# Create shared library (DLL)
add_library(BML_Render SHARED
    ${BML_RENDER_SOURCES}
    ${BML_RENDER_HEADERS}
)

# Include directories - only module-local and loader headers
target_include_directories(BML_Render PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${VIRTOOLS_SDK_INCLUDE_DIR}
)

# Link dependencies
target_link_libraries(BML_Render PRIVATE
    VirtoolsSDK::CK2
    VirtoolsSDK::VxMath
    minhook
)

# Output configuration
set_target_properties(BML_Render PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BML_RENDER_OUTPUT_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${BML_RENDER_OUTPUT_DIR}
)

# Copy mod.toml to output directory
add_custom_command(TARGET BML_Render POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/mod.toml
        $<TARGET_FILE_DIR:BML_Render>/mod.toml
)

# Install target
install(TARGETS BML_Render
    RUNTIME DESTINATION Mods/BML_Render
    LIBRARY DESTINATION Mods/BML_Render
)

install(FILES mod.toml
    DESTINATION Mods/BML_Render
)
