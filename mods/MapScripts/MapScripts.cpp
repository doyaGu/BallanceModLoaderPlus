#include "MapScripts.h"

BML_DEFINE_MOD(MapScripts, {
    .Id = "MapScripts",
    .Name = "Map Scripts",
    .Version = BML_VERSION,
    .Author = "Gamepiaynmo",
    .Description = "Activate callback scripts in maps.",
    .MinimumLoaderVersion = BML_VERSION,
})

void MapScripts::OnLoadObject(const BML::Mod::Context &, const BML::Mod::LoadObjectEvent &event) {
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

#define FILL_MAP(FUNC) m_MapScripts.insert_or_assign(#FUNC, \
    ModSupport::Object(m_Context, ModSupport::Find<CKBehavior>(m_Context, "BML_" #FUNC)))

    if (isMap) {
        FILL_MAP(OnPostLoadLevel);
        FILL_MAP(OnStartLevel);
        FILL_MAP(OnPreResetLevel);
        FILL_MAP(OnPostResetLevel);
        FILL_MAP(OnPauseLevel);
        FILL_MAP(OnUnpauseLevel);
        FILL_MAP(OnPreExitLevel);
        FILL_MAP(OnPreNextLevel);
        FILL_MAP(OnDead);
        FILL_MAP(OnPreEndLevel);
        FILL_MAP(OnPostEndLevel);
        FILL_MAP(OnCounterActive);
        FILL_MAP(OnCounterInactive);
        FILL_MAP(OnBallNavActive);
        FILL_MAP(OnBallNavInactive);
        FILL_MAP(OnCamNavActive);
        FILL_MAP(OnCamNavInactive);
        FILL_MAP(OnBallOff);
        FILL_MAP(OnPreCheckpointReached);
        FILL_MAP(OnPostCheckpointReached);
        FILL_MAP(OnLevelFinish);
        FILL_MAP(OnGameOver);
        FILL_MAP(OnExtraPoint);
        FILL_MAP(OnPreSubLife);
        FILL_MAP(OnPostSubLife);
        FILL_MAP(OnPreLifeUp);
        FILL_MAP(OnPostLifeUp);
    }
#undef FILL_MAP
}

#define ACT_SCRIPT(FUNC)                                                                           \
    {                                                                                              \
        const auto found = m_MapScripts.find(#FUNC); \
        CKBehavior *script = found == m_MapScripts.end() ? nullptr : found->second.Get();                                                  \
        if (script != nullptr) {                                                                   \
            CKBeObject *obj = script->GetOwner();                                                  \
            CKScene *scene = ModSupport::Engine(m_Context)->GetCurrentScene();                     \
            scene->Activate(obj, false);                                                           \
            scene->Activate(script, true);                                                         \
        }                                                                                          \
    }

void MapScripts::OnPostLoadLevel(const BML::Mod::Context &) { ACT_SCRIPT(OnPostLoadLevel); }

void MapScripts::OnStartLevel(const BML::Mod::Context &) { ACT_SCRIPT(OnStartLevel); }

void MapScripts::OnPreResetLevel(const BML::Mod::Context &) { ACT_SCRIPT(OnPreResetLevel); }

void MapScripts::OnPostResetLevel(const BML::Mod::Context &) { ACT_SCRIPT(OnPostResetLevel); }

void MapScripts::OnPauseLevel(const BML::Mod::Context &) { ACT_SCRIPT(OnPauseLevel); }

void MapScripts::OnUnpauseLevel(const BML::Mod::Context &) { ACT_SCRIPT(OnUnpauseLevel); }

void MapScripts::OnPreExitLevel(const BML::Mod::Context &) { ACT_SCRIPT(OnPreExitLevel); }

void MapScripts::OnPreNextLevel(const BML::Mod::Context &) { ACT_SCRIPT(OnPreNextLevel); }

void MapScripts::OnDead(const BML::Mod::Context &) { ACT_SCRIPT(OnDead); }

void MapScripts::OnPreEndLevel(const BML::Mod::Context &) { ACT_SCRIPT(OnPreEndLevel); }

void MapScripts::OnPostEndLevel(const BML::Mod::Context &) { ACT_SCRIPT(OnPostEndLevel); }

void MapScripts::OnCounterActive(const BML::Mod::Context &) { ACT_SCRIPT(OnCounterActive); }

void MapScripts::OnCounterInactive(const BML::Mod::Context &) { ACT_SCRIPT(OnCounterInactive); }

void MapScripts::OnBallNavActive(const BML::Mod::Context &) { ACT_SCRIPT(OnBallNavActive); }

void MapScripts::OnBallNavInactive(const BML::Mod::Context &) { ACT_SCRIPT(OnBallNavInactive); }

void MapScripts::OnCamNavActive(const BML::Mod::Context &) { ACT_SCRIPT(OnCamNavActive); }

void MapScripts::OnCamNavInactive(const BML::Mod::Context &) { ACT_SCRIPT(OnCamNavInactive); }

void MapScripts::OnBallOff(const BML::Mod::Context &) { ACT_SCRIPT(OnBallOff); }

void MapScripts::OnPreCheckpointReached(const BML::Mod::Context &) {
    ACT_SCRIPT(OnPreCheckpointReached);
}

void MapScripts::OnPostCheckpointReached(const BML::Mod::Context &) {
    ACT_SCRIPT(OnPostCheckpointReached);
}

void MapScripts::OnLevelFinish(const BML::Mod::Context &) { ACT_SCRIPT(OnLevelFinish); }

void MapScripts::OnGameOver(const BML::Mod::Context &) { ACT_SCRIPT(OnGameOver); }

void MapScripts::OnExtraPoint(const BML::Mod::Context &) { ACT_SCRIPT(OnExtraPoint); }

void MapScripts::OnPreSubLife(const BML::Mod::Context &) { ACT_SCRIPT(OnPreSubLife); }

void MapScripts::OnPostSubLife(const BML::Mod::Context &) { ACT_SCRIPT(OnPostSubLife); }

void MapScripts::OnPreLifeUp(const BML::Mod::Context &) { ACT_SCRIPT(OnPreLifeUp); }

void MapScripts::OnPostLifeUp(const BML::Mod::Context &) { ACT_SCRIPT(OnPostLifeUp); }

#undef ACT_SCRIPT