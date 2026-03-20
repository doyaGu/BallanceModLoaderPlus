#define BML_LOADER_IMPLEMENTATION
#include "bml_module.hpp"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_set>

#include "bml_builtin_interfaces.h"
#include "bml_config.hpp"
#include "bml_core.hpp"
#include "bml_game_topics.h"
#include "bml_imgui.hpp"
#include "bml_interface.hpp"
#include "bml_topics.h"
#include "bml_ui.hpp"
#include "bml_virtools.h"
#include "bml_virtools.hpp"
#include "bml_virtools_payloads.h"

#include "BML/ScriptGraph.h"

#include "CKAll.h"

#include "FpsCounter.h"
#include "SRTimer.h"

namespace {
constexpr uint32_t kDefaultFpsUpdateFrequency = 30;
struct GameplaySettings {
    bool lanternAlphaTest = true;
    bool fixLifeBallFreeze = true;
    bool overclock = false;
    bool hudEnabled = true;
    bool showTitle = true;
    bool showFps = true;
    bool showSr = true;
    uint32_t fpsUpdateFrequency = kDefaultFpsUpdateFrequency;
    std::string titleText;
};

std::string FormatVersion(const BML_Version &version) {
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%u.%u.%u", version.major, version.minor, version.patch);
    return buffer;
}

std::string BuildDefaultTitle(const BML_CoreContextInterface *contextInterface) {
    if (auto runtimeVersion = bml::GetRuntimeVersion(contextInterface)) {
        return "BML Plus " + FormatVersion(*runtimeVersion);
    }
    return "BML Plus " BML_VERSION_STRING;
}

void AddOutlinedText(ImDrawList *drawList, ImFont *font, float size, ImVec2 pos, ImU32 color, const char *text) {
    if (!drawList || !font || !text || text[0] == '\0') {
        return;
    }

    const ImU32 shadowColor = IM_COL32(0, 0, 0, 220);
    drawList->AddText(font, size, ImVec2(pos.x + 1.0f, pos.y + 1.0f), shadowColor, text);
    drawList->AddText(font, size, pos, color, text);
}
} // namespace

class GameplayMod : public bml::Module {
    bml::imc::SubscriptionManager m_Subs;
    GameplaySettings m_Settings;
    std::unordered_set<CK_ID> m_PatchedScripts;
    CKBehaviorLink *m_OverclockLinks[3] = {};
    CKBehaviorIO *m_OverclockLinkIO[3][2] = {};
    bml::ui::DrawRegistration m_DrawReg;
    FpsCounter m_FpsCounter;
    SRTimer m_SRTimer;
    bool m_CheatEnabled = false;
    bool m_SrVisible = false;
    float m_SrAlpha = 1.0f;

    void EnsureDefaultConfig() {
        auto config = Services().Config();
        if (!config.GetBool("Tweak", "LanternAlphaTest").has_value()) {
            config.SetBool("Tweak", "LanternAlphaTest", true);
        }
        if (!config.GetBool("Tweak", "FixLifeBallFreeze").has_value()) {
            config.SetBool("Tweak", "FixLifeBallFreeze", true);
        }
        if (!config.GetBool("Tweak", "Overclock").has_value()) {
            config.SetBool("Tweak", "Overclock", false);
        }
        if (!config.GetBool("General", "Enabled").has_value()) {
            config.SetBool("General", "Enabled", true);
        }
        if (!config.GetString("General", "TitleText").has_value()) {
            const std::string defaultTitle = BuildDefaultTitle(Services().Builtins().Context);
            config.SetString("General", "TitleText", defaultTitle.c_str());
        }
        if (!config.GetBool("HUD", "ShowTitle").has_value()) {
            config.SetBool("HUD", "ShowTitle", true);
        }
        if (!config.GetBool("HUD", "ShowFPS").has_value()) {
            config.SetBool("HUD", "ShowFPS", true);
        }
        if (!config.GetBool("HUD", "ShowSRTimer").has_value()) {
            config.SetBool("HUD", "ShowSRTimer", true);
        }
        if (!config.GetInt("HUD", "FPSUpdateFrequency").has_value()) {
            config.SetInt("HUD", "FPSUpdateFrequency", static_cast<int32_t>(kDefaultFpsUpdateFrequency));
        }
    }

