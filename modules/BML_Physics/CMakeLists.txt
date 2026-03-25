cmake_minimum_required(VERSION 3.15)

find_package(VirtoolsSDK REQUIRED)

bml_add_module(BML_Physics
    SOURCES
        src/PhysicsMod.cpp
        src/PhysicsHook.cpp
        src/PhysicsHook.h
    MANIFEST mod.toml
    INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/include/BML
        ${CMAKE_SOURCE_DIR}/src/Utils
        ${CMAKE_SOURCE_DIR}/deps
        ${VIRTOOLS_SDK_INCLUDE_DIR}
    LINK_LIBRARIES
        VirtoolsSDK::CK2
        VirtoolsSDK::VxMath
        BML_HookUtils
)

install(TARGETS BML_Physics
    RUNTIME DESTINATION Mods/BML_Physics
    LIBRARY DESTINATION Mods/BML_Physics
)

install(FILES mod.toml
    DESTINATION Mods/BML_Physics
)
