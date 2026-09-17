add_bml_test(BehaviorAuthoringTest
        SOURCES BehaviorAuthoringTest.cpp
        DEPENDENCIES VirtoolsSDK::CK2 VirtoolsSDK::VxMath
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
)
target_include_directories(BehaviorSessionTest PRIVATE
        "${VIRTOOLS_SDK_INCLUDE_DIR}"
)

add_bml_test(BehaviorPrototypeCatalogTest
        SOURCES
        BehaviorPrototypeCatalogTest.cpp
        ${BML_SOURCE_DIR}/Behavior/PrototypeCatalog.cpp
)
target_include_directories(BehaviorPrototypeCatalogTest PRIVATE
        "${VIRTOOLS_SDK_INCLUDE_DIR}"
)

add_bml_test(BehaviorLifecycleTest
        SOURCES
        BehaviorLifecycleTest.cpp
        ${BML_SOURCE_DIR}/Behavior/Lifecycle.cpp
)

add_bml_test(BehaviorScriptTest
        SOURCES
        BehaviorScriptTest.cpp
        ${BML_SOURCE_DIR}/Behavior/Script.cpp
)
target_compile_definitions(BehaviorScriptTest PRIVATE
        BML_BEHAVIOR_SCRIPT_DOMAIN_ONLY=1)
target_include_directories(BehaviorScriptTest PRIVATE
        "${VIRTOOLS_SDK_INCLUDE_DIR}"
)

add_bml_test(BehaviorCallbackTest
        SOURCES
        BehaviorCallbackTest.cpp
        ${BML_SOURCE_DIR}/Behavior/Callback.cpp
        ${BML_SOURCE_DIR}/Behavior/Blocks/Hook.cpp
)
target_include_directories(BehaviorCallbackTest PRIVATE
        "${VIRTOOLS_SDK_INCLUDE_DIR}"
)

add_bml_test(BehaviorWatchTest
        SOURCES
        BehaviorWatchTest.cpp
        ${BML_SOURCE_DIR}/Behavior/Watch.cpp
        ${BML_SOURCE_DIR}/Behavior/Callback.cpp
)
target_include_directories(BehaviorWatchTest PRIVATE
        "${VIRTOOLS_SDK_INCLUDE_DIR}"
)

add_bml_test(BehaviorTopologyTest
        SOURCES
        BehaviorTopologyTest.cpp
        ${BML_SOURCE_DIR}/Behavior/Topology.cpp
        DEPENDENCIES VirtoolsSDK::CK2 VirtoolsSDK::VxMath
)

add_bml_test(BehaviorRelationsTest
        SOURCES
        BehaviorRelationsTest.cpp
        ${BML_SOURCE_DIR}/Behavior/Relations.cpp
        DEPENDENCIES VirtoolsSDK::CK2 VirtoolsSDK::VxMath
)

add_bml_test(BehaviorPlanTest
        SOURCES
        BehaviorPlanTest.cpp
        ${BML_SOURCE_DIR}/Behavior/Plan.cpp
        DEPENDENCIES VirtoolsSDK::CK2 VirtoolsSDK::VxMath
)

add_bml_test(BehaviorPlanAllocationTest
        SOURCES
        BehaviorPlanAllocationTest.cpp
        ${BML_SOURCE_DIR}/Behavior/Plan.cpp
        DEPENDENCIES VirtoolsSDK::CK2 VirtoolsSDK::VxMath
)

add_bml_test(BehaviorEditTest
        SOURCES
        BehaviorEditTest.cpp
        BehaviorBlockTestStubs.cpp
        ${BML_SOURCE_DIR}/Behavior/Edit.cpp
        ${BML_SOURCE_DIR}/Behavior/Execution.cpp
        ${BML_SOURCE_DIR}/Behavior/FrameStore.cpp
        ${BML_SOURCE_DIR}/Behavior/Value.cpp
        DEPENDENCIES VirtoolsSDK::CK2 VirtoolsSDK::VxMath
)

add_bml_test(BehaviorGraphEditTest
        SOURCES
        BehaviorGraphEditTest.cpp
        BehaviorBlockTestStubs.cpp
        ${BML_SOURCE_DIR}/Behavior/Block.cpp
        ${BML_SOURCE_DIR}/Behavior/Callback.cpp
        ${BML_SOURCE_DIR}/Behavior/GraphEdit.cpp
        ${BML_SOURCE_DIR}/Behavior/Pattern.cpp
        ${BML_SOURCE_DIR}/Behavior/Edit.cpp
        ${BML_SOURCE_DIR}/Behavior/Execution.cpp
        ${BML_SOURCE_DIR}/Behavior/FrameStore.cpp
        ${BML_SOURCE_DIR}/Behavior/Topology.cpp
        ${BML_SOURCE_DIR}/Behavior/Value.cpp
        DEPENDENCIES VirtoolsSDK::CK2 VirtoolsSDK::VxMath
)

add_bml_test(BehaviorValueTest
        SOURCES
        BehaviorValueTest.cpp
        ${BML_SOURCE_DIR}/Behavior/Value.cpp
)
target_include_directories(BehaviorValueTest PRIVATE
        "${VIRTOOLS_SDK_INCLUDE_DIR}"
)