    void RefreshConfig() {
        auto config = Services().Config();
        m_Settings.lanternAlphaTest = config.GetBool("Tweak", "LanternAlphaTest").value_or(true);
        m_Settings.fixLifeBallFreeze = config.GetBool("Tweak", "FixLifeBallFreeze").value_or(true);
        m_Settings.overclock = config.GetBool("Tweak", "Overclock").value_or(false);
        m_Settings.hudEnabled = config.GetBool("General", "Enabled").value_or(true);
        m_Settings.titleText = config.GetString("General", "TitleText").value_or(BuildDefaultTitle(Services().Builtins().Context));
        m_Settings.showTitle = config.GetBool("HUD", "ShowTitle").value_or(true);
        m_Settings.showFps = config.GetBool("HUD", "ShowFPS").value_or(true);
        m_Settings.showSr = config.GetBool("HUD", "ShowSRTimer").value_or(true);
        m_Settings.fpsUpdateFrequency = static_cast<uint32_t>(std::max(
            1,
            config.GetInt("HUD", "FPSUpdateFrequency").value_or(static_cast<int32_t>(kDefaultFpsUpdateFrequency))));
        m_FpsCounter.SetUpdateFrequency(m_Settings.fpsUpdateFrequency);
    }

    CKContext *GetContext() const {
        return bml::virtools::GetCKContext(Services());
    }

    void ApplyLanternMaterial() const {
        CKContext *context = GetContext();
        if (!context) {
            return;
        }

        auto *material = static_cast<CKMaterial *>(context->GetObjectByNameAndClass("Laterne_Verlauf", CKCID_MATERIAL));
        if (!material) {
            return;
        }

        const CKBOOL alphaTest = m_Settings.lanternAlphaTest ? TRUE : FALSE;
        material->EnableAlphaTest(alphaTest);
        material->SetAlphaFunc(VXCMP_GREATEREQUAL);
        int ref = 0;
        material->SetAlphaRef(ref);
    }

    void ApplyOverclockSetting() {
        for (int index = 0; index < 3; ++index) {
            if (m_OverclockLinks[index] && m_OverclockLinkIO[index][0] && m_OverclockLinkIO[index][1]) {
                m_OverclockLinks[index]->SetOutBehaviorIO(
                    m_OverclockLinkIO[index][m_Settings.overclock ? 1 : 0]);
            }
        }
    }

    void PatchLevelInitBuild(CKBehavior *script) {
        bml::Graph graph(script);
        if (!graph.Find("Load LevelXX")) {
            return;
        }

        auto setAlphaTest = graph.Find("set Mapping and Textures")
                                 .Find("Set Mat Laterne")
                                 .Find("Set Alpha Test");
        if (!setAlphaTest) {
            return;
        }

        CKParameter *enabled = setAlphaTest->GetInputParameter(0)->GetDirectSource();
        if (enabled) {
            CKBOOL alphaTest = m_Settings.lanternAlphaTest ? TRUE : FALSE;
            enabled->SetValue(&alphaTest);
        }
    }

    void PatchExtraLifeFix(CKBehavior *script) {
        bml::Graph graph(script);
        auto emitter = graph.Find("SphericalParticleSystem");
        if (!emitter) {
            return;
        }

        auto *realTimeMode = emitter->CreateInputParameter("Real-Time Mode", CKPGUID_BOOL);
        if (realTimeMode) {
            realTimeMode->SetDirectSource(graph.Param("Real-Time Mode", CKPGUID_BOOL, CKBOOL(1)));
        }

        auto *deltaTime = emitter->CreateInputParameter("DeltaTime", CKPGUID_FLOAT);
        if (deltaTime) {
            deltaTime->SetDirectSource(graph.Param("DeltaTime", CKPGUID_FLOAT, 20.0f));
        }
    }

