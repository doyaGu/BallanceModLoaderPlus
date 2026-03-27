
# Public API headers for consumers of this module
add_library(BML_GameEvent_api INTERFACE)
target_include_directories(BML_GameEvent_api INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:modules/BML_GameEvent/include>
)
bml_register_module_api(BML_GameEvent_api)

bml_add_module(BML_GameEvent
    SOURCES
        src/EventMod.cpp
        src/EventTopics.cpp
        src/EventTopics.h
    MANIFEST mod.toml
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/BML
    LINK_LIBRARIES
        VirtoolsSDK::CK2
        VirtoolsSDK::VxMath
    COMPILE_DEFINITIONS
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX
)

install(TARGETS BML_GameEvent
    RUNTIME DESTINATION Mods/BML_GameEvent
    LIBRARY DESTINATION Mods/BML_GameEvent
)

install(FILES mod.toml
    DESTINATION Mods/BML_GameEvent
)

install(DIRECTORY include/
    DESTINATION modules/BML_GameEvent/include
    COMPONENT sdk
)
