include("${PROJECT_SOURCE_DIR}/cmake/BMLMod.cmake")

if (NOT TARGET BML::BML)
    add_library(BML::BML ALIAS BML)
endif ()

# This Mod is loaded by Ballance Player. It exercises both the public adapter
# and the exact private Runtime implementation against the proprietary CK2.
bml_add_mod(ExecuteBBTest
        ExecuteBBTest.cpp
        BehaviorRuntimeProbe.cpp
        BehaviorRuntimeProbe.h
        ${PROJECT_SOURCE_DIR}/src/Behavior/Execution.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/HookBlock.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/ObjectLoad.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Physicalize.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/PhysicsImpulse.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Runtime.cpp
        ${PROJECT_SOURCE_DIR}/src/Behavior/Text2D.cpp
)
target_include_directories(ExecuteBBTest PRIVATE ${PROJECT_SOURCE_DIR}/src)
set_target_properties(ExecuteBBTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        FOLDER "Tests/Player"
)
