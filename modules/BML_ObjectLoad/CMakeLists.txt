cmake_minimum_required(VERSION 3.15)

find_package(VirtoolsSDK REQUIRED)

bml_add_module(BML_ObjectLoad
    SOURCES
        src/ObjectLoadMod.cpp
        src/ObjectLoadHook.cpp
        src/ObjectLoadHook.h
    MANIFEST mod.toml
    INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/include/BML
        ${CMAKE_SOURCE_DIR}/deps
        ${VIRTOOLS_SDK_INCLUDE_DIR}
    LINK_LIBRARIES
        VirtoolsSDK::CK2
        VirtoolsSDK::VxMath
)

install(TARGETS BML_ObjectLoad
    RUNTIME DESTINATION Mods/BML_ObjectLoad
    LIBRARY DESTINATION Mods/BML_ObjectLoad
)

install(FILES mod.toml
    DESTINATION Mods/BML_ObjectLoad
)
