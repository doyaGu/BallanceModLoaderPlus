#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif

#include "BuiltinModuleProbe.h"

#include <Windows.h>
#include <dinput.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "bml.hpp"
#include "bml_builtin_interfaces.h"
#include "bml_console.h"
#include "bml_engine_events.h"
#include "bml_game_topics.h"
#include "bml_input.h"
#include "bml_input_control.h"
#include "bml_interface.hpp"
#include "bml_topics.h"
#include "bml_ui.hpp"

namespace {
struct ConfigEntrySnapshot {
    std::string category;
    std::string name;
    BML_ConfigType type = BML_CONFIG_STRING;
};

struct ProbeReport {
    std::string target_module;
    std::vector<std::string> loaded_modules;
    std::map<std::string, std::vector<ConfigEntrySnapshot>> configs;
    int pre_process_count = 0;
    int post_process_count = 0;
    int ui_draw_count = 0;
    int pre_render_count = 0;
    int post_render_count = 0;
    int objectload_script_count = 0;
    int objectload_object_count = 0;
    int menu_pre_start_count = 0;
    int menu_post_start_count = 0;
    int physics_physicalize_count = 0;
    int physics_unphysicalize_count = 0;
    bool engine_init_received = false;
    bool engine_play_received = false;
    bool ui_extension_loaded = false;
    bool ui_imgui_context_live = false;
    bool input_extension_loaded = false;
    bool console_extension_loaded = false;
    bool console_window_visible = false;
    bool modmenu_window_visible = false;
    bool mapmenu_window_visible = false;
    bool mapmenu_button_visible = false;
    int console_help_result = BML_RESULT_NOT_FOUND;
    int hud_show_result = BML_RESULT_NOT_FOUND;
    int mapmenu_refresh_result = BML_RESULT_NOT_FOUND;
    int mapmenu_open_result = BML_RESULT_NOT_FOUND;
    int mapmenu_close_result = BML_RESULT_NOT_FOUND;
};

static std::string EscapeJson(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (unsigned char ch : value) {
        switch (ch) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\b':
            escaped += "\\b";
            break;
        case '\f':
            escaped += "\\f";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            if (ch < 0x20) {
                char buffer[7] = {};
                std::snprintf(buffer, sizeof(buffer), "\\u%04X", ch);
                escaped += buffer;
            } else {
                escaped.push_back(static_cast<char>(ch));
            }
            break;
        }
    }
    return escaped;
}

static std::filesystem::path ResolveProbeReportPath() {
    std::wstring path(260, L'\0');
    while (true) {
        const DWORD copied = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (copied == 0) {
            break;
        }
        if (copied < path.size() - 1) {
            path.resize(copied);
            const std::filesystem::path exe(path);
            return (exe.parent_path().parent_path() / L"ModLoader" / L"BuiltinModuleProbeReport.json").lexically_normal();
        }
        path.resize(path.size() * 2, L'\0');
    }

    return (std::filesystem::current_path() / "ModLoader" / "BuiltinModuleProbeReport.json").lexically_normal();
}

static std::string GetEnvironmentString(const char *name) {
    if (!name) {
        return {};
    }
    const char *value = std::getenv(name);
    return value ? std::string(value) : std::string();
}

static int GetEnvironmentInt(const char *name, int default_value) {
    const std::string value = GetEnvironmentString(name);
    if (value.empty()) {
        return default_value;
    }
    const int parsed = std::atoi(value.c_str());
    return parsed > 0 ? parsed : default_value;
}

static std::string ConfigTypeName(BML_ConfigType type) {
    switch (type) {
    case BML_CONFIG_BOOL:
        return "bool";
    case BML_CONFIG_INT:
        return "int";
    case BML_CONFIG_FLOAT:
        return "float";
    case BML_CONFIG_STRING:
        return "string";
    default:
        return "unknown";
    }
}

static void EnumerateConfigCallback(BML_Context,
    const BML_ConfigKey *key,
    const BML_ConfigValue *value,
    void *user_data) {
    if (!key || !value || !user_data) {
        return;
    }

    auto *entries = static_cast<std::vector<ConfigEntrySnapshot> *>(user_data);
    ConfigEntrySnapshot entry;
    entry.category = key->category ? key->category : "";
    entry.name = key->name ? key->name : "";
    entry.type = value->type;
    entries->push_back(std::move(entry));
}

} // namespace

