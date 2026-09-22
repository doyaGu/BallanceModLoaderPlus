include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/BallanceMod.cmake")

set(BML_GAMEPLAY_MODS
        BallSticky
        BMLModuls
        DeformedWB
        DualBallControl
        DynamicFov
        MapScripts
        NewSpawn
        SpiritTrail)

set(BML_TOOLING_MODS
        CameraUtilities
        DebugUtilities
        Physics
        Segment
        TravelMode)

foreach(mod IN LISTS BML_GAMEPLAY_MODS)
    ballance_add_mod_directory(${mod} Gameplay)
endforeach()
foreach(mod IN LISTS BML_TOOLING_MODS)
    ballance_add_mod_directory(${mod} Tooling)
endforeach()

ballance_add_mod_directory(IVP Providers)

set(BML_MOD_PACKAGES)
foreach(mod IN LISTS BML_GAMEPLAY_MODS BML_TOOLING_MODS)
    if(TARGET ${mod}Package)
        list(APPEND BML_MOD_PACKAGES "${mod}.zip")
    else()
        list(APPEND BML_MOD_PACKAGES "${mod}.bmodp")
    endif()
endforeach()
list(APPEND BML_MOD_PACKAGES "IVP.bmodp")
list(SORT BML_MOD_PACKAGES)
string(REPLACE ";" "\n" BML_MOD_PACKAGE_LIST "${BML_MOD_PACKAGES}")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/mods.txt"
        CONTENT "${BML_MOD_PACKAGE_LIST}\n")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/mods.txt"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/BML")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/BML/mods")
