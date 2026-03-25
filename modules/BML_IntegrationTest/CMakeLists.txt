cmake_minimum_required(VERSION 3.15)

bml_add_module(BML_IntegrationTest
    SOURCES
        src/IntegrationTestMod.cpp
        src/BuiltinModuleProbe.cpp
    MANIFEST mod.toml
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/Utils
        ${IMGUI_SOURCE_DIR}
    LINK_LIBRARIES
        BML_Utils
)
