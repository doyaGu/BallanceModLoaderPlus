include("${PROJECT_SOURCE_DIR}/cmake/BMLMod.cmake")

add_library(BehaviorLifecycleFixture SHARED
        BehaviorLifecycleFixture.cpp
        BehaviorLifecycleFixtureApi.h
)
target_compile_features(BehaviorLifecycleFixture PRIVATE cxx_std_20)
target_include_directories(BehaviorLifecycleFixture PRIVATE
        ${VIRTOOLS_SDK_INCLUDE_DIR}
)
target_link_libraries(BehaviorLifecycleFixture PRIVATE
        VirtoolsSDK::CK2
        VirtoolsSDK::VxMath
)
set_target_properties(BehaviorLifecycleFixture PROPERTIES
        PREFIX ""
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        FOLDER "Tests/Player"
)

add_library(BehaviorTransportFixture SHARED
        BehaviorTransportFixture.cpp
        BehaviorTransportFixtureApi.h
)
target_compile_features(BehaviorTransportFixture PRIVATE cxx_std_20)
target_include_directories(BehaviorTransportFixture PRIVATE
        ${VIRTOOLS_SDK_INCLUDE_DIR}
)
target_link_libraries(BehaviorTransportFixture PRIVATE
        VirtoolsSDK::CK2
        VirtoolsSDK::VxMath
)
set_target_properties(BehaviorTransportFixture PROPERTIES
        PREFIX ""
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        FOLDER "Tests/Player"
)

if (NOT TARGET BML::BML)
    add_library(BML::BML ALIAS BML)
endif ()

# One Mod drives the shipped Player flow; every subject under test is a probe
# Mod that publishes a single verdict through the exported probe entry points.
# Installing a probe is all it takes to include it in a run.
set(BML_PLAYER_PROBE_SOURCES
        PlayerProbe.cpp
        PlayerProbe.h
        PlayerProbeApi.h
)

function(bml_add_player_test target)
    bml_add_mod(${target} ${ARGN})
    set_target_properties(${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
            FOLDER "Tests/Player"
    )
endfunction()

# Menu -> Level_01 -> tutorial exit -> screenshot -> exit, with no knowledge of
# any subject under test.
bml_add_player_test(PlayerFlowDriver
        PlayerFlowDriver.cpp
        PlayerFrameCapture.cpp
        PlayerFrameCapture.h
        PlayerNavigator.cpp
        PlayerNavigator.h
        PlayerProbeApi.h
)

# The ExecuteBB physics operations, on a body the probe owns itself.
bml_add_player_test(ExecuteBBTest
        ExecuteBBTest.cpp
        ${BML_PLAYER_PROBE_SOURCES}
)

# Offers a real script graph for a script Mod to hook and retire.
bml_add_player_test(BehaviorScriptHookTest
        BehaviorScriptHookTest.cpp
        ${BML_PLAYER_PROBE_SOURCES}
)

# This fixture exercises CK2 semantics through private sources recompiled into
# the Mod. It is deliberately separate from production-composition probes.
bml_add_player_test(BehaviorRuntimeSemanticsTest
        BehaviorRuntimeSemanticsTest.cpp
        BehaviorRuntimeSemantics.cpp
        BehaviorRuntimeSemantics.h
        BehaviorLifecycleFixtureApi.h
        ${BML_PLAYER_PROBE_SOURCES}
        ${PROJECT_SOURCE_DIR}/src/Behavior/Block.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Callback.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/CKEdit.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/CKGraph.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Edit.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Execution.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Lifecycle.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Layout.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Hook.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/HookBlock.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/FrameStore.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Parameter.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Value.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/PhysicsForce.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/PrototypeCatalog.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Relations.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Runtime.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Sessions.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Text2DView.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Topology.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Watch.cpp
)
target_include_directories(BehaviorRuntimeSemanticsTest PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

bml_add_player_test(BehaviorTransportTest
        BehaviorTransportTest.cpp
        BehaviorTransportFixtureApi.h
        ${BML_PLAYER_PROBE_SOURCES}
)

bml_add_player_test(BehaviorPatchTest
        BehaviorPatchTest.cpp
        BehaviorLifecycleFixtureApi.h
        ${BML_PLAYER_PROBE_SOURCES}
)
# This Mod links the normal import library and reaches Patches owned by BMLPlus.dll.
target_include_directories(BehaviorPatchTest PRIVATE ${PROJECT_SOURCE_DIR}/src)

# The published Script, Plan, and Hook facade, authored the way a Mod would
# author it. The Script is created, edited, activated, and retired entirely
# through the public Behavior interface.
bml_add_player_test(BehaviorFacadeTest
        BehaviorFacadeTest.cpp
        BehaviorLifecycleFixtureApi.h
        ${BML_PLAYER_PROBE_SOURCES}
)