    void PatchGameplayIngame(CKBehavior *script) {
        bml::Graph graph(script);
        auto ballManager = graph.Find("BallManager");
        if (!ballManager) {
            return;
        }

        auto deactivateBall = ballManager.Find("Deactivate Ball");
        auto resetPieces = deactivateBall ? deactivateBall.Find("reset Ballpieces") : bml::Node{};
        if (deactivateBall && resetPieces) {
            m_OverclockLinks[0] = resetPieces.NextLink();
            if (m_OverclockLinks[0]) {
                bml::Graph deactGraph(deactivateBall);
                auto unphysicalize = deactGraph.From(
                    m_OverclockLinks[0]->GetOutBehaviorIO()->GetOwner()).Next().Next();
                if (unphysicalize) {
                    m_OverclockLinkIO[0][1] = unphysicalize->GetInput(1);
                }
            }
        }

        auto newBall = ballManager.Find("New Ball");
        auto physNewBall = newBall ? newBall.Find("physicalize new Ball") : bml::Node{};
        if (newBall && physNewBall) {
            m_OverclockLinks[1] = physNewBall.SkipBack(3).PrevLink();
            if (m_OverclockLinks[1]) {
                m_OverclockLinkIO[1][1] = physNewBall->GetInput(0);
            }
        }

        ApplyOverclockSetting();
    }

    void PatchGameplayEnergy(CKBehavior *script) {
        bml::Graph graph(script);
        auto delay = graph.Find("Delayer");
        if (!delay) {
            return;
        }

        m_OverclockLinks[2] = delay.PrevLink();
        CKBehaviorLink *next = delay.NextLink();
        if (next) {
            m_OverclockLinkIO[2][1] = next->GetOutBehaviorIO();
        }

        for (int index = 0; index < 3; ++index) {
            if (m_OverclockLinks[index]) {
                m_OverclockLinkIO[index][0] = m_OverclockLinks[index]->GetOutBehaviorIO();
            }
        }

        ApplyOverclockSetting();
    }

    void HandleScriptLoad(const BML_ScriptLoadEvent *payload) {
        if (!payload || !payload->script) {
            return;
        }

        CKBehavior *script = payload->script;
        if (!m_PatchedScripts.insert(script->GetID()).second) {
            return;
        }

        RefreshConfig();
        ApplyLanternMaterial();

        const char *name = script->GetName();
        if (!name) {
            return;
        }

        if (std::strcmp(name, "Levelinit_build") == 0) {
            PatchLevelInitBuild(script);
        } else if (std::strcmp(name, "Gameplay_Ingame") == 0) {
            PatchGameplayIngame(script);
        } else if (std::strcmp(name, "Gameplay_Energy") == 0) {
            PatchGameplayEnergy(script);
        } else if (m_Settings.fixLifeBallFreeze &&
                   (std::strcmp(name, "P_Extra_Life_Particle_Blob Script") == 0 ||
                    std::strcmp(name, "P_Extra_Life_Particle_Fizz Script") == 0)) {
            PatchExtraLifeFix(script);
        }
    }

    void TickHud(float deltaSeconds) {
        if (deltaSeconds <= 0.0f) {
            return;
        }

        RefreshConfig();

        m_FpsCounter.Update(deltaSeconds);
        m_SRTimer.Update(deltaSeconds);

        const float targetAlpha = m_SRTimer.IsRunning() ? 1.0f : 0.5f;
        const float fadeRate = 3.0f * deltaSeconds;
        if (m_SrAlpha < targetAlpha) {
            m_SrAlpha = std::min(targetAlpha, m_SrAlpha + fadeRate);
        } else if (m_SrAlpha > targetAlpha) {
            m_SrAlpha = std::max(targetAlpha, m_SrAlpha - fadeRate);
        }

        if (m_FpsCounter.IsDirty()) {
            m_FpsCounter.ClearDirty();
        }
        if (m_SRTimer.IsDirty()) {
            m_SRTimer.ClearDirty();
        }
    }

