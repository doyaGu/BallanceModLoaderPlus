#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <Windows.h>
#include <dinput.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "imgui.h"

#include "bml_config.h"
#include "bml_extension.h"
#include "bml_input.h"
#include "bml_module.h"
#include "bml_topics.h"
#include "bml_ui.h"

#define BML_LOADER_IMPLEMENTATION
#include "bml_loader.h"

namespace {
constexpr size_t kStringBufferSize = 512;

struct ModuleInfo {
    BML_Mod handle = nullptr;
    std::string id;
    BML_Version version = BML_VERSION_INIT(0, 0, 0);
};

struct ConfigEntry {
    std::string category;
    std::string name;
    BML_ConfigType type = BML_CONFIG_STRING;
    bool boolValue = false;
    int intValue = 0;
    float floatValue = 0.0f;
    std::string stringValue;
    std::string draftString;
};

BML_Mod g_ModHandle = nullptr;
BML_Subscription g_DrawSub = nullptr;
BML_Subscription g_KeyDownSub = nullptr;
BML_TopicId g_TopicDraw = 0;
BML_TopicId g_TopicKeyDown = 0;
static BML_UI_ApiTable *g_UiApi = nullptr;
PFN_bmlUIGetImGuiContext g_GetImGuiContext = nullptr;
BML_InputExtension *g_InputExtension = nullptr;
bool g_InputExtensionLoaded = false;

bool g_Visible = false;
std::vector<ModuleInfo> g_Modules;
std::vector<ConfigEntry> g_ConfigEntries;
int g_SelectedModuleIndex = 0;

std::string FormatVersion(const BML_Version &version) {
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%u.%u.%u", version.major, version.minor, version.patch);
    return buffer;
}

void SetVisible(bool visible) {
    if (g_Visible == visible) {
        return;
    }
    g_Visible = visible;
    if (g_InputExtension) {
        if (visible) {
            g_InputExtension->BlockDevice(BML_INPUT_DEVICE_KEYBOARD);
            g_InputExtension->BlockDevice(BML_INPUT_DEVICE_MOUSE);
            g_InputExtension->ShowCursor(true);
        } else {
            g_InputExtension->UnblockDevice(BML_INPUT_DEVICE_MOUSE);
            g_InputExtension->UnblockDevice(BML_INPUT_DEVICE_KEYBOARD);
            g_InputExtension->ShowCursor(false);
        }
    }
}

bool ValidateModuleIndex(int index) {
    return index >= 0 && index < static_cast<int>(g_Modules.size());
}

void RefreshModules() {
    std::string previousSelection;
    if (ValidateModuleIndex(g_SelectedModuleIndex)) {
        previousSelection = g_Modules[static_cast<size_t>(g_SelectedModuleIndex)].id;
    }

    g_Modules.clear();
    if (!bmlGetLoadedModuleCount || !bmlGetLoadedModuleAt) {
        g_SelectedModuleIndex = 0;
        return;
    }

    const uint32_t count = bmlGetLoadedModuleCount();
    g_Modules.reserve(count);
    for (uint32_t index = 0; index < count; ++index) {
        BML_Mod mod = bmlGetLoadedModuleAt(index);
        if (!mod) {
            continue;
        }
        ModuleInfo info;
        info.handle = mod;
        const char *id = nullptr;
        if (bmlGetModId && bmlGetModId(mod, &id) == BML_RESULT_OK && id) {
            info.id = id;
        } else {
            info.id = "<unknown>";
        }
        if (bmlGetModVersion) {
            bmlGetModVersion(mod, &info.version);
        }
        g_Modules.push_back(std::move(info));
    }

    std::sort(g_Modules.begin(), g_Modules.end(), [](const ModuleInfo &lhs, const ModuleInfo &rhs) {
        return lhs.id < rhs.id;
    });

    g_SelectedModuleIndex = 0;
    if (!previousSelection.empty()) {
        for (size_t index = 0; index < g_Modules.size(); ++index) {
            if (g_Modules[index].id == previousSelection) {
                g_SelectedModuleIndex = static_cast<int>(index);
                break;
            }
        }
    }
}

void ConfigEnumCallback(BML_Context, const BML_ConfigKey *key, const BML_ConfigValue *value, void *user_data) {
    if (!key || !value || !user_data) {
        return;
    }

    auto *entries = static_cast<std::vector<ConfigEntry> *>(user_data);
    ConfigEntry entry;
    entry.category = key->category ? key->category : "";
    entry.name = key->name ? key->name : "";
    entry.type = value->type;
    switch (value->type) {
    case BML_CONFIG_BOOL:
        entry.boolValue = value->data.bool_value != 0;
        break;
    case BML_CONFIG_INT:
        entry.intValue = value->data.int_value;
        break;
    case BML_CONFIG_FLOAT:
        entry.floatValue = value->data.float_value;
        break;
    case BML_CONFIG_STRING:
        entry.stringValue = value->data.string_value ? value->data.string_value : "";
        entry.draftString = entry.stringValue;
        break;
    default:
        break;
    }
    entries->push_back(std::move(entry));
}

void RefreshSelectedConfig() {
    g_ConfigEntries.clear();
    if (!ValidateModuleIndex(g_SelectedModuleIndex) || !bmlConfigEnumerate) {
        return;
    }

    bmlConfigEnumerate(g_Modules[static_cast<size_t>(g_SelectedModuleIndex)].handle, ConfigEnumCallback, &g_ConfigEntries);
    std::sort(g_ConfigEntries.begin(), g_ConfigEntries.end(), [](const ConfigEntry &lhs, const ConfigEntry &rhs) {
        if (lhs.category != rhs.category) {
            return lhs.category < rhs.category;
        }
        return lhs.name < rhs.name;
    });
}

bool CommitBool(ConfigEntry &entry, bool value) {
    if (!ValidateModuleIndex(g_SelectedModuleIndex) || !bmlConfigSet) {
        return false;
    }
    BML_ConfigKey key = BML_CONFIG_KEY_INIT(entry.category.c_str(), entry.name.c_str());
    BML_ConfigValue configValue = BML_CONFIG_VALUE_INIT_BOOL(value ? BML_TRUE : BML_FALSE);
    if (bmlConfigSet(g_Modules[static_cast<size_t>(g_SelectedModuleIndex)].handle, &key, &configValue) != BML_RESULT_OK) {
        return false;
    }
    entry.boolValue = value;
    return true;
}

bool CommitInt(ConfigEntry &entry, int value) {
    if (!ValidateModuleIndex(g_SelectedModuleIndex) || !bmlConfigSet) {
        return false;
    }
    BML_ConfigKey key = BML_CONFIG_KEY_INIT(entry.category.c_str(), entry.name.c_str());
    BML_ConfigValue configValue = BML_CONFIG_VALUE_INIT_INT(value);
    if (bmlConfigSet(g_Modules[static_cast<size_t>(g_SelectedModuleIndex)].handle, &key, &configValue) != BML_RESULT_OK) {
        return false;
    }
    entry.intValue = value;
    return true;
}

bool CommitFloat(ConfigEntry &entry, float value) {
    if (!ValidateModuleIndex(g_SelectedModuleIndex) || !bmlConfigSet) {
        return false;
    }
    BML_ConfigKey key = BML_CONFIG_KEY_INIT(entry.category.c_str(), entry.name.c_str());
    BML_ConfigValue configValue = BML_CONFIG_VALUE_INIT_FLOAT(value);
    if (bmlConfigSet(g_Modules[static_cast<size_t>(g_SelectedModuleIndex)].handle, &key, &configValue) != BML_RESULT_OK) {
        return false;
    }
    entry.floatValue = value;
    return true;
}

bool CommitString(ConfigEntry &entry, const std::string &value) {
    if (!ValidateModuleIndex(g_SelectedModuleIndex) || !bmlConfigSet) {
        return false;
    }
    BML_ConfigKey key = BML_CONFIG_KEY_INIT(entry.category.c_str(), entry.name.c_str());
    BML_ConfigValue configValue = BML_CONFIG_VALUE_INIT_STRING(value.c_str());
    if (bmlConfigSet(g_Modules[static_cast<size_t>(g_SelectedModuleIndex)].handle, &key, &configValue) != BML_RESULT_OK) {
        return false;
    }
    entry.stringValue = value;
    entry.draftString = value;
    return true;
}

bool ResetEntry(const ConfigEntry &entry) {
    if (!ValidateModuleIndex(g_SelectedModuleIndex) || !bmlConfigReset) {
        return false;
    }
    BML_ConfigKey key = BML_CONFIG_KEY_INIT(entry.category.c_str(), entry.name.c_str());
    return bmlConfigReset(g_Modules[static_cast<size_t>(g_SelectedModuleIndex)].handle, &key) == BML_RESULT_OK;
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
    if (g_KeyDownSub) {
        bmlImcUnsubscribe(g_KeyDownSub);
        g_KeyDownSub = nullptr;
    }
}

void RenderModuleList() {
    if (ImGui::BeginChild("##module-list", ImVec2(240.0f, 0.0f), true)) {
        for (size_t index = 0; index < g_Modules.size(); ++index) {
            const ModuleInfo &module = g_Modules[index];
            const bool selected = static_cast<int>(index) == g_SelectedModuleIndex;
            if (ImGui::Selectable(module.id.c_str(), selected)) {
                g_SelectedModuleIndex = static_cast<int>(index);
                RefreshSelectedConfig();
            }
            ImGui::SameLine();
            ImGui::TextDisabled("v%s", FormatVersion(module.version).c_str());
        }
    }
    ImGui::EndChild();
}

void RenderConfigPanel() {
    if (ImGui::BeginChild("##config-panel", ImVec2(0.0f, 0.0f), true)) {
        if (!ValidateModuleIndex(g_SelectedModuleIndex)) {
            ImGui::TextUnformatted("No loaded modules.");
            ImGui::EndChild();
            return;
        }

        const ModuleInfo &module = g_Modules[static_cast<size_t>(g_SelectedModuleIndex)];
        ImGui::Text("Module: %s", module.id.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Refresh")) {
            RefreshModules();
            RefreshSelectedConfig();
        }
        ImGui::Separator();

        if (g_ConfigEntries.empty()) {
            ImGui::TextUnformatted("No config entries registered for this module.");
            ImGui::EndChild();
            return;
        }

        std::string currentCategory;
        for (size_t index = 0; index < g_ConfigEntries.size(); ++index) {
            ConfigEntry &entry = g_ConfigEntries[index];
            if (entry.category != currentCategory) {
                currentCategory = entry.category;
                ImGui::SeparatorText(currentCategory.c_str());
            }

            ImGui::PushID(static_cast<int>(index));
            const std::string label = entry.name + "##value";

            switch (entry.type) {
            case BML_CONFIG_BOOL: {
                bool value = entry.boolValue;
                if (ImGui::Checkbox(label.c_str(), &value)) {
                    CommitBool(entry, value);
                }
                break;
            }
            case BML_CONFIG_INT: {
                int value = entry.intValue;
                if (ImGui::InputInt(label.c_str(), &value)) {
                    CommitInt(entry, value);
                }
                break;
            }
            case BML_CONFIG_FLOAT: {
                float value = entry.floatValue;
                if (ImGui::InputFloat(label.c_str(), &value, 0.1f, 1.0f, "%.3f")) {
                    CommitFloat(entry, value);
                }
                break;
            }
            case BML_CONFIG_STRING: {
                char buffer[kStringBufferSize] = {};
                std::strncpy(buffer, entry.draftString.c_str(), kStringBufferSize - 1);
                if (ImGui::InputText(label.c_str(), buffer, sizeof(buffer))) {
                    entry.draftString = buffer;
                }
                if (ImGui::IsItemDeactivatedAfterEdit() && entry.draftString != entry.stringValue) {
                    CommitString(entry, entry.draftString);
                }
                break;
            }
            default:
                ImGui::TextDisabled("%s: unsupported", entry.name.c_str());
                break;
            }

            ImGui::SameLine();
            if (ImGui::SmallButton("Reset")) {
                if (ResetEntry(entry)) {
                    RefreshSelectedConfig();
                    ImGui::PopID();
                    break;
                }
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
}

void RenderWindow() {
    if (!g_Visible) {
        return;
    }

    ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + viewport->Size.x * 0.08f, viewport->Pos.y + viewport->Size.y * 0.08f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x * 0.84f, viewport->Size.y * 0.78f), ImGuiCond_Always);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("BML Module Menu", nullptr, flags)) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            SetVisible(false);
        }
        RenderModuleList();
        ImGui::SameLine();
        RenderConfigPanel();
    }
    ImGui::End();
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
    RenderWindow();
}

