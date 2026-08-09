if(NOT DEFINED BML_MOD_CMAKE OR "${BML_MOD_CMAKE}" STREQUAL "")
    message(FATAL_ERROR "BmlModCmakeTest needs BML_MOD_CMAKE.")
endif()

set(MSVC FALSE)
include("${BML_MOD_CMAKE}")
bml_add_mod(InvalidMod "${CMAKE_CURRENT_LIST_FILE}")
message(FATAL_ERROR "bml_add_mod accepted a non-MSVC-compatible C++ ABI.")
