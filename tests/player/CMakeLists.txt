include("${PROJECT_SOURCE_DIR}/cmake/BMLMod.cmake")

if (NOT TARGET BML::BML)
    add_library(BML::BML ALIAS BML)
endif ()

# This Mod is loaded by Ballance Player and uses only the public BML interface.
bml_add_mod(ExecuteBBTest ExecuteBBTest.cpp)
set_target_properties(ExecuteBBTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        FOLDER "Tests/Player"
)
