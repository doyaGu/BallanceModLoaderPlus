add_bml_test(BehaviorAuthoringTest
        SOURCES BehaviorAuthoringTest.cpp
        DEPENDENCIES CK2 VxMath
)

add_bml_test(BehaviorExecutionTest
        SOURCES
        BehaviorExecutionTest.cpp
        ${BML_SOURCE_DIR}/Behavior/Execution.cpp
        ${BML_SOURCE_DIR}/Behavior/FrameStore.cpp
)

add_bml_test(BehaviorExecutionAllocationTest
        SOURCES
        BehaviorExecutionAllocationTest.cpp
        ${BML_SOURCE_DIR}/Behavior/Execution.cpp
        ${BML_SOURCE_DIR}/Behavior/FrameStore.cpp
)

add_bml_test(BehaviorSessionTest
        SOURCES
        BehaviorSessionTest.cpp
        BehaviorSessionTestStubs.cpp
        ${BML_SOURCE_DIR}/Behavior/Sessions.cpp
        ${BML_SOURCE_DIR}/Behavior/Execution.cpp
        ${BML_SOURCE_DIR}/Behavior/FrameStore.cpp
        ${BML_SOURCE_DIR}/Behavior/Watch.cpp
        ${BML_SOURCE_DIR}/Behavior/Callback.cpp
        DEPENDENCIES CK2 VxMath
)

add_bml_test(BehaviorPrototypeCatalogTest
        SOURCES
        BehaviorPrototypeCatalogTest.cpp
        ${BML_SOURCE_DIR}/Behavior/PrototypeCatalog.cpp
        DEPENDENCIES CK2 VxMath
)

add_bml_test(BehaviorLifecycleTest
        SOURCES
        BehaviorLifecycleTest.cpp
        ${BML_SOURCE_DIR}/Behavior/Lifecycle.cpp
)

set(BML_BEHAVIOR_PROGRAM_TEST_SOURCES
        BehaviorBlockTestStubs.cpp
        ${BML_SOURCE_DIR}/Behavior/Block.cpp
        ${BML_SOURCE_DIR}/Behavior/Callback.cpp
        ${BML_SOURCE_DIR}/Behavior/Edit/Program.cpp
        ${BML_SOURCE_DIR}/Behavior/Pattern.cpp
        ${BML_SOURCE_DIR}/Behavior/Edit/Ops.cpp
        ${BML_SOURCE_DIR}/Behavior/Execution.cpp
        ${BML_SOURCE_DIR}/Behavior/FrameStore.cpp
        ${BML_SOURCE_DIR}/Behavior/Topology.cpp
        ${BML_SOURCE_DIR}/Behavior/Core/Value.cpp
)

add_bml_test(BehaviorScriptTest
        SOURCES
        BehaviorScriptTest.cpp
        ${BML_SOURCE_DIR}/Behavior/Script.cpp
        ${BML_BEHAVIOR_PROGRAM_TEST_SOURCES}
        DEPENDENCIES CK2 VxMath
)
target_compile_definitions(BehaviorScriptTest PRIVATE
        BML_BEHAVIOR_SCRIPT_DOMAIN_ONLY=1)

add_bml_test(BehaviorCallbackTest
        SOURCES
        BehaviorCallbackTest.cpp
        ${BML_SOURCE_DIR}/Behavior/Callback.cpp
        ${BML_SOURCE_DIR}/Behavior/Blocks/Hook.cpp
        DEPENDENCIES CK2 VxMath
)

add_bml_test(BehaviorWatchTest
        SOURCES
        BehaviorWatchTest.cpp
        ${BML_SOURCE_DIR}/Behavior/Watch.cpp
        ${BML_SOURCE_DIR}/Behavior/Callback.cpp
        DEPENDENCIES CK2 VxMath
)

add_bml_test(BehaviorTopologyTest
        SOURCES
        BehaviorTopologyTest.cpp
        ${BML_SOURCE_DIR}/Behavior/Topology.cpp
        DEPENDENCIES CK2 VxMath
)

add_bml_test(BehaviorRelationsTest
        SOURCES
        BehaviorRelationsTest.cpp
        ${BML_SOURCE_DIR}/Behavior/Relations.cpp
        DEPENDENCIES CK2 VxMath
)

add_bml_test(BehaviorSelectionTest
        SOURCES
        BehaviorSelectionTest.cpp
        ${BML_SOURCE_DIR}/Behavior/Install/Selection.cpp
        DEPENDENCIES CK2 VxMath
)

add_bml_test(BehaviorSelectionAllocationTest
        SOURCES
        BehaviorSelectionAllocationTest.cpp
        ${BML_SOURCE_DIR}/Behavior/Install/Selection.cpp
        DEPENDENCIES CK2 VxMath
)

add_bml_test(BehaviorOpsTest
        SOURCES
        BehaviorOpsTest.cpp
        BehaviorBlockTestStubs.cpp
        ${BML_SOURCE_DIR}/Behavior/Edit/Ops.cpp
        ${BML_SOURCE_DIR}/Behavior/Execution.cpp
        ${BML_SOURCE_DIR}/Behavior/FrameStore.cpp
        ${BML_SOURCE_DIR}/Behavior/Core/Value.cpp
        DEPENDENCIES CK2 VxMath
)

add_bml_test(BehaviorProgramTest
        SOURCES
        BehaviorProgramTest.cpp
        ${BML_BEHAVIOR_PROGRAM_TEST_SOURCES}
        DEPENDENCIES CK2 VxMath
)

add_bml_test(BehaviorValueTest
        SOURCES
        BehaviorValueTest.cpp
        ${BML_SOURCE_DIR}/Behavior/Core/Value.cpp
        DEPENDENCIES CK2 VxMath
)

add_bml_test(BehaviorTextTest
        SOURCES
        BehaviorTextTest.cpp
        ${BML_SOURCE_DIR}/Engine/Text.cpp
        DEPENDENCIES BML_StringUtils
)

# Exercise the production graph adapter against a real CK context. The test
# supplies the EDITED callback so it can change graph ownership during Apply.
get_target_property(behavior_sources BML SOURCES)
list(FILTER behavior_sources INCLUDE REGEX "^Behavior/.*[.]cpp$")
list(FILTER behavior_sources EXCLUDE REGEX "/(CKPrototypeSource|Message|ScriptHelper|Sessions|Script|Text2DView|PhysicsForce)[.]cpp$")
list(TRANSFORM behavior_sources PREPEND "${BML_SOURCE_DIR}/")
add_bml_test(BehaviorTransactionFailureTest
        SOURCES BehaviorTransactionFailureTest.cpp ${behavior_sources}
        DEPENDENCIES CK2 VxMath
)
