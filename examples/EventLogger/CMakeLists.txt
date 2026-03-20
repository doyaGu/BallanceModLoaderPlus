cmake_minimum_required(VERSION 3.12)

# EventLogger - v0.4+ Module Example
# 
# Demonstrates:
# - IMC event subscription (Input, Render, Game lifecycle)
# - Dynamic API loading using bml_loader.h
# - Proper module lifecycle (ATTACH/DETACH)
# - Logging to per-module log file

bml_add_module(EventLogger
    SOURCES EventLogger.cpp
    ID "com.example.eventlogger"
    VERSION "1.0.0"
    DISPLAY_NAME "Event Logger"
    AUTHORS "BML Team"
    DESCRIPTION "Example v0.4 module demonstrating IMC event subscription and logging"
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/mod.toml"
)
