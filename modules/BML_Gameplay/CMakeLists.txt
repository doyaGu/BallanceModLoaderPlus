
bml_add_module(BML_Gameplay
    SOURCES
        src/FpsCounter.cpp
        src/GameplayMod.cpp
        src/SRTimer.cpp
        src/RealTimer.cpp
    MANIFEST mod.toml
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${VIRTOOLS_SDK_INCLUDE_DIR}
        ${IMGUI_SOURCE_DIR}
    LINK_LIBRARIES
        VirtoolsSDK::CK2
        VirtoolsSDK::VxMath
)

bml_add_module_resources(BML_Gameplay
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/locale
    DESTINATION locale
)

install(TARGETS BML_Gameplay
    RUNTIME DESTINATION Mods/BML_Gameplay
    LIBRARY DESTINATION Mods/BML_Gameplay
)

install(FILES mod.toml DESTINATION Mods/BML_Gameplay)
install(DIRECTORY locale DESTINATION Mods/BML_Gameplay)
