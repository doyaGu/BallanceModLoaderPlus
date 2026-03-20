# MinimalMod - Minimal v0.4+ Module Example
# 
# This example demonstrates the bare minimum code needed for a v0.4+ module.

bml_add_module(MinimalMod
    SOURCES MinimalMod.cpp
    ID "com.example.minimalmod"
    VERSION "1.0.0"
    DISPLAY_NAME "Minimal Mod"
    AUTHORS "BML Team"
    DESCRIPTION "Minimal example demonstrating v0.4 module structure"
    GENERATE_MANIFEST
)