    void RenderTitle(const ImVec2 &viewportPos, const ImVec2 &viewportSize) const {
        if (!m_Settings.hudEnabled || !m_Settings.showTitle || m_Settings.titleText.empty()) {
            return;
        }

        ImDrawList *drawList = ImGui::GetForegroundDrawList();
        ImFont *font = ImGui::GetFont();
        const float size = ImGui::GetFontSize() * 1.2f;
        const ImVec2 textSize = font->CalcTextSizeA(size, FLT_MAX, 0.0f, m_Settings.titleText.c_str());
        const ImVec2 pos(
            viewportPos.x + (viewportSize.x - textSize.x) * 0.5f,
            viewportPos.y + viewportSize.y * 0.02f);
        AddOutlinedText(drawList, font, size, pos, IM_COL32_WHITE, m_Settings.titleText.c_str());
    }

    void RenderFps(const ImVec2 &viewportPos) const {
        if (!m_Settings.hudEnabled || !m_Settings.showFps) {
            return;
        }

        AddOutlinedText(
            ImGui::GetForegroundDrawList(),
            ImGui::GetFont(),
            ImGui::GetFontSize(),
            ImVec2(viewportPos.x + 12.0f, viewportPos.y + 12.0f),
            IM_COL32_WHITE,
            m_FpsCounter.GetFormattedFps());
    }

    void RenderSrTimer(const ImVec2 &viewportPos, const ImVec2 &viewportSize) const {
        if (!m_Settings.hudEnabled || !m_Settings.showSr || !m_SrVisible) {
            return;
        }

        ImDrawList *drawList = ImGui::GetForegroundDrawList();
        ImFont *font = ImGui::GetFont();
        const float alpha = std::clamp(m_SrAlpha, 0.0f, 1.0f);
        const ImU32 color = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, alpha));
        const float baseX = viewportPos.x + viewportSize.x * 0.03f;
        const float baseY = viewportPos.y + viewportSize.y * 0.845f;

        AddOutlinedText(drawList, font, ImGui::GetFontSize(), ImVec2(baseX, baseY), color, Services().Locale()["hud.sr_timer"]);
        AddOutlinedText(
            drawList,
            font,
            ImGui::GetFontSize(),
            ImVec2(baseX, baseY + ImGui::GetFontSize() * 1.2f),
            color,
            m_SRTimer.GetFormattedTime());
    }

    void RenderCheatState(const ImVec2 &viewportPos, const ImVec2 &viewportSize) const {
        if (!m_Settings.hudEnabled || !m_CheatEnabled) {
            return;
        }

        const char *cheatText = Services().Locale()["hud.cheat_enabled"];
        ImDrawList *drawList = ImGui::GetForegroundDrawList();
        ImFont *font = ImGui::GetFont();
        const float size = ImGui::GetFontSize();
        const ImVec2 textSize = font->CalcTextSizeA(size, FLT_MAX, 0.0f, cheatText);
        const ImVec2 pos(
            viewportPos.x + (viewportSize.x - textSize.x) * 0.5f,
            viewportPos.y + viewportSize.y * 0.88f);
        AddOutlinedText(drawList, font, size, pos, IM_COL32(255, 200, 60, 255), cheatText);
    }

    void RenderHud() const {
        ImGuiViewport *viewport = ImGui::GetMainViewport();
        if (!viewport) {
            return;
        }

        RenderTitle(viewport->Pos, viewport->Size);
        RenderFps(viewport->Pos);
        RenderSrTimer(viewport->Pos, viewport->Size);
        RenderCheatState(viewport->Pos, viewport->Size);
    }

