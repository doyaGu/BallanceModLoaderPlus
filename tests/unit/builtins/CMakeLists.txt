add_bml_test(ConsoleLayoutTest
        SOURCES
        ConsoleLayoutTest.cpp
        ${BML_SOURCE_DIR}/Console/ConsoleLayout.cpp
)

add_bml_test(ModMenuSessionTest
        SOURCES
        ModMenuSessionTest.cpp
        ${BML_SOURCE_DIR}/ModMenu/ModMenuSession.cpp
)
target_include_directories(ModMenuSessionTest PRIVATE
        "${VIRTOOLS_SDK_INCLUDE_DIR}"
)

add_bml_test(ModMenuPagesTest
        SOURCES
        ModMenuPagesTest.cpp
        ${BML_SOURCE_DIR}/ModMenu/ModMenuPages.cpp
)

add_bml_test(FpsCounterTest
        SOURCES
        FpsCounterTest.cpp
        ${BML_SOURCE_DIR}/HUD/FpsCounter.cpp
)

add_bml_test(HUDAnimationTest
        SOURCES
        HUDAnimationTest.cpp
        ${BML_SOURCE_DIR}/HUD/HUDAnimation.cpp
        DEPENDENCIES
        VirtoolsSDK::CK2
        VirtoolsSDK::VxMath
)
target_include_directories(HUDAnimationTest BEFORE PRIVATE ${IMGUI_SOURCE_DIR})

add_bml_test(MapCatalogTest
        SOURCES
        MapCatalogTest.cpp
        ${BML_SOURCE_DIR}/CustomMaps/MapCatalog.cpp
        ${BML_SOURCE_DIR}/CustomMaps/MapCatalog.h
        DEPENDENCIES
        BMLUtils
)

add_bml_test(MapMenuStateTest
        SOURCES
        MapMenuStateTest.cpp
        ${BML_SOURCE_DIR}/CustomMaps/MapCatalog.cpp
        ${BML_SOURCE_DIR}/CustomMaps/MapCatalog.h
        ${BML_SOURCE_DIR}/CustomMaps/MapMenuState.cpp
        ${BML_SOURCE_DIR}/CustomMaps/MapMenuState.h
        DEPENDENCIES
        BMLUtils
)
target_include_directories(MapMenuStateTest BEFORE PRIVATE
        ${IMGUI_SOURCE_DIR}
        ${VIRTOOLS_SDK_INCLUDE_DIR}
)

add_bml_test(SRTimerTest
        SOURCES
        SRTimerTest.cpp
        ${BML_SOURCE_DIR}/HUD/SRTimer.cpp
)

add_bml_test(CommandContextTest
        SOURCES
        CommandContextTest.cpp
        ${BML_SOURCE_DIR}/Console/CommandContext.cpp
        DEPENDENCIES
        BMLUtils
)
