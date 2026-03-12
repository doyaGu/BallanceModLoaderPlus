#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "imgui.h"

#include "bml_console.h"
#include "bml_config.hpp"
#include "bml_module.h"
#include "bml_topics.h"
#include "bml_ui.h"

#define BML_LOADER_IMPLEMENTATION
#include "bml_loader.h"

namespace {
constexpr size_t kFpsSamples = 90;

struct HudSettings {
    bool enabled = true;
    bool showTitle = true;
    bool showFps = true;
    std::string titleText = "Ballance Mod Loader Plus";
};

class FpsCounter {
public:
    void Update(float deltaTime) {
        if (deltaTime <= 0.0f) {
            return;
        }
        m_Samples[m_Index] = deltaTime;
        m_Index = (m_Index + 1u) % m_Samples.size();
        if (m_Count < m_Samples.size()) {
            ++m_Count;
        }
        float total = 0.0f;
        for (size_t i = 0; i < m_Count; ++i) {
            total += m_Samples[i];
        }
        m_Fps = total > 0.0001f ? static_cast<float>(m_Count) / total : 0.0f;
    }

    const char *Format() {
        const int rounded = static_cast<int>(m_Fps + 0.5f);
        if (rounded != m_LastRounded) {
            m_LastRounded = rounded;
            std::snprintf(m_Buffer.data(), m_Buffer.size(), "FPS: %d", rounded);
        }
        return m_Buffer.data();
    }

private:
    std::array<float, kFpsSamples> m_Samples = {};
    std::array<char, 24> m_Buffer = {"FPS: 0"};
    size_t m_Index = 0;
    size_t m_Count = 0;
    float m_Fps = 0.0f;
    int m_LastRounded = -1;
};

BML_Mod g_ModHandle = nullptr;
BML_Subscription g_DrawSub = nullptr;
BML_Subscription g_PostProcessSub = nullptr;
BML_TopicId g_TopicDraw = 0;
BML_TopicId g_TopicPostProcess = 0;
BML_TopicId g_TopicConsoleOutput = 0;
static BML_UI_ApiTable *g_UiApi = nullptr;
PFN_bmlUIGetImGuiContext g_GetImGuiContext = nullptr;
BML_ConsoleExtension *g_ConsoleExtension = nullptr;
bool g_ConsoleExtensionLoaded = false;
HudSettings g_Settings;
FpsCounter g_FpsCounter;

std::string ToLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

void AddCompletionIfMatches(const char *candidate,
    const std::string &prefix,
    PFN_BML_ConsoleCompletionSink sink,
    void *sinkUserData) {
    if (!candidate || !sink) {
        return;
    }

    const std::string loweredCandidate = ToLowerAscii(candidate);
    if (prefix.empty() || loweredCandidate.rfind(prefix, 0) == 0) {
        sink(candidate, sinkUserData);
    }
}

void PublishConsoleMessage(const std::string &message, uint32_t flags = BML_CONSOLE_OUTPUT_FLAG_SYSTEM) {
    if (g_TopicConsoleOutput == 0 || message.empty()) {
        return;
    }
    BML_ConsoleOutputEvent event = BML_CONSOLE_OUTPUT_EVENT_INIT;
    event.message_utf8 = message.c_str();
    event.flags = flags;
    bmlImcPublish(g_TopicConsoleOutput, &event, sizeof(event));
}

void SetBoolConfig(const char *name, bool value) {
    bml::Config config(g_ModHandle);
    config.SetBool("General", name, value);
}

void SetStringConfig(const char *name, const std::string &value) {
    bml::Config config(g_ModHandle);
    config.SetString("General", name, value);
}

void EnsureDefaultConfig() {
    bml::Config config(g_ModHandle);
    if (!config.GetBool("General", "Enabled").has_value()) {
        config.SetBool("General", "Enabled", g_Settings.enabled);
    }
    if (!config.GetBool("General", "ShowTitle").has_value()) {
        config.SetBool("General", "ShowTitle", g_Settings.showTitle);
    }
    if (!config.GetBool("General", "ShowFPS").has_value()) {
        config.SetBool("General", "ShowFPS", g_Settings.showFps);
    }
    if (!config.GetString("General", "TitleText").has_value()) {
        config.SetString("General", "TitleText", g_Settings.titleText);
    }
}

void RefreshConfig() {
    bml::Config config(g_ModHandle);
    g_Settings.enabled = config.GetBool("General", "Enabled").value_or(true);
    g_Settings.showTitle = config.GetBool("General", "ShowTitle").value_or(true);
    g_Settings.showFps = config.GetBool("General", "ShowFPS").value_or(true);
    g_Settings.titleText = config.GetString("General", "TitleText").value_or("Ballance Mod Loader Plus");
}

void DrawShadowedText(ImDrawList *drawList, const ImVec2 &pos, ImU32 color, const char *text) {
    drawList->AddText(ImVec2(pos.x + 1.0f, pos.y + 1.0f), IM_COL32(0, 0, 0, 200), text);
    drawList->AddText(pos, color, text);
}

void RenderHud() {
    if (!g_Settings.enabled) {
        return;
    }

    ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImDrawList *drawList = ImGui::GetForegroundDrawList();
    const ImVec2 viewPos = viewport->Pos;
    const ImVec2 viewSize = viewport->Size;
    const float marginX = std::max(16.0f, viewSize.x * 0.02f);
    const float marginY = std::max(14.0f, viewSize.y * 0.018f);

    if (g_Settings.showTitle && !g_Settings.titleText.empty()) {
        const ImVec2 textSize = ImGui::CalcTextSize(g_Settings.titleText.c_str());
        const ImVec2 textPos(viewPos.x + (viewSize.x - textSize.x) * 0.5f, viewPos.y + marginY);
        DrawShadowedText(drawList, textPos, IM_COL32(255, 255, 255, 255), g_Settings.titleText.c_str());
    }

    if (g_Settings.showFps) {
        const ImVec2 fpsPos(viewPos.x + marginX, viewPos.y + marginY);
        DrawShadowedText(drawList, fpsPos, IM_COL32(196, 232, 255, 255), g_FpsCounter.Format());
    }
}

BML_Result ValidateAttachArgs(const BML_ModAttachArgs *args) {
    if (!args || args->struct_size != sizeof(BML_ModAttachArgs) || args->mod == nullptr || args->get_proc == nullptr) {
        return BML_RESULT_INVALID_ARGUMENT;
    }
    return args->api_version == BML_MOD_ENTRYPOINT_API_VERSION ? BML_RESULT_OK : BML_RESULT_VERSION_MISMATCH;
}

BML_Result ValidateDetachArgs(const BML_ModDetachArgs *args) {
    if (!args || args->struct_size != sizeof(BML_ModDetachArgs) || args->mod == nullptr) {
        return BML_RESULT_INVALID_ARGUMENT;
    }
    return args->api_version == BML_MOD_ENTRYPOINT_API_VERSION ? BML_RESULT_OK : BML_RESULT_VERSION_MISMATCH;
}

void ReleaseSubscriptions() {
    if (g_DrawSub) {
        bmlImcUnsubscribe(g_DrawSub);
        g_DrawSub = nullptr;
    }
    if (g_PostProcessSub) {
        bmlImcUnsubscribe(g_PostProcessSub);
        g_PostProcessSub = nullptr;
    }
}

void OnDraw(BML_Context, BML_TopicId, const BML_ImcMessage *, void *) {
    if (!g_GetImGuiContext) {
        return;
    }
    ImGuiContext *context = static_cast<ImGuiContext *>(g_GetImGuiContext());
    if (!context) {
        return;
    }
    ImGui::SetCurrentContext(context);
    RenderHud();
}

void OnPostProcess(BML_Context, BML_TopicId, const BML_ImcMessage *msg, void *) {
    if (!msg || !msg->data || msg->size < sizeof(float)) {
        return;
    }
    g_FpsCounter.Update(*static_cast<const float *>(msg->data));
}

BML_Result ExecuteHudConsoleCommand(const BML_ConsoleCommandInvocation *invocation, void *) {
    if (!invocation || invocation->argc == 0 || !invocation->argv_utf8 || !invocation->argv_utf8[0]) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    if (invocation->argc == 1 || (invocation->argc == 2 && std::strcmp(invocation->argv_utf8[1], "show") == 0)) {
        PublishConsoleMessage(std::string("[hud] enabled=") + (g_Settings.enabled ? "on" : "off") +
            ", title=" + (g_Settings.showTitle ? "on" : "off") +
            ", fps=" + (g_Settings.showFps ? "on" : "off"));
        return BML_RESULT_OK;
    }

    auto readBoolArg = [&](uint32_t index, bool &outValue) -> bool {
        if (index >= invocation->argc || !invocation->argv_utf8[index]) {
            return false;
        }
        if (std::strcmp(invocation->argv_utf8[index], "on") == 0) {
            outValue = true;
            return true;
        }
        if (std::strcmp(invocation->argv_utf8[index], "off") == 0) {
            outValue = false;
            return true;
        }
        return false;
    };

    bool value = false;
    if (std::strcmp(invocation->argv_utf8[1], "on") == 0 || std::strcmp(invocation->argv_utf8[1], "off") == 0) {
        if (!readBoolArg(1, value)) {
            PublishConsoleMessage("[hud] usage: hud on|off|title on|off|fps on|off|text <value>", BML_CONSOLE_OUTPUT_FLAG_ERROR);
            return BML_RESULT_INVALID_ARGUMENT;
        }
        g_Settings.enabled = value;
        SetBoolConfig("Enabled", value);
        PublishConsoleMessage(std::string("[hud] enabled ") + (value ? "on" : "off"));
        return BML_RESULT_OK;
    }

    if ((std::strcmp(invocation->argv_utf8[1], "title") == 0 || std::strcmp(invocation->argv_utf8[1], "fps") == 0) && readBoolArg(2, value)) {
        if (std::strcmp(invocation->argv_utf8[1], "title") == 0) {
            g_Settings.showTitle = value;
            SetBoolConfig("ShowTitle", value);
        } else {
            g_Settings.showFps = value;
            SetBoolConfig("ShowFPS", value);
        }
        PublishConsoleMessage(std::string("[hud] ") + invocation->argv_utf8[1] + " " + (value ? "on" : "off"));
        return BML_RESULT_OK;
    }

    if (std::strcmp(invocation->argv_utf8[1], "text") == 0 && invocation->argc >= 3 && invocation->args_utf8) {
        std::string text = invocation->args_utf8;
        if (text.rfind("text ", 0) == 0) {
            text = text.substr(5);
        }
        if (!text.empty()) {
            g_Settings.titleText = text;
            SetStringConfig("TitleText", text);
            PublishConsoleMessage("[hud] title text updated");
            return BML_RESULT_OK;
        }
    }

    PublishConsoleMessage("[hud] usage: hud on|off|title on|off|fps on|off|text <value>", BML_CONSOLE_OUTPUT_FLAG_ERROR);
    return BML_RESULT_INVALID_ARGUMENT;
}

void CompleteHudConsoleCommand(const BML_ConsoleCompletionContext *context,
    PFN_BML_ConsoleCompletionSink sink,
    void *sinkUserData,
    void *) {
    if (!context || !sink) {
        return;
    }

    const std::string prefix = context->current_token_utf8 ? ToLowerAscii(context->current_token_utf8) : std::string();
    if (context->token_index == 1) {
        static constexpr const char *kSubcommands[] = {"show", "on", "off", "title", "fps", "text"};
        for (const char *candidate : kSubcommands) {
            AddCompletionIfMatches(candidate, prefix, sink, sinkUserData);
        }
        return;
    }

    if (context->token_index == 2 && context->argc >= 2 && context->argv_utf8 && context->argv_utf8[1]) {
        const std::string subcommand = ToLowerAscii(context->argv_utf8[1]);
        if (subcommand == "title" || subcommand == "fps") {
            AddCompletionIfMatches("on", prefix, sink, sinkUserData);
            AddCompletionIfMatches("off", prefix, sink, sinkUserData);
        }
    }
}

BML_Result HandleAttach(const BML_ModAttachArgs *args) {
    const BML_Result validation = ValidateAttachArgs(args);
    if (validation != BML_RESULT_OK) {
        return validation;
    }

    g_ModHandle = args->mod;
    BML_Result result = bmlLoadAPI(args->get_proc, args->get_proc_by_id);
    if (result != BML_RESULT_OK) {
        return result;
    }

    {
        BML_Version uiReqVer = bmlMakeVersion(0, 4, 0);
        BML_Result uiResult = bmlExtensionLoad(BML_UI_EXTENSION_NAME, &uiReqVer, reinterpret_cast<void **>(&g_UiApi), nullptr);
        if (uiResult != BML_RESULT_OK || !g_UiApi) {
            bmlUnloadAPI();
            g_ModHandle = nullptr;
            return BML_RESULT_NOT_FOUND;
        }
        g_GetImGuiContext = g_UiApi->GetImGuiContext;
    }

    EnsureDefaultConfig();
    RefreshConfig();

    bmlImcGetTopicId(BML_TOPIC_UI_DRAW, &g_TopicDraw);
    bmlImcGetTopicId(BML_TOPIC_ENGINE_POST_PROCESS, &g_TopicPostProcess);
    bmlImcGetTopicId(BML_TOPIC_CONSOLE_OUTPUT, &g_TopicConsoleOutput);

    if ((result = bmlImcSubscribe(g_TopicDraw, OnDraw, nullptr, &g_DrawSub)) != BML_RESULT_OK) {
        bmlUnloadAPI();
        g_ModHandle = nullptr;
        return result;
    }
    if ((result = bmlImcSubscribe(g_TopicPostProcess, OnPostProcess, nullptr, &g_PostProcessSub)) != BML_RESULT_OK) {
        ReleaseSubscriptions();
        bmlUnloadAPI();
        g_ModHandle = nullptr;
        return result;
    }

    BML_Version requiredVersion = bmlMakeVersion(1, 0, 0);
    result = bmlExtensionLoad(BML_CONSOLE_EXTENSION_NAME, &requiredVersion, reinterpret_cast<void **>(&g_ConsoleExtension), nullptr);
    if (result == BML_RESULT_OK) {
        g_ConsoleExtensionLoaded = true;

        BML_ConsoleCommandDesc desc = BML_CONSOLE_COMMAND_DESC_INIT;
        desc.name_utf8 = "hud";
        desc.description_utf8 = "Control HUD visibility, title text, and FPS overlay";
        desc.execute = ExecuteHudConsoleCommand;
        desc.complete = CompleteHudConsoleCommand;
        result = g_ConsoleExtension->RegisterCommand(&desc);
        if (result != BML_RESULT_OK) {
            bmlLog(bmlGetGlobalContext(), BML_LOG_WARN, "BML_HUD", "Failed to register hud console command");
        }
    } else {
        g_ConsoleExtension = nullptr;
        g_ConsoleExtensionLoaded = false;
        bmlLog(bmlGetGlobalContext(), BML_LOG_WARN, "BML_HUD", "Failed to load BML_EXT_Console; hud console command will be unavailable");
    }

    bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, "BML_HUD", "HUD module initialized");
    PublishConsoleMessage("BML_HUD ready. Try: hud show");
    return BML_RESULT_OK;
}