void OnKeyDown(BML_Context, BML_TopicId, const BML_ImcMessage *msg, void *) {
    if (!msg || !msg->data || msg->size < sizeof(BML_KeyDownEvent)) {
        return;
    }
    const auto *event = static_cast<const BML_KeyDownEvent *>(msg->data);
    if (event->repeat) {
        return;
    }
    if (event->key_code == DIK_F1) {
        if (!g_Visible) {
            RefreshModules();
            RefreshSelectedConfig();
        }
        SetVisible(!g_Visible);
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

    BML_Version requiredVersion = bmlMakeVersion(1, 0, 0);
    result = bmlExtensionLoad(BML_INPUT_EXTENSION_NAME, &requiredVersion, reinterpret_cast<void **>(&g_InputExtension), nullptr);
    if (result == BML_RESULT_OK) {
        g_InputExtensionLoaded = true;
    } else {
        g_InputExtension = nullptr;
        g_InputExtensionLoaded = false;
        bmlLog(bmlGetGlobalContext(), BML_LOG_WARN, "BML_ModMenu", "Failed to load BML_EXT_Input; menu will not capture input");
    }

    RefreshModules();
    RefreshSelectedConfig();

    bmlImcGetTopicId(BML_TOPIC_UI_DRAW, &g_TopicDraw);
    bmlImcGetTopicId(BML_TOPIC_INPUT_KEY_DOWN, &g_TopicKeyDown);

    if ((result = bmlImcSubscribe(g_TopicDraw, OnDraw, nullptr, &g_DrawSub)) != BML_RESULT_OK) {
        bmlUnloadAPI();
        g_ModHandle = nullptr;
        return result;
    }
    if ((result = bmlImcSubscribe(g_TopicKeyDown, OnKeyDown, nullptr, &g_KeyDownSub)) != BML_RESULT_OK) {
        ReleaseSubscriptions();
        bmlUnloadAPI();
        g_ModHandle = nullptr;
        return result;
    }

    bmlLog(bmlGetGlobalContext(), BML_LOG_INFO, "BML_ModMenu", "Module menu initialized; press F1 to open");
    return BML_RESULT_OK;
}

BML_Result HandleDetach(const BML_ModDetachArgs *args) {
    const BML_Result validation = ValidateDetachArgs(args);
    if (validation != BML_RESULT_OK) {
        return validation;
    }

    SetVisible(false);
    ReleaseSubscriptions();
    if (g_InputExtensionLoaded) {
        bmlExtensionUnload(BML_INPUT_EXTENSION_NAME);
        g_InputExtensionLoaded = false;
    }
    g_InputExtension = nullptr;
    g_Modules.clear();
    g_ConfigEntries.clear();
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