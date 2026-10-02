cmake_minimum_required(VERSION 3.25)
project(BehaviorCK2Regression LANGUAGES CXX)

# Standalone integration tests against the reference CK2 implementation.
set(CK2_SOURCE_DIR "" CACHE PATH "Reference CK2 source directory")
set(CK2_BUILD_DIR "${CK2_SOURCE_DIR}/build" CACHE PATH "Reference CK2 build directory")
set(VXMATH_SOURCE_DIR "${CK2_SOURCE_DIR}/../VxMath" CACHE PATH "Reference VxMath source directory")
if (NOT EXISTS "${CK2_SOURCE_DIR}/include/CKAll.h")
    message(FATAL_ERROR "Set CK2_SOURCE_DIR to the reference CK2 checkout")
endif ()
get_filename_component(BML_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(BEHAVIOR_ROOT "${BML_ROOT}/src/Behavior")
file(GLOB BEHAVIOR_SOURCES CONFIGURE_DEPENDS
    "${BEHAVIOR_ROOT}/Core/*.cpp"
    "${BEHAVIOR_ROOT}/Engine/*.cpp"
    "${BEHAVIOR_ROOT}/Edit/Ops/*.cpp")
foreach(SOURCE IN ITEMS Block Callback CKEdit CKGraph Execution FrameStore
        Lifecycle Pattern PrototypeCatalog Relations Runtime Sessions Watch
        Topology Edit/Ops Edit/Journal Edit/Transaction Edit/Revert
        Blocks/Hook Blocks/HookBlock)
    list(APPEND BEHAVIOR_SOURCES "${BEHAVIOR_ROOT}/${SOURCE}.cpp")
endforeach()
add_executable(BehaviorCK2Regression BehaviorCK2Regression.cpp ${BEHAVIOR_SOURCES})
target_compile_features(BehaviorCK2Regression PRIVATE cxx_std_20)
target_include_directories(BehaviorCK2Regression PRIVATE
    "${BML_ROOT}/src" "${BML_ROOT}/include"
    "${CK2_SOURCE_DIR}/include" "${VXMATH_SOURCE_DIR}/include")
target_compile_definitions(BehaviorCK2Regression PRIVATE
    CK2_EXPORTS NOMINMAX _CRT_SECURE_NO_WARNINGS)
target_compile_options(BehaviorCK2Regression PRIVATE /utf-8 /EHsc)
target_link_libraries(BehaviorCK2Regression PRIVATE
    "${CK2_BUILD_DIR}/lib/$<CONFIG>/CK2.lib"
    "${CK2_BUILD_DIR}/lib/$<CONFIG>/VxMath.lib")

enable_testing()
foreach(CASE IN ITEMS values value-conflict value-overlap slot-set slot-bind slot-value-only
        pin-type pin-type-shared pin-type-foreign)
    add_test(NAME BehaviorCK2-${CASE} COMMAND BehaviorCK2Regression ${CASE})
    set_tests_properties(BehaviorCK2-${CASE} PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CK2_BUILD_DIR}/bin/$<CONFIG>")
endforeach()