public:
    GameplayMod()
        : m_FpsCounter(90) {
    }

    static void DrawOverlay(const BML_UIDrawContext *ctx, void *userData) {
        auto *self = static_cast<GameplayMod *>(userData);
        if (!self || !ctx) {
            return;
        }

        BML_IMGUI_SCOPE_FROM_CONTEXT(ctx);
        self->RenderHud();
    }

    BML_Result OnAttach(bml::ModuleServices &services) override {
        m_Subs = services.CreateSubscriptions();

        Services().Locale().Load(nullptr);

        EnsureDefaultConfig();
        RefreshConfig();
        ApplyLanternMaterial();

        m_DrawReg = bml::ui::RegisterDraw("bml.gameplay.overlay", 25, DrawOverlay, this);
        if (!m_DrawReg) {
            return BML_RESULT_NOT_FOUND;
        }

        m_Subs.Add(BML_TOPIC_STATE_CHEAT_ENABLED, [this](const bml::imc::Message &msg) {
            const auto *value = msg.As<BML_Bool>();
            m_CheatEnabled = value && (*value == BML_TRUE);
        });

        m_Subs.Add(BML_TOPIC_OBJECTLOAD_LOAD_SCRIPT, [this](const bml::imc::Message &msg) {
            HandleScriptLoad(msg.As<BML_ScriptLoadEvent>());
        });
        m_Subs.Add(BML_TOPIC_ENGINE_POST_PROCESS, [this](const bml::imc::Message &msg) {
            const float *dt = msg.As<float>();
            TickHud(dt ? *dt : (1.0f / 60.0f));
        });
        m_Subs.Add(BML_TOPIC_GAME_MENU_PRE_START, [this](const bml::imc::Message &) {
            m_SrVisible = false;
            m_SrAlpha = 1.0f;
        });
        m_Subs.Add(BML_TOPIC_GAME_LEVEL_START, [this](const bml::imc::Message &) {
            m_SRTimer.Reset();
            m_SrVisible = true;
            m_SrAlpha = 1.0f;
        });
        m_Subs.Add(BML_TOPIC_GAME_LEVEL_POST_EXIT, [this](const bml::imc::Message &) {
            m_SrVisible = false;
        });
        m_Subs.Add(BML_TOPIC_GAME_LEVEL_PAUSE, [this](const bml::imc::Message &) {
            m_SRTimer.Pause();
        });
        m_Subs.Add(BML_TOPIC_GAME_LEVEL_UNPAUSE, [this](const bml::imc::Message &) {
            m_SRTimer.Start();
        });
        m_Subs.Add(BML_TOPIC_GAME_PLAY_COUNTER_ACTIVE, [this](const bml::imc::Message &) {
            m_SRTimer.Start();
        });
        m_Subs.Add(BML_TOPIC_GAME_PLAY_COUNTER_INACTIVE, [this](const bml::imc::Message &) {
            m_SRTimer.Pause();
        });

        if (m_Subs.Count() < 10) {
            m_DrawReg.Reset();
            Services().Log().Error("Failed to subscribe to gameplay and HUD topics");
            return BML_RESULT_FAIL;
        }

        Services().Log().Info("Gameplay module initialized");
        return BML_RESULT_OK;
    }

    BML_Result OnPrepareDetach() override {
        m_DrawReg.Reset();
        return BML_RESULT_OK;
    }

    void OnDetach() override {
        m_PatchedScripts.clear();
        for (int index = 0; index < 3; ++index) {
            m_OverclockLinks[index] = nullptr;
            m_OverclockLinkIO[index][0] = nullptr;
            m_OverclockLinkIO[index][1] = nullptr;
        }

        m_DrawReg.Reset();
        m_CheatEnabled = false;
        m_SrVisible = false;
        m_SrAlpha = 1.0f;
    }
};

BML_DEFINE_MODULE(GameplayMod)
