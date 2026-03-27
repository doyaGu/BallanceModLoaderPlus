
# Public API headers for consumers of this module
add_library(BML_Event_api INTERFACE)
target_include_directories(BML_Event_api INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:modules/BML_Event/include>
)
bml_register_module_api(BML_Event_api)

bml_add_module(BML_Event
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

install(TARGETS BML_Event
    RUNTIME DESTINATION Mods/BML_Event
    LIBRARY DESTINATION Mods/BML_Event
)

install(FILES mod.toml
    DESTINATION Mods/BML_Event
)

install(DIRECTORY include/
    DESTINATION modules/BML_Event/include
    COMPONENT sdk
)
