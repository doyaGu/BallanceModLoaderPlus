# ConfigMod - v0.4+ Module Example
#
# Demonstrates:
# - Reading and writing configuration values (string, int, bool)
# - Using default values for missing keys
# - Logging loaded configuration on engine init

bml_add_module(ConfigMod
    SOURCES ConfigMod.cpp
    ID "com.example.configmod"
    VERSION "1.0.0"
    DISPLAY_NAME "Config Example"
    AUTHORS "BML Team"
    DESCRIPTION "Demonstrates BML configuration API"
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/mod.toml"
)
