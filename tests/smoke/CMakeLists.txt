# Runtime smoke assets are installed by Validate-BMLBallance.ps1.

include("${PROJECT_SOURCE_DIR}/cmake/BMLMod.cmake")

# An installed SDK gets BML_IMC_CODEGEN from BMLConfig.cmake; in this tree the
# generator sits next to the sources.
set(BML_IMC_CODEGEN "${PROJECT_SOURCE_DIR}/tools/imc_codegen.py")
include("${PROJECT_SOURCE_DIR}/cmake/BMLImc.cmake")

if (NOT TARGET BML::BML)
    add_library(BML::BML ALIAS BML)
endif ()

bml_add_mod(BMLNativeImcSmoke NativeImcSmoke.cpp)
bml_target_imc_api(BMLNativeImcSmoke INPUT smoke.native.imc)
set_target_properties(BMLNativeImcSmoke PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
