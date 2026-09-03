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

# Shared Player acceptance helpers. Every driver Mod navigates the shipped menu
# graph and captures frames the same way, so those units are compiled into each
# Mod that needs them instead of being duplicated per driver.
set(BML_PLAYER_SHARED_SOURCES
        PlayerFrameCapture.cpp
        PlayerFrameCapture.h
        PlayerNavigator.cpp
        PlayerNavigator.h
)

# Drives the Behavior Runtime acceptance flow on its own so the Behavior probes
# do not interleave with the ExecuteBB scenario.
bml_add_mod(BehaviorAcceptanceTest
        BehaviorAcceptanceTest.cpp
        ${BML_PLAYER_SHARED_SOURCES}
        BehaviorRuntimeSemanticsApi.h
)
set_target_properties(BehaviorAcceptanceTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        FOLDER "Tests/Player"
)

bml_add_mod(ExecuteBBTest
        ExecuteBBTest.cpp
        BehaviorRuntimeSemanticsApi.h
        BehaviorLifecycleFixtureApi.h
)
set_target_properties(ExecuteBBTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        FOLDER "Tests/Player"
)

# This fixture exercises CK2 semantics through private sources recompiled into
# the Mod. It is deliberately separate from production-composition probes.
bml_add_mod(BehaviorRuntimeSemanticsTest
        BehaviorRuntimeSemanticsTest.cpp
        BehaviorRuntimeSemantics.cpp
        BehaviorRuntimeSemantics.h
        BehaviorRuntimeSemanticsApi.h
        BehaviorLifecycleFixtureApi.h
        ${PROJECT_SOURCE_DIR}/src/Behavior/Callback.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/CKEdit.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/CKGraph.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Edit.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Execution.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Lifecycle.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Layout.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Hook.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/HookBlock.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/ObjectLoad.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/FrameStore.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Parameter.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Value.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Physicalize.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/PhysicsForce.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/PhysicsImpulse.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/PrototypeCatalog.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Relations.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Runtime.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Text2D.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Topology.cpp
)
target_include_directories(BehaviorRuntimeSemanticsTest PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)
set_target_properties(BehaviorRuntimeSemanticsTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        FOLDER "Tests/Player"
)

bml_add_mod(BehaviorTransportTest
        BehaviorTransportTest.cpp
        BehaviorTransportFixtureApi.h
)
set_target_properties(BehaviorTransportTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        FOLDER "Tests/Player"
)

bml_add_mod(BehaviorPatchTest
        BehaviorPatchTest.cpp
        BehaviorLifecycleFixtureApi.h
)
# This Mod links the normal import library and reaches Patches owned by BMLPlus.dll.
target_include_directories(BehaviorPatchTest PRIVATE ${PROJECT_SOURCE_DIR}/src)
set_target_properties(BehaviorPatchTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        FOLDER "Tests/Player"
)

# The published Plan and Hook facade, authored the way a Mod would author it.
# It reaches into src only for ObserveScript, which puts a script the probe
# created itself into the Plans world the game normally feeds.
bml_add_mod(BehaviorFacadeTest
        BehaviorFacadeTest.cpp
        BehaviorLifecycleFixtureApi.h
)
target_include_directories(BehaviorFacadeTest PRIVATE ${PROJECT_SOURCE_DIR}/src)
set_target_properties(BehaviorFacadeTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        FOLDER "Tests/Player"
)
