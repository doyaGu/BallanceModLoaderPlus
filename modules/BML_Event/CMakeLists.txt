cmake_minimum_required(VERSION 3.15)
project(BML_Event LANGUAGES CXX)

set(BML_EVENT_OUTPUT_DIR "${CMAKE_BINARY_DIR}/Mods/BML_Event")

# BML_Event Module - Game Event Hook and Topic Registration
# This module hooks into game scripts to capture and publish game events

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES
    src/EventMod.cpp
    src/EventTopics.cpp
    src/EventHook.cpp
)

set(HEADERS
    src/EventTopics.h
    src/EventHook.h
    src/ScriptHelper.h
)

# Build as shared library (Virtools plugin format)
add_library(BML_Event SHARED ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(BML_Event
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/BML
        ${CMAKE_SOURCE_DIR}/src
)

# Link dependencies - Self-contained, no BMLPlus dependency
target_link_libraries(BML_Event
    PRIVATE CK2 VxMath
)

# Windows-specific settings
if(WIN32)
    target_compile_definitions(BML_Event PRIVATE
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

# Output directory
set_target_properties(BML_Event PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${BML_EVENT_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${BML_EVENT_OUTPUT_DIR}"
    OUTPUT_NAME "BML_Event"
)

add_custom_command(TARGET BML_Event POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/mod.toml
        $<TARGET_FILE_DIR:BML_Event>/mod.toml
)

install(TARGETS BML_Event
    RUNTIME DESTINATION Mods/BML_Event
    LIBRARY DESTINATION Mods/BML_Event
)

install(FILES mod.toml
    DESTINATION Mods/BML_Event
)
