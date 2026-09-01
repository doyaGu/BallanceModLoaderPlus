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

# This Mod is loaded by Ballance Player. It exercises both the public adapter
# and the exact private Runtime implementation against the proprietary CK2.
bml_add_mod(ExecuteBBTest
        ExecuteBBTest.cpp
        BehaviorRuntimeProbe.cpp
        BehaviorRuntimeProbe.h
        BehaviorLifecycleFixtureApi.h
        ${PROJECT_SOURCE_DIR}/src/Behavior/Callback.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/CKEdit.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/CKGraph.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Edit.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Execution.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Lifecycle.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Layout.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/HookBlock.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/ObjectLoad.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/FrameStore.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Parameter.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Physicalize.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/PhysicsImpulse.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/PrototypeCatalog.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Runtime.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Text2D.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Topology.cpp
)
target_include_directories(ExecuteBBTest PRIVATE ${PROJECT_SOURCE_DIR}/src)
set_target_properties(ExecuteBBTest PROPERTIES
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