class BuiltinModuleProbe::Impl {
public:
    explicit Impl(BML_Mod handle)
        : m_Handle(handle) {}

    BML_Result OnAttach() {
        m_Context = bml::AcquireInterface<BML_CoreContextInterface>(BML_CORE_CONTEXT_INTERFACE_ID, 1, 0, 0);
        m_Log = bml::AcquireInterface<BML_CoreLoggingInterface>(BML_CORE_LOGGING_INTERFACE_ID, 1, 0, 0);
        m_Config = bml::AcquireInterface<BML_CoreConfigInterface>(BML_CORE_CONFIG_INTERFACE_ID, 1, 0, 0);
        m_Module = bml::AcquireInterface<BML_CoreModuleInterface>(BML_CORE_MODULE_INTERFACE_ID, 1, 0, 0);
        m_ImcBus = bml::AcquireInterface<BML_ImcBusInterface>(BML_IMC_BUS_INTERFACE_ID, 1, 0, 0);
        if (!m_Context || !m_Log || !m_Config || !m_Module || !m_ImcBus) {
            return BML_RESULT_NOT_FOUND;
        }
        m_Subs.Bind(m_ImcBus.Get());

        m_StartTime = std::chrono::steady_clock::now();
        m_TargetModule = GetEnvironmentString("BML_PROBE_TARGET");
        m_MinDurationMs = GetEnvironmentInt("BML_PROBE_MIN_DURATION_MS", 8000);

        RefreshLoadedModules();
        CaptureConfigs();

        const bool all_subs =
            m_Subs.Add(BML_TOPIC_ENGINE_INIT, [this](const bml::imc::Message &) { OnEngineInit(); }) &&
            m_Subs.Add(BML_TOPIC_ENGINE_PLAY, [this](const bml::imc::Message &) { m_EnginePlayReceived = true; }) &&
            m_Subs.Add(BML_TOPIC_ENGINE_PRE_PROCESS, [this](const bml::imc::Message &) { OnPreProcess(); }) &&
            m_Subs.Add(BML_TOPIC_ENGINE_POST_PROCESS, [this](const bml::imc::Message &) {
                m_PostProcessCount.fetch_add(1, std::memory_order_relaxed);
            }) &&
            m_Subs.Add(BML_TOPIC_ENGINE_PRE_RENDER, [this](const bml::imc::Message &) {
                m_PreRenderCount.fetch_add(1, std::memory_order_relaxed);
            }) &&
            m_Subs.Add(BML_TOPIC_ENGINE_POST_RENDER, [this](const bml::imc::Message &) {
                m_PostRenderCount.fetch_add(1, std::memory_order_relaxed);
            }) &&
            m_Subs.Add(BML_TOPIC_OBJECTLOAD_LOAD_SCRIPT, [this](const bml::imc::Message &) {
                m_ObjectLoadScriptCount.fetch_add(1, std::memory_order_relaxed);
            }) &&
            m_Subs.Add(BML_TOPIC_OBJECTLOAD_LOAD_OBJECT, [this](const bml::imc::Message &) {
                m_ObjectLoadObjectCount.fetch_add(1, std::memory_order_relaxed);
            }) &&
            m_Subs.Add(BML_TOPIC_GAME_MENU_PRE_START, [this](const bml::imc::Message &) {
                m_MenuPreStartCount.fetch_add(1, std::memory_order_relaxed);
            }) &&
            m_Subs.Add(BML_TOPIC_GAME_MENU_POST_START, [this](const bml::imc::Message &) {
                m_MenuPostStartCount.fetch_add(1, std::memory_order_relaxed);
            }) &&
            m_Subs.Add("Physics/Physicalize", [this](const bml::imc::Message &) {
                m_PhysicsPhysicalizeCount.fetch_add(1, std::memory_order_relaxed);
            }) &&
            m_Subs.Add("Physics/Unphysicalize", [this](const bml::imc::Message &) {
                m_PhysicsUnphysicalizeCount.fetch_add(1, std::memory_order_relaxed);
            });

        if (!all_subs) {
            Log(BML_LOG_ERROR, "Failed to subscribe to probe topics");
            return BML_RESULT_FAIL;
        }

        Log(BML_LOG_INFO,
            "Builtin module probe active for target '%s'",
            m_TargetModule.empty() ? "<unspecified>" : m_TargetModule.c_str());
        return BML_RESULT_OK;
    }

