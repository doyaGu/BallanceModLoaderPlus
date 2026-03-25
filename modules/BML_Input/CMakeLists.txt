cmake_minimum_required(VERSION 3.15)

find_package(VirtoolsSDK REQUIRED)

# Public API headers for consumers of this module
add_library(BML_Input_api INTERFACE)
target_include_directories(BML_Input_api INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:modules/BML_Input/include>
)
bml_register_module_api(BML_Input_api)

bml_add_module(BML_Input
    SOURCES
        src/InputMod.cpp
        src/InputHook.cpp
        src/InputHook.h
        src/VTables.h
        src/Macros.h
    MANIFEST mod.toml
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${VIRTOOLS_SDK_INCLUDE_DIR}
    LINK_LIBRARIES
        BML_HookUtils
        VirtoolsSDK::CK2
        VirtoolsSDK::VxMath
)

install(TARGETS BML_Input
    RUNTIME DESTINATION Mods/BML_Input
    LIBRARY DESTINATION Mods/BML_Input
)

install(FILES mod.toml
    DESTINATION Mods/BML_Input
)

install(DIRECTORY include/
    DESTINATION modules/BML_Input/include
    COMPONENT sdk
)
