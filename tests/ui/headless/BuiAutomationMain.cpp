#include <cstdio>

#include "imgui.h"
#include "imgui_test_engine/imgui_te_engine.h"
#include "imgui_test_engine/imgui_te_exporters.h"

void RegisterBuiAutomationTests(ImGuiTestEngine *engine);

int main(int argc, char **argv) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.DisplaySize = ImVec2(1280.0f, 720.0f);
    io.DeltaTime = 1.0f / 60.0f;
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.Fonts->AddFontDefault();

    unsigned char *pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    ImGui::StyleColorsDark();

    ImGuiTestEngine *engine = ImGuiTestEngine_CreateContext();
    ImGuiTestEngineIO &testIo = ImGuiTestEngine_GetIO(engine);
    testIo.ConfigRunSpeed = ImGuiTestRunSpeed_Fast;
    testIo.ConfigVerboseLevel = ImGuiTestVerboseLevel_Warning;
    testIo.ConfigVerboseLevelOnError = ImGuiTestVerboseLevel_Debug;
    testIo.ConfigLogToTTY = true;
    testIo.ConfigSavedSettings = false;
    testIo.ConfigCaptureEnabled = false;
    testIo.ConfigStopOnError = false;

    RegisterBuiAutomationTests(engine);
    ImGuiTestEngine_Start(engine, ImGui::GetCurrentContext());
    const char *filter = argc > 1 ? argv[1] : "all";
    ImGuiTestEngine_QueueTests(engine, ImGuiTestGroup_Tests, filter,
                               ImGuiTestRunFlags_RunFromCommandLine);

    constexpr int kMaximumFrames = 2000;
    int frame = 0;
    for (; frame < kMaximumFrames; ++frame) {
        ImGui::NewFrame();
        ImGui::Render();
        ImGuiTestEngine_PreSwap(engine);
        ImGuiTestEngine_PostSwap(engine);

        if (!testIo.IsRunningTests && ImGuiTestEngine_IsTestQueueEmpty(engine))
            break;
    }

    const bool timedOut = frame == kMaximumFrames;
    if (timedOut) {
        std::fprintf(stderr, "Bui automation exceeded %d frames.\n", kMaximumFrames);
        std::fflush(stderr);
        for (int abortFrame = 0; abortFrame < 100; ++abortFrame) {
            if (ImGuiTestEngine_TryAbortEngine(engine))
                break;
            ImGui::NewFrame();
            ImGui::Render();
        }
    }

    ImGuiTestEngine_Stop(engine);

    ImGuiTestEngineResultSummary result;
    ImGuiTestEngine_GetResultSummary(engine, &result);
    ImGuiTestEngine_PrintResultSummary(engine);

    ImGui::DestroyContext();
    ImGuiTestEngine_DestroyContext(engine);

    return !timedOut && result.CountTested > 0 &&
                   result.CountTested == result.CountSuccess &&
                   result.CountInQueue == 0
               ? 0
               : 1;
}
