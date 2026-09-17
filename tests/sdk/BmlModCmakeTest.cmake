if(NOT DEFINED BML_MOD_CMAKE OR "${BML_MOD_CMAKE}" STREQUAL "")
    message(FATAL_ERROR "BmlModCmakeTest needs BML_MOD_CMAKE.")
endif()
if(NOT DEFINED TEST_CASE OR "${TEST_CASE}" STREQUAL "")
    message(FATAL_ERROR "BmlModCmakeTest needs TEST_CASE.")
endif()

if(TEST_CASE STREQUAL "compiler")
    set(MSVC FALSE)
elseif(TEST_CASE STREQUAL "architecture")
    set(MSVC TRUE)
    set(CMAKE_SIZEOF_VOID_P 8)
elseif(TEST_CASE STREQUAL "runtime")
    set(MSVC TRUE)
    set(CMAKE_SIZEOF_VOID_P 4)
    set(BML_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL")
    cmake_policy(SET CMP0091 NEW)
elseif(TEST_CASE STREQUAL "runtime-policy")
    set(MSVC TRUE)
    set(CMAKE_SIZEOF_VOID_P 4)
    set(BML_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    # Reproduce a consumer whose cmake_minimum_required predates CMP0091, where
    # MSVC_RUNTIME_LIBRARY would be ignored and the pin would silently do
    # nothing. Selecting OLD explicitly is deprecated on purpose: it is the only
    # way to reach that state from script mode.
    cmake_policy(SET CMP0091 OLD)
else()
    message(FATAL_ERROR "Unknown BmlModCmakeTest case: ${TEST_CASE}")
endif()

include("${BML_MOD_CMAKE}")
bml_add_mod(InvalidMod "${CMAKE_CURRENT_LIST_FILE}")
message(FATAL_ERROR "bml_add_mod accepted the invalid ${TEST_CASE} case.")