BML_Result HandleDetach(const BML_ModDetachArgs *args) {
    const BML_Result validation = ValidateDetachArgs(args);
    if (validation != BML_RESULT_OK) {
        return validation;
    }

    if (g_ConsoleExtensionLoaded && g_ConsoleExtension) {
        g_ConsoleExtension->UnregisterCommand("hud");
        bmlExtensionUnload(BML_CONSOLE_EXTENSION_NAME);
        g_ConsoleExtensionLoaded = false;
    }
    g_ConsoleExtension = nullptr;
    ReleaseSubscriptions();
    g_GetImGuiContext = nullptr;
    bmlExtensionUnload(BML_UI_EXTENSION_NAME);
    g_UiApi = nullptr;
    g_ModHandle = nullptr;
    bmlUnloadAPI();
    return BML_RESULT_OK;
}
} // namespace

BML_MODULE_ENTRY BML_Result BML_ModEntrypoint(BML_ModEntrypointCommand cmd, void *data) {
    switch (cmd) {
    case BML_MOD_ENTRYPOINT_ATTACH:
        return HandleAttach(static_cast<const BML_ModAttachArgs *>(data));
    case BML_MOD_ENTRYPOINT_DETACH:
        return HandleDetach(static_cast<const BML_ModDetachArgs *>(data));
    default:
        return BML_RESULT_INVALID_ARGUMENT;
    }
}