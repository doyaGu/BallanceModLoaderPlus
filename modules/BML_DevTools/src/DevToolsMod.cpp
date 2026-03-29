#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <memory>
#include <vector>

#define BML_LOADER_IMPLEMENTATION
#include "bml_module.hpp"
#include "bml_services.hpp"
#include "bml_input.h"
#include "bml_topics.h"
#include "bml_ui.hpp"

#include "imgui.h"

#include "Panel.h"
#include "MemoryPanel.h"
#include "ProfilingPanel.h"
#include "FaultPanel.h"
#include "ImcPanel.h"
#include "InterfacePanel.h"
#include "HookPanel.h"
#include "ModulePanel.h"

namespace {

class DevToolsMod : public bml::Module {
    bml::ui::DrawRegistration m_DrawReg;
    bml::imc::SubscriptionManager m_Subs;
    bool m_Visible = false;
    std::vector<std::unique_ptr<devtools::Panel>> m_Panels;

    static void OnDraw(const BML_UIDrawContext *, void *userData) {
        static_cast<DevToolsMod *>(userData)->Draw();
    }

    void InitPanels() {
        m_Panels.push_back(std::make_unique<devtools::MemoryPanel>());
        m_Panels.push_back(std::make_unique<devtools::ProfilingPanel>());
        m_Panels.push_back(std::make_unique<devtools::FaultPanel>());
        m_Panels.push_back(std::make_unique<devtools::ImcPanel>());
        m_Panels.push_back(std::make_unique<devtools::InterfacePanel>());
        m_Panels.push_back(std::make_unique<devtools::HookPanel>());
        m_Panels.push_back(std::make_unique<devtools::ModulePanel>());
    }

    void RefreshAll() {
        for (auto &p : m_Panels)
            p->Refresh(Services());
    }

    void SampleAll() {
        for (auto &p : m_Panels)
            p->Sample(Services());
    }

    void Draw() {
        if (!m_Visible) return;

        const auto &loc = Services().Locale();
        ImGui::SetNextWindowSize(ImVec2(800, 550), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(loc["window.title"], &m_Visible, ImGuiWindowFlags_MenuBar)) {
            ImGui::End();
            return;
        }

        if (ImGui::BeginMenuBar()) {
            if (ImGui::MenuItem(loc["menu.refresh"])) RefreshAll();
            ImGui::EndMenuBar();
        }

        if (ImGui::BeginTabBar("DevToolsTabs")) {
            for (auto &panel : m_Panels) {
                if (ImGui::BeginTabItem(panel->Label(loc))) {
                    panel->Draw(Services());
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }

        ImGui::End();
    }

public:
    BML_Result OnAttach() override {
        Services().Locale().Load(nullptr);

        InitPanels();

        m_DrawReg = bml::ui::RegisterDraw(Handle(), "bml.devtools.overlay", 900, OnDraw, this);

        m_Subs = Services().CreateSubscriptions();

        m_Subs.Add(BML_TOPIC_INPUT_KEY_DOWN, [this](const bml::imc::Message &msg) {
            const auto *event = msg.As<BML_KeyDownEvent>();
            if (!event || event->repeat) return;
            if (event->key_code == 0x58) {
                m_Visible = !m_Visible;
                if (m_Visible) RefreshAll();
            }
        });

        m_Subs.Add(BML_TOPIC_ENGINE_POST_PROCESS, [this](const bml::imc::Message &) {
            if (m_Visible) SampleAll();
        });

        return BML_RESULT_OK;
    }

    void OnDetach() override {
        m_Panels.clear();
        m_Subs.Clear();
        m_DrawReg.Reset();
    }
};

} // namespace

BML_DEFINE_MODULE(DevToolsMod)
