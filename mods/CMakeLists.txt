include(BMLUtils)

set(BMOD_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)

set(BMODS
        BallSticky
        BMLModuls
        DeformedWB
        DualBallControl
        DynamicFov
        LanternFader
        MapScripts
        SpiritTrail
        TASSupport
        )

foreach(BMOD IN LISTS BMODS)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${BMOD}/CMakeLists.txt")
        add_subdirectory(${BMOD})
    endif()
endforeach()