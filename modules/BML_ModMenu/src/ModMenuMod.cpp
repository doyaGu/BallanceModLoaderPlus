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

#include <cstdio>
#include <cstring>
#include <string>

#define BML_LOADER_IMPLEMENTATION
#include "bml_module.hpp"
#include "bml_services.hpp"
#include "bml_engine_events.h"
#include "bml_engine_events.hpp"
#include "bml_imgui.hpp"
#include "bml_input.h"
#include "bml_input_control.h"
#include "bml_interface.hpp"
#include "bml_topics.h"
#include "bml_ui.hpp"
#include "bml_virtools.h"
#include "bml_virtools.hpp"
#include "bml_virtools_payloads.h"

#include "bml_menu.hpp"
#include "BML/Guids/Hooks.h"
#include "BML/Guids/Interface.h"
#include "BML/Guids/TT_Toolbox_RT.h"
#include "BML/ScriptGraph.h"
#include "CKAll.h"

#include "MenuRegistry.h"
#include "MenuRuntime.h"
#include "ModMenu.h"

namespace {
using CKBehaviorCallback = int (*)(const CKBehaviorContext *behcontext, void *arg);

CK2dEntity *FindNamed2dEntity(CKContext *context, const char *name) {
    if (!context || !name || !*name) {
        return nullptr;
    }
    return static_cast<CK2dEntity *>(context->GetObjectByNameAndClass((CKSTRING) name, CKCID_2DENTITY));
}

CKDataArray *FindNamedArray(CKContext *context, const char *name) {
    if (!context || !name || !*name) {
        return nullptr;
    }
    return static_cast<CKDataArray *>(context->GetObjectByNameAndClass((CKSTRING) name, CKCID_DATAARRAY));
}

CKBehavior *CreateHookBlock(bml::Graph &graph, CKBehaviorCallback callback, void *arg,
    int inputCount = 1, int outputCount = 1) {
    CKBehavior *behavior = graph.CreateBehavior(HOOKS_HOOKBLOCK_GUID).Get();
    if (!behavior) {
        return nullptr;
    }

    behavior->SetLocalParameterValue(0, &callback);
    behavior->SetLocalParameterValue(1, &arg);

    char name[16] = {};
    for (int index = 0; index < inputCount; ++index) {
        std::snprintf(name, sizeof(name), "In %d", index);
        behavior->CreateInput(name);
    }
    for (int index = 0; index < outputCount; ++index) {
        std::snprintf(name, sizeof(name), "Out %d", index);
        behavior->CreateOutput(name);
    }

    return behavior;
}
} // namespace

class ModMenuMod : public bml::Module {
public:
    static void DrawCallback(const BML_UIDrawContext *ctx, void *user_data) {
        auto *self = static_cast<ModMenuMod *>(user_data);
        if (!self || !ctx) {
            return;
        }
        if (!self->m_Visible) {
            return;
        }
        BML_IMGUI_SCOPE_FROM_CONTEXT(ctx);
        self->m_Menu.Render();
        self->m_Visible = self->m_Menu.IsOpen();
    }

