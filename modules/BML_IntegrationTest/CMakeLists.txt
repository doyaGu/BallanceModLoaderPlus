
bml_add_module(BML_IntegrationTest
    SOURCES
        src/IntegrationTestMod.cpp
        src/BuiltinModuleProbe.cpp
    MANIFEST mod.toml
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${IMGUI_SOURCE_DIR}
    LINK_LIBRARIES
        BML_PathUtils
)
