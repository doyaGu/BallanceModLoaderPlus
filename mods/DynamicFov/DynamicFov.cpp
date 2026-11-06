#include "DynamicFov.h"

BML_DEFINE_MOD(DynamicFov, {
    .Id = "DynamicFov",
    .Name = "Dynamic Fov",
    .Version = BML_VERSION,
    .Author = "Gamepiaynmo",
    .Description = "Dynamically adjust camera fov according to ball speed.",
    .MinimumLoaderVersion = BML_VERSION,
})

void DynamicFov::OnLoad(const BML::Mod::Context &context) {
    m_Context = context;
    m_Timers = ModSupport::Timers(context);
    m_Context.Config().SetCategoryComment("Misc", "Miscellaneous").Take();
    m_Enabled = m_Context.Config()
                    .DeclareProperty({"Misc", "Enable", bool(false), "Enable Dynamic Fov"})
                    .Take();
}

void DynamicFov::OnLoadObject(const BML::Mod::Context &, const BML::Mod::LoadObjectEvent &event) {
    const char *filename = event.Filename.data();
    const bool isMap = event.IsMap;
    const char *masterName = event.MasterName.data();
    const auto filterClass = static_cast<CK_CLASSID>(event.FilterClass);
    const bool addToScene = event.AddToScene, addtoscene = event.AddToScene,
               reuseMeshes = event.ReuseMeshes, reuseMaterials = event.ReuseMaterials,
               dynamic = event.Dynamic;
    XObjectArray objects;
    for (auto ref : event.Objects) {
        auto p = BML::Engine::BorrowObject<CKObject>(m_Context, ref);
        if (p)
            objects.PushBack(p.Value()->GetID());
    }
    auto *objArray = &objects;
    auto master = BML::Engine::BorrowObject<CKObject>(m_Context, event.MasterObject);
    CKObject *masterObj = master ? master.Value() : nullptr;

    if (ModSupport::ResourceIs(filename, "3D Entities\\Camera.nmo")) {
        m_InGameCam = ModSupport::Find<CKTargetCamera>(m_Context, "InGameCam");
    }

    if (ModSupport::ResourceIs(filename, "3D Entities\\Gameplay.nmo")) {
        m_CurLevel = ModSupport::Find<CKDataArray>(m_Context, "CurrentLevel");
    }
}

void DynamicFov::OnLoadScript(const BML::Mod::Context &, const BML::Mod::LoadScriptEvent &event) {
    const char *filename = event.Filename.data();
    auto borrowed = BML::Engine::BorrowObject<CKBehavior>(m_Context, event.Script);
    if (!borrowed)
        return;
    auto *script = borrowed.Value();

    if (!strcmp(script->GetName(), "Gameplay_Ingame")) {
        m_IngameScript = script;
        CKBehavior *ballMgr = ScriptHelper::FindFirstBB(script, "BallManager");
        auto hook =
            ModSupport::Session(m_Behaviors, m_Context)
                .SpawnIn(script, BML::Behavior::Hook([this](const BML::Behavior::HookEvent &) {
                             SetInactive();
                             return BML::Behavior::HookResult::Ok;
                         }));
        ModSupport::Check(hook.Code());
        auto graph = hook.Value().Inspect();
        ModSupport::Check(graph.Code());
        auto block =
            BML::Engine::BorrowObject<CKBehavior>(m_Context, graph.Value().Root().Object());
        ModSupport::Check(block.Code());
        ScriptHelper::CreateLink(script, ballMgr, block.Value());
        m_Hooks.push_back(hook.Take());
        CKBehavior *init = ScriptHelper::FindFirstBB(script, "Init Ingame");
        m_DynamicPos = ScriptHelper::FindNextBB(script, init, "TT Set Dynamic Position");
    }
}

void DynamicFov::OnProcess(const BML::Mod::Context &) {
    if (m_IngameScript && m_InGameCam && m_Enabled.Get<bool>() &&
        ModSupport::Find<CK3dObject>(m_Context, "PS_FourFlames_01_Dual") == nullptr) {
        if (m_IngameScript->IsActive() && m_DynamicPos->IsActive() &&
            !m_DynamicPos->IsOutputActive(1)) {
            auto *ball = (CK3dObject *)m_CurLevel->GetElementObject(0, 1);
            if (ball != nullptr) {
                VxVector position;
                ball->GetPosition(&position);
                CKRenderContext *rc = ModSupport::Render(m_Context);

                if (!m_IsActive) {
                    m_LastPos = position;
                    m_InGameCam->SetFov(0.75f * rc->GetWidth() / rc->GetHeight());
                } else {
                    float delta =
                        ModSupport::Engine(m_Context)->GetTimeManager()->GetLastDeltaTime();
                    float speed = (position - m_LastPos).Magnitude() / delta * 6;
                    float newfov = (0.75f + speed) * rc->GetWidth() / rc->GetHeight();
                    newfov = (std::min)(newfov, PI / 2);
                    float curfov = m_InGameCam->GetFov();
                    m_InGameCam->SetFov((newfov - curfov) * (std::min)(delta, 20.0f) / 1000 +
                                        curfov);
                }

                m_IsActive = true;
                m_LastPos = position;
            }
        } else if (!m_WasPaused) {
            m_IsActive = false;
        }

        m_WasPaused = ModSupport::GameState().Paused;
    }
}

void DynamicFov::OnModifyConfig(const BML::Mod::Context &, const BML::Mod::ConfigEvent &event) {
    const auto *category = event.Category.data();
    const auto *key = event.Key.data();
    auto found = m_Context.Config().FindProperty(event.Category, event.Key);
    if (!found)
        return;
    auto prop = found.Value();

    if ((event.Category == "Misc" && event.Key == "Enable") && !m_Enabled.Get<bool>()) {
        CKRenderContext *rc = ModSupport::Render(m_Context);
        m_InGameCam->SetFov(0.75f * rc->GetWidth() / rc->GetHeight());
        m_IsActive = false;
    }
}