    void OnDetach() {
        FinalizeReport();
        UnloadServices();
    }

private:
    static constexpr int kTargetFrames = 120;

    BML_Mod m_Handle = nullptr;
    bml::imc::SubscriptionManager m_Subs;
    bml::InterfaceLease<BML_CoreContextInterface> m_Context;
    bml::InterfaceLease<BML_CoreLoggingInterface> m_Log;
    bml::InterfaceLease<BML_CoreConfigInterface> m_Config;
    bml::InterfaceLease<BML_CoreModuleInterface> m_Module;
    bml::InterfaceLease<BML_ImcBusInterface> m_ImcBus;
    std::chrono::steady_clock::time_point m_StartTime;
    std::filesystem::path m_ReportPath{ResolveProbeReportPath()};
    std::string m_TargetModule;
    int m_MinDurationMs = 8000;

    std::vector<std::string> m_LoadedModules;
    std::unordered_map<std::string, BML_Mod> m_LoadedModuleMap;
    std::map<std::string, std::vector<ConfigEntrySnapshot>> m_ConfigSnapshots;

    std::atomic<int> m_PreProcessCount{0};
    std::atomic<int> m_PostProcessCount{0};
    std::atomic<int> m_UIDrawCount{0};
    std::atomic<int> m_PreRenderCount{0};
    std::atomic<int> m_PostRenderCount{0};
    std::atomic<int> m_ObjectLoadScriptCount{0};
    std::atomic<int> m_ObjectLoadObjectCount{0};
    std::atomic<int> m_MenuPreStartCount{0};
    std::atomic<int> m_MenuPostStartCount{0};
    std::atomic<int> m_PhysicsPhysicalizeCount{0};
    std::atomic<int> m_PhysicsUnphysicalizeCount{0};
    std::atomic<bool> m_EngineInitReceived{false};
    std::atomic<bool> m_EnginePlayReceived{false};
    std::atomic<bool> m_ReportWritten{false};

    bml::ui::DrawRegistration m_DrawReg;
    bml::InterfaceLease<BML_InputCaptureInterface> m_InputCaptureService;
    bml::InterfaceLease<BML_ConsoleCommandRegistry> m_ConsoleRegistry;
    bool m_UiLoaded = false;
    bool m_InputLoaded = false;
    bool m_ConsoleLoaded = false;

    int m_ConsoleHelpResult = BML_RESULT_NOT_FOUND;
    int m_HudShowResult = BML_RESULT_NOT_FOUND;
    int m_MapMenuRefreshResult = BML_RESULT_NOT_FOUND;
    int m_MapMenuOpenResult = BML_RESULT_NOT_FOUND;
    int m_MapMenuCloseResult = BML_RESULT_NOT_FOUND;

    bool m_ImGuiContextObserved = false;
    bool m_ConsoleWindowObserved = false;
    bool m_ModMenuWindowObserved = false;
    bool m_MapMenuWindowObserved = false;
    bool m_MapMenuButtonObserved = false;

    BML_Context GetContextHandle() const {
        if (!m_Context || !m_Context->GetGlobalContext) {
            return nullptr;
        }

        return m_Context->GetGlobalContext();
    }

    template <typename... Args>
    void Log(BML_LogSeverity severity, const char *format, Args... args) const {
        if (!m_Log || !m_Log->Log || !format) {
            return;
        }

        m_Log->Log(GetContextHandle(), severity, "BuiltinProbe", format, args...);
    }

    void RefreshLoadedModules() {
        m_LoadedModules.clear();
        m_LoadedModuleMap.clear();

        for (const bml::Mod mod : bml::GetLoadedModules(m_Module.Get())) {
            const auto id = mod.GetId();
            if (!id.has_value() || !*id) {
                continue;
            }
            m_LoadedModules.emplace_back(*id);
            m_LoadedModuleMap.emplace(*id, mod.Handle());
        }
        std::sort(m_LoadedModules.begin(), m_LoadedModules.end());
    }

