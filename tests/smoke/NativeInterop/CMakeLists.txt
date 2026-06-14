add_library(BMLNativeInteropSmokeMod SHARED
        BMLNativeInteropSmokeMod.cpp
)

target_include_directories(BMLNativeInteropSmokeMod
        PRIVATE
        ${BML_INCLUDE_DIR}
)

target_link_libraries(BMLNativeInteropSmokeMod
        PRIVATE
        BML
)

set_target_properties(BMLNativeInteropSmokeMod PROPERTIES
        OUTPUT_NAME "BMLNativeInteropSmoke"
        SUFFIX ".bmodp"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        FOLDER "Tests/Smoke"
)
