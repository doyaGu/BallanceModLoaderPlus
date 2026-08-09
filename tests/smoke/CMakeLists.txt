# Runtime smoke assets are installed by Validate-BMLBallance.ps1.

include("${PROJECT_SOURCE_DIR}/cmake/BMLMod.cmake")

if (NOT TARGET BML::BML)
    add_library(BML::BML ALIAS BML)
endif ()

bml_add_mod(BMLNativeImcSmoke NativeImcSmoke.cpp)
set_target_properties(BMLNativeImcSmoke PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