    void CaptureConfigs() {
        m_ConfigSnapshots.clear();
        if (!m_Config || !m_Config->Enumerate) {
            return;
        }

        for (const auto &pair : m_LoadedModuleMap) {
            std::vector<ConfigEntrySnapshot> entries;
            m_Config->Enumerate(pair.second, EnumerateConfigCallback, &entries);
            if (!entries.empty()) {
                std::sort(entries.begin(), entries.end(), [](const ConfigEntrySnapshot &lhs, const ConfigEntrySnapshot &rhs) {
                    if (lhs.category != rhs.category) {
                        return lhs.category < rhs.category;
                    }
                    return lhs.name < rhs.name;
                });
                m_ConfigSnapshots.emplace(pair.first, std::move(entries));
            }
        }
    }

    static void DrawProbeCallback(const BML_UIDrawContext *ctx, void *user_data) {
        BML_UNUSED(ctx);
        auto *self = static_cast<Impl *>(user_data);
        if (self) {
            self->OnUiDraw();
        }
    }

    void LoadServices() {
        bml::CurrentModuleScope scope(m_Handle, m_Module.Get());

        m_DrawReg = bml::ui::RegisterDraw("bml.inttest.builtin_probe", 1000, DrawProbeCallback, this);
        m_UiLoaded = static_cast<bool>(m_DrawReg);

        m_InputCaptureService = bml::AcquireInterface<BML_InputCaptureInterface>(BML_INPUT_CAPTURE_INTERFACE_ID, 1, 0, 0);
        m_InputLoaded = static_cast<bool>(m_InputCaptureService);

        m_ConsoleRegistry = bml::AcquireInterface<BML_ConsoleCommandRegistry>(
            BML_CONSOLE_COMMAND_REGISTRY_INTERFACE_ID, 1, 0, 0);
        m_ConsoleLoaded = static_cast<bool>(m_ConsoleRegistry);
    }

    void UnloadServices() {
        bml::CurrentModuleScope scope(m_Handle, m_Module.Get());
        m_ConsoleRegistry.Reset();
        m_ConsoleLoaded = false;
        m_DrawReg.Reset();
        m_InputCaptureService.Reset();
        m_UiLoaded = false;
        m_InputLoaded = false;
    }

    void PublishSyntheticKey(uint32_t key_code) {
        const BML_KeyDownEvent event{key_code, key_code, 0u, false};
        bml::imc::publish(BML_TOPIC_INPUT_KEY_DOWN, event, m_ImcBus.Get());
    }

    void RunCommandProbes() {
        if (!m_ConsoleRegistry || !m_ConsoleRegistry->ExecuteCommand) {
            return;
        }
        m_ConsoleHelpResult = static_cast<int>(m_ConsoleRegistry->ExecuteCommand("help"));
        m_HudShowResult = static_cast<int>(m_ConsoleRegistry->ExecuteCommand("hud show"));
        m_MapMenuRefreshResult = static_cast<int>(m_ConsoleRegistry->ExecuteCommand("mapmenu refresh"));
        m_MapMenuOpenResult = static_cast<int>(m_ConsoleRegistry->ExecuteCommand("mapmenu open"));
    }

    void OnEngineInit() {
        if (m_EngineInitReceived.exchange(true)) {
            return;
        }

        RefreshLoadedModules();
        CaptureConfigs();
        LoadServices();
        RunCommandProbes();

        PublishSyntheticKey(DIK_SLASH);
        PublishSyntheticKey(DIK_F2);
    }

    void OnUiDraw() {
        m_UIDrawCount.fetch_add(1, std::memory_order_relaxed);
        m_ImGuiContextObserved = true;
    }