    BML_Result OnAttach(bml::ModuleServices &services) override {
        m_Subs = services.CreateSubscriptions();

        m_DrawReg = bml::ui::RegisterDraw(Handle(), "bml.modmenu.window", 10, DrawCallback, this);
        if (!m_DrawReg) {
            return BML_RESULT_NOT_FOUND;
        }

        m_InputCaptureService = Services().Acquire<BML_InputCaptureInterface>();
        MenuRuntime::SetInputService(m_InputCaptureService.Get());

        m_Menu.Init();

        m_Subs.Add(BML_TOPIC_ENGINE_INIT, [this](const bml::imc::Message &msg) {
            auto *payload = bml::ValidateEnginePayload<BML_EngineInitEvent>(msg);
            if (!payload) return;
            SetContext(payload->context);
        });

        m_Subs.Add(BML_TOPIC_ENGINE_PLAY, [this](const bml::imc::Message &msg) {
            auto *payload = bml::ValidateEnginePayload<BML_EnginePlayEvent>(msg);
            if (payload) {
                SetContext(payload->context);
            } else if (!m_Context) {
                SetContext(bml::virtools::GetCKContext(Services()));
            }

            EnsureAssetsReady();
        });

        m_Subs.Add(BML_TOPIC_OBJECTLOAD_LOAD_SCRIPT, [this](const bml::imc::Message &msg) {
            auto *payload = msg.As<BML_ScriptLoadEvent>();
            if (!payload || !payload->script) {
                return;
            }

            const char *scriptName = payload->script->GetName();
            if (!scriptName || std::strcmp(scriptName, "Menu_Options") != 0) {
                return;
            }

            SetContext(static_cast<CKContext *>(payload->script->GetCKContext()));
            if (!m_OptionsEntryInstalled && PatchOptionsMenu(payload->script)) {
                m_OptionsEntryInstalled = true;
                Services().Log().Info("Inserted Mods button into Menu_Options");
            }
        });

        m_Subs.Add(BML_TOPIC_INPUT_KEY_UP, [](const bml::imc::Message &msg) {
            auto *event = msg.As<BML_KeyUpEvent>();
            if (!event) {
                return;
            }
            MenuRuntime::OnKeyUp(event->key_code);
        });

        m_Subs.Add(BML_TOPIC_INPUT_KEY_DOWN, [](const bml::imc::Message &msg) {
            auto *event = msg.As<BML_KeyDownEvent>();
            if (!event) {
                return;
            }
            MenuRuntime::OnKeyDown(event->key_code);
        });

        return m_Subs.Count() >= 6 ? BML_RESULT_OK : BML_RESULT_FAIL;
    }

    BML_Result OnPrepareDetach() override {
        CloseMenu();
        m_DrawReg.Reset();
        MenuRuntime::SetInputService(nullptr);
        m_InputCaptureService.Reset();
        return BML_RESULT_OK;
    }

    void OnDetach() override {
        CloseMenu();

        MenuRegistry::Instance().Reset();
        MenuRuntime::Reset();

        MenuRuntime::SetInputService(nullptr);
        m_InputCaptureService.Reset();
        m_DrawReg.Reset();

        m_Context = nullptr;
        m_AssetsReady = false;
        m_OptionsEntryInstalled = false;
    }

private:
    static int OpenMenuCallback(const CKBehaviorContext *, void *arg) {
        auto *self = static_cast<ModMenuMod *>(arg);
        if (self) {
            self->OpenMenu();
        }
        return CKBR_OK;
    }

    void SetContext(CKContext *context) {
        if (!context) {
            return;
        }

        m_Context = context;
        MenuRegistry::Instance().Initialize(m_Context, Services().Interfaces().Module, Services().Interfaces().Config);
        MenuRuntime::Initialize(m_Context, m_InputCaptureService.Get());
    }

    void EnsureAssetsReady() {
        if (m_AssetsReady || !m_Context) {
            return;
        }

        m_AssetsReady = Menu::InitTextures(m_Context) &&
            Menu::InitMaterials(m_Context) &&
            Menu::InitSounds(m_Context);
    }

    void OpenMenu() {
        if (!m_Context) {
            SetContext(bml::virtools::GetCKContext(Services()));
        }

        EnsureAssetsReady();
        if (!m_Context || !m_AssetsReady) {
            return;
        }

        m_Menu.Open("Mod List");
        m_Visible = m_Menu.IsOpen();
    }

    void CloseMenu() {
        if (!m_Visible) {
            return;
        }

        m_Menu.Close();
        m_Visible = false;
    }

