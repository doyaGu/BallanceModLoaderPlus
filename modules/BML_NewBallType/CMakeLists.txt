cmake_minimum_required(VERSION 3.15)

# Public API headers for consumers of this module
add_library(BML_NewBallType_api INTERFACE)
target_include_directories(BML_NewBallType_api INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:modules/BML_NewBallType/include>
)
bml_register_module_api(BML_NewBallType_api)

bml_add_module(BML_NewBallType
    SOURCES
        src/NewBallTypeModule.cpp
        src/NewBallTypeMod.cpp
    MANIFEST mod.toml
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/Utils
        ${VIRTOOLS_SDK_INCLUDE_DIR}
    LINK_LIBRARIES
        VirtoolsSDK::CK2
        VirtoolsSDK::VxMath
)

install(TARGETS BML_NewBallType
    RUNTIME DESTINATION Mods/BML_NewBallType
    LIBRARY DESTINATION Mods/BML_NewBallType
)

install(FILES mod.toml DESTINATION Mods/BML_NewBallType)

install(DIRECTORY include/
    DESTINATION modules/BML_NewBallType/include
    COMPONENT sdk
)