    void OnPreProcess() {
        const int frame = m_PreProcessCount.fetch_add(1, std::memory_order_relaxed) + 1;
        const auto elapsed_ms = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - m_StartTime).count());
        if (frame >= kTargetFrames && elapsed_ms >= m_MinDurationMs) {
            FinalizeReport();
        }
    }

    ProbeReport BuildReport() const {
        ProbeReport report;
        report.target_module = m_TargetModule;
        report.loaded_modules = m_LoadedModules;
        report.configs = m_ConfigSnapshots;
        report.pre_process_count = m_PreProcessCount.load(std::memory_order_relaxed);
        report.post_process_count = m_PostProcessCount.load(std::memory_order_relaxed);
        report.ui_draw_count = m_UIDrawCount.load(std::memory_order_relaxed);
        report.pre_render_count = m_PreRenderCount.load(std::memory_order_relaxed);
        report.post_render_count = m_PostRenderCount.load(std::memory_order_relaxed);
        report.objectload_script_count = m_ObjectLoadScriptCount.load(std::memory_order_relaxed);
        report.objectload_object_count = m_ObjectLoadObjectCount.load(std::memory_order_relaxed);
        report.menu_pre_start_count = m_MenuPreStartCount.load(std::memory_order_relaxed);
        report.menu_post_start_count = m_MenuPostStartCount.load(std::memory_order_relaxed);
        report.physics_physicalize_count = m_PhysicsPhysicalizeCount.load(std::memory_order_relaxed);
        report.physics_unphysicalize_count = m_PhysicsUnphysicalizeCount.load(std::memory_order_relaxed);
        report.engine_init_received = m_EngineInitReceived.load(std::memory_order_relaxed);
        report.engine_play_received = m_EnginePlayReceived.load(std::memory_order_relaxed);
        report.ui_extension_loaded = m_UiLoaded;
        report.ui_imgui_context_live = m_ImGuiContextObserved;
        report.input_extension_loaded = m_InputLoaded;
        report.console_extension_loaded = m_ConsoleLoaded;
        report.console_window_visible = m_ConsoleWindowObserved;
        report.modmenu_window_visible = m_ModMenuWindowObserved;
        report.mapmenu_window_visible = m_MapMenuWindowObserved;
        report.mapmenu_button_visible = m_MapMenuButtonObserved;
        report.console_help_result = m_ConsoleHelpResult;
        report.hud_show_result = m_HudShowResult;
        report.mapmenu_refresh_result = m_MapMenuRefreshResult;
        report.mapmenu_open_result = m_MapMenuOpenResult;
        report.mapmenu_close_result = m_MapMenuCloseResult;
        return report;
    }

    void WriteReport() {
        const ProbeReport report = BuildReport();

        std::error_code ec;
        std::filesystem::create_directories(m_ReportPath.parent_path(), ec);

        FILE *fp = _wfopen(m_ReportPath.c_str(), L"wb");
        if (!fp) {
            return;
        }

        const double duration_ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - m_StartTime).count();

        fprintf(fp, "{\n");
        fprintf(fp, "  \"version\": \"0.4.0\",\n");
        fprintf(fp, "  \"mode\": \"builtin-module-probe\",\n");
        fprintf(fp, "  \"target_module\": \"%s\",\n", EscapeJson(report.target_module).c_str());
        fprintf(fp, "  \"duration_ms\": %.1f,\n", duration_ms);

        fprintf(fp, "  \"loaded_modules\": [\n");
        for (size_t index = 0; index < report.loaded_modules.size(); ++index) {
            fprintf(fp, "    \"%s\"%s\n",
                EscapeJson(report.loaded_modules[index]).c_str(),
                (index + 1u < report.loaded_modules.size()) ? "," : "");
        }
        fprintf(fp, "  ],\n");

        fprintf(fp, "  \"counts\": {\n");
        fprintf(fp, "    \"pre_process\": %d,\n", report.pre_process_count);
        fprintf(fp, "    \"post_process\": %d,\n", report.post_process_count);
        fprintf(fp, "    \"ui_draw\": %d,\n", report.ui_draw_count);
        fprintf(fp, "    \"pre_render\": %d,\n", report.pre_render_count);
        fprintf(fp, "    \"post_render\": %d,\n", report.post_render_count);
        fprintf(fp, "    \"objectload_script\": %d,\n", report.objectload_script_count);
        fprintf(fp, "    \"objectload_object\": %d,\n", report.objectload_object_count);
        fprintf(fp, "    \"menu_pre_start\": %d,\n", report.menu_pre_start_count);
        fprintf(fp, "    \"menu_post_start\": %d,\n", report.menu_post_start_count);
        fprintf(fp, "    \"physics_physicalize\": %d,\n", report.physics_physicalize_count);
        fprintf(fp, "    \"physics_unphysicalize\": %d\n", report.physics_unphysicalize_count);
        fprintf(fp, "  },\n");

        fprintf(fp, "  \"flags\": {\n");
        fprintf(fp, "    \"engine_init_received\": %s,\n", report.engine_init_received ? "true" : "false");
        fprintf(fp, "    \"engine_play_received\": %s,\n", report.engine_play_received ? "true" : "false");
        fprintf(fp, "    \"ui_extension_loaded\": %s,\n", report.ui_extension_loaded ? "true" : "false");
        fprintf(fp, "    \"ui_imgui_context_live\": %s,\n", report.ui_imgui_context_live ? "true" : "false");
        fprintf(fp, "    \"input_extension_loaded\": %s,\n", report.input_extension_loaded ? "true" : "false");
        fprintf(fp, "    \"console_extension_loaded\": %s,\n", report.console_extension_loaded ? "true" : "false");
        fprintf(fp, "    \"console_window_visible\": %s,\n", report.console_window_visible ? "true" : "false");
        fprintf(fp, "    \"modmenu_window_visible\": %s,\n", report.modmenu_window_visible ? "true" : "false");
        fprintf(fp, "    \"mapmenu_window_visible\": %s,\n", report.mapmenu_window_visible ? "true" : "false");
        fprintf(fp, "    \"mapmenu_button_visible\": %s\n", report.mapmenu_button_visible ? "true" : "false");
        fprintf(fp, "  },\n");

        fprintf(fp, "  \"command_results\": {\n");
        fprintf(fp, "    \"help\": %d,\n", report.console_help_result);
        fprintf(fp, "    \"hud_show\": %d,\n", report.hud_show_result);
        fprintf(fp, "    \"mapmenu_refresh\": %d,\n", report.mapmenu_refresh_result);
        fprintf(fp, "    \"mapmenu_open\": %d,\n", report.mapmenu_open_result);
        fprintf(fp, "    \"mapmenu_close\": %d\n", report.mapmenu_close_result);
        fprintf(fp, "  },\n");

        fprintf(fp, "  \"configs\": {\n");
        size_t module_index = 0;
        for (const auto &pair : report.configs) {
            fprintf(fp, "    \"%s\": [\n", EscapeJson(pair.first).c_str());
            for (size_t entry_index = 0; entry_index < pair.second.size(); ++entry_index) {
                const ConfigEntrySnapshot &entry = pair.second[entry_index];
                fprintf(fp,
                    "      {\"category\": \"%s\", \"name\": \"%s\", \"type\": \"%s\"}%s\n",
                    EscapeJson(entry.category).c_str(),
                    EscapeJson(entry.name).c_str(),
                    EscapeJson(ConfigTypeName(entry.type)).c_str(),
                    (entry_index + 1u < pair.second.size()) ? "," : "");
            }
            fprintf(fp, "    ]%s\n", (++module_index < report.configs.size()) ? "," : "");
        }
        fprintf(fp, "  }\n");
        fprintf(fp, "}\n");
        fclose(fp);
    }

    void FinalizeReport() {
        if (m_ReportWritten.exchange(true)) {
            return;
        }

        WriteReport();
        Log(BML_LOG_INFO,
            "Probe report written to %s", m_ReportPath.string().c_str());
    }
};

bool BuiltinModuleProbe::IsEnabled() {
    const std::string mode = GetEnvironmentString("BML_INTTEST_MODE");
    return mode == "probe";
}

BuiltinModuleProbe::BuiltinModuleProbe(BML_Mod handle)
    : m_Impl(new Impl(handle)) {}

BuiltinModuleProbe::~BuiltinModuleProbe() {
    delete m_Impl;
    m_Impl = nullptr;
}

BML_Result BuiltinModuleProbe::OnAttach() {
    return m_Impl->OnAttach();
}

void BuiltinModuleProbe::OnDetach() {
    m_Impl->OnDetach();
    delete m_Impl;
    m_Impl = nullptr;
}
