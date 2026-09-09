include_guard(GLOBAL)

include(FetchContent)

if (POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif ()

# Dear ImGui Test Engine deliberately tracks Dear ImGui internals. Keep this
# revision close in date to deps/imgui and pin both the commit and archive hash.
FetchContent_Declare(
        imgui_test_engine
        URL https://github.com/ocornut/imgui_test_engine/archive/88c286c82d9589ff619370355bc51b1c3b666120.tar.gz
        URL_HASH SHA256=72812253A69F60BA1B6F1F60FD38D46430D483D98D0C9F292707357A0EEA9445
)
FetchContent_MakeAvailable(imgui_test_engine)

set(IMGUI_TEST_ENGINE_DIR
        "${imgui_test_engine_SOURCE_DIR}/imgui_test_engine")

set(IMGUI_TEST_ENGINE_SOURCES
        ${IMGUI_TEST_ENGINE_DIR}/imgui_capture_tool.cpp
        ${IMGUI_TEST_ENGINE_DIR}/imgui_te_context.cpp
        ${IMGUI_TEST_ENGINE_DIR}/imgui_te_coroutine.cpp
        ${IMGUI_TEST_ENGINE_DIR}/imgui_te_engine.cpp
        ${IMGUI_TEST_ENGINE_DIR}/imgui_te_exporters.cpp
        ${IMGUI_TEST_ENGINE_DIR}/imgui_te_perftool.cpp
        ${IMGUI_TEST_ENGINE_DIR}/imgui_te_ui.cpp
        ${IMGUI_TEST_ENGINE_DIR}/imgui_te_utils.cpp
)
