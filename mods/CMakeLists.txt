include(BMLUtils)

set(BMOD_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)

set(BMODS
        BallSticky
        BMLModuls
        DebugUtilities
        DeformedWB
        DualBallControl
        DynamicFov
        LanternFader
        MapScripts
        Overclock
        SpiritTrail
        Summoner
        TASSupport
        TravelMode
        )

foreach(BMOD IN LISTS BMODS)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${BMOD}/CMakeLists.txt")
        add_subdirectory(${BMOD})
    endif()
endforeach()