add_executable(ImGuiBackendBenchmark
        ImGuiBackendBenchmark.cpp
        ${BML_SOURCE_DIR}/UI/imgui_impl_ck2.cpp
        ${IMGUI_SOURCE_DIR}/imgui.cpp
        ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
        ${IMGUI_SOURCE_DIR}/imgui_tables.cpp
        ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
)

target_include_directories(ImGuiBackendBenchmark PRIVATE
        ${BML_SOURCE_DIR}
        ${IMGUI_SOURCE_DIR}
)

target_compile_definitions(ImGuiBackendBenchmark PRIVATE
        BML_IMGUI_TEST_ENGINE_SOURCE
        BML_TEST
        BML_TEST_CK2_BACKEND_LOGIC
        IMGUI_USER_CONFIG="BMLImGuiConfig.h"
)

set_target_properties(ImGuiBackendBenchmark PROPERTIES FOLDER "Benchmarks")