    bool PatchOptionsMenu(CKBehavior *script) {
        if (!script) {
            return false;
        }

        CKContext *context = static_cast<CKContext *>(script->GetCKContext());
        if (!context) {
            return false;
        }

        if (FindNamed2dEntity(context, "M_Options_But_4")) {
            return true;
        }

        CK2dEntity *buttons[6] = {};
        buttons[0] = FindNamed2dEntity(context, "M_Options_Title");
        for (int i = 1; i < 4; ++i) {
            char buttonName[] = "M_Options_But_X";
            buttonName[14] = static_cast<char>('0' + i);
            buttons[i] = FindNamed2dEntity(context, buttonName);
        }
        buttons[5] = FindNamed2dEntity(context, "M_Options_But_Back");

        for (int i = 0; i < 4; ++i) {
            if (!buttons[i]) {
                return false;
            }
        }
        if (!buttons[5]) {
            return false;
        }

        buttons[4] = static_cast<CK2dEntity *>(context->CopyObject(buttons[1]));
        if (!buttons[4]) {
            return false;
        }
        buttons[4]->SetName("M_Options_But_4");

        for (int i = 0; i < 5; ++i) {
            Vx2DVector position;
            buttons[i]->GetPosition(position, true);
            position.y = 0.1f + 0.14f * static_cast<float>(i);
            buttons[i]->SetPosition(position, true);
        }

        CKDataArray *showHideArray = FindNamedArray(context, "Menu_Options_ShowHide");
        if (!showHideArray) {
            return false;
        }
        showHideArray->InsertRow(3);
        showHideArray->SetElementObject(3, 0, buttons[4]);
        CKBOOL show = TRUE;
        showHideArray->SetElementValue(3, 1, &show, sizeof(show));

        bml::Graph scriptGraph(script);
        CKBehavior *optionsGraph = scriptGraph.Find("Options Menu").Get();
        if (!optionsGraph) {
            return false;
        }

        bml::Graph options(optionsGraph);
        CKBehavior *upSop = nullptr;
        CKBehavior *downSop = nullptr;
        options.FindAll([&](CKBehavior *behavior) {
            CKBehavior *previous = options.From(behavior).Prev().Get();
            const char *name = previous ? previous->GetName() : nullptr;
            if (name && std::strcmp(name, "Set 2D Material") == 0) {
                upSop = behavior;
            }
            if (name && std::strcmp(name, "Send Message") == 0) {
                downSop = behavior;
            }
            return !(upSop && downSop);
        }, "Switch On Parameter");

        CKBehavior *upPs = nullptr;
        CKBehavior *downPs = nullptr;
        options.FindAll([&](CKBehavior *behavior) {
            CKBehavior *next = options.From(behavior).Next().Get();
            const char *name = next ? next->GetName() : nullptr;
            if (name && std::strcmp(name, "Keyboard") == 0) {
                upPs = behavior;
            }
            if (name && std::strcmp(name, "Send Message") == 0) {
                downPs = behavior;
            }
            return !(upPs && downPs);
        }, "Parameter Selector");

        if (!upSop || !downSop || !upPs || !downPs) {
            return false;
        }

        CKParameterLocal *pin = options.Param("Pin 5", CKPGUID_INT, 4);
        if (!pin) {
            return false;
        }

        upSop->CreateInputParameter("Pin 5", CKPGUID_INT)->SetDirectSource(pin);
        upSop->CreateOutput("Out 5");
        downSop->CreateInputParameter("Pin 5", CKPGUID_INT)->SetDirectSource(pin);
        downSop->CreateOutput("Out 5");
        upPs->CreateInputParameter("pIn 4", CKPGUID_INT)->SetDirectSource(pin);
        upPs->CreateInput("In 4");
        downPs->CreateInputParameter("pIn 4", CKPGUID_INT)->SetDirectSource(pin);
        downPs->CreateInput("In 4");

        CKBehavior *text2d = options.CreateBehavior(VT_INTERFACE_2DTEXT, true).Get();
        CKBehavior *pushButton = options.CreateBehavior(TT_TOOLBOX_RT_TTPUSHBUTTON2, true).Get();
        CKBehavior *text2dRef = options.Find("2D Text").Get();
        CKBehavior *nop = options.Find("Nop").Get();
        if (!text2d || !pushButton || !text2dRef || !nop) {
            return false;
        }

        CKParameterLocal *entity2d = options.Param("Button", CKPGUID_2DENTITY, static_cast<CKObject *>(buttons[4]));
        CKParameterLocal *buttonName = options.ParamString("Text", "Mods");
        if (!entity2d || !buttonName) {
            return false;
        }

        int textFlags = 0;
        text2dRef->GetLocalParameterValue(0, &textFlags);
        text2d->SetLocalParameterValue(0, &textFlags, sizeof(textFlags));

        text2d->GetTargetParameter()->SetDirectSource(entity2d);
        pushButton->GetTargetParameter()->SetDirectSource(entity2d);
        text2d->GetInputParameter(0)->ShareSourceWith(text2dRef->GetInputParameter(0));
        text2d->GetInputParameter(1)->SetDirectSource(buttonName);
        for (int i = 2; i < 6; ++i) {
            text2d->GetInputParameter(i)->ShareSourceWith(text2dRef->GetInputParameter(i));
        }

        CKBehaviorLink *link = options.From(upSop).NextLink(nullptr, 4, 0);
        if (!link) {
            return false;
        }
        link->SetInBehaviorIO(upSop->GetOutput(5));

        options.Link(upSop, text2d, 4, 0);
        options.Link(text2d, nop, 0, 0);
        options.Link(text2d, pushButton, 0, 0);

        link = options.From(upPs).PrevLink(nullptr, 1, 3);
        if (!link) {
            return false;
        }
        link->SetOutBehaviorIO(upPs->GetInput(4));

        link = options.From(downPs).PrevLink(nullptr, 2, 3);
        if (!link) {
            return false;
        }
        link->SetOutBehaviorIO(downPs->GetInput(4));

        options.Link(pushButton, upPs, 1, 3);
        options.Link(pushButton, downPs, 2, 3);

        optionsGraph->CreateOutput("Button 5 Pressed");
        options.Link(downSop, optionsGraph->GetOutput(4), 5);

        link = scriptGraph.From(optionsGraph).NextLink(nullptr, 3, 0);
        if (!link) {
            return false;
        }
        link->SetInBehaviorIO(optionsGraph->GetOutput(4));

        CKBehavior *modsMenu = CreateHookBlock(scriptGraph, OpenMenuCallback, this);
        CKBehavior *exit = scriptGraph.Find("Exit", false, 1, 0).Get();
        if (!modsMenu || !exit) {
            return false;
        }

        scriptGraph.Link(optionsGraph, modsMenu, 3, 0);
        scriptGraph.Link(modsMenu, exit, 0, 0);

        CKBehavior *keyboard = options.Find("Keyboard").Get();
        if (!keyboard) {
            return false;
        }

        bml::Graph keyboardGraph(keyboard);
        keyboardGraph.FindAll([&](CKBehavior *behavior) {
            CKParameter *source = behavior->GetInputParameter(0)->GetRealSource();
            if (!source || bml::GetParam<CKKEYBOARD>(source) != CKKEY_ESCAPE) {
                return true;
            }

            CKBehavior *identity = keyboardGraph.From(behavior).Next().Get();
            if (!identity || identity->GetInputParameterCount() <= 0) {
                return false;
            }

            CKParameter *identitySource = identity->GetInputParameter(0)->GetRealSource();
            if (!identitySource) {
                return false;
            }

            bml::SetParam(identitySource, 4);
            return false;
        }, "Secure Key");

        return true;
    }

    bml::imc::SubscriptionManager m_Subs;
    ModMenu m_Menu;
    CKContext *m_Context = nullptr;
    bml::ui::DrawRegistration m_DrawReg;
    bml::InterfaceLease<BML_InputCaptureInterface> m_InputCaptureService;
    bool m_Visible = false;
    bool m_AssetsReady = false;
    bool m_OptionsEntryInstalled = false;
};

BML_DEFINE_MODULE(ModMenuMod)
