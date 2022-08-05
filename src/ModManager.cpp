#include "ModManager.h"

#include "InputHook.h"
#include "Logger.h"
#include "ModLoader.h"
#include "BMLMod.h"
#include "NewBallTypeMod.h"

extern void AddDataPath(const char *path);
extern bool HookObjectLoad();
extern bool HookPhysicalize();

const char *ModManager::Name = "Ballance Mod Manager";

ModManager::ModManager(CKContext *ctx) : CKBaseManager(ctx, BML_MODMANAGER_GUID, (char *) Name) {
    ctx->RegisterNewManager(this);
    m_Logger = new Logger("ModManager");
    m_Loader = &ModLoader::GetInstance();
}

ModManager::~ModManager() {
    delete m_Logger;
}

CKERROR ModManager::OnCKInit() {
    AddDataPath("..\\ModLoader");

    if (HookObjectLoad())
        m_Logger->Info("Hook ObjectLoad Success");
    else
        m_Logger->Info("Hook ObjectLoad Failed");

    if (HookPhysicalize())
        m_Logger->Info("Hook Physicalize Success");
    else
        m_Logger->Info("Hook Physicalize Failed");

    return CK_OK;
}

CKERROR ModManager::OnCKEnd() {
    m_Initialized = false;

    m_Loader->BroadcastMessage("OnUnload", &IMod::OnUnload);

    for (Config *config: m_Loader->m_Configs)
        config->Save();
    m_Loader->m_Configs.clear();
    return CK_OK;
}

CKERROR ModManager::OnCKReset() {
    m_Loader->m_IsReset = true;
    return CK_OK;
}

CKERROR ModManager::OnCKPostReset() {
    if (!m_Initialized) {
        m_Loader->m_RenderContext = m_Context->GetPlayerRenderContext();
        m_Logger->Info("Get Render Context pointer 0x%08x", m_Loader->m_RenderContext);

        m_Loader->m_RenderManager = m_Context->GetRenderManager();
        m_Logger->Info("Get Render Manager pointer 0x%08x", m_Loader->m_RenderManager);

        ExecuteBB::Init(m_Context);

        auto &mods = m_Loader->m_Mods;
        m_Loader->m_BMLMod = new BMLMod(m_Loader);
        mods.push_back(m_Loader->m_BMLMod);

        m_Loader->m_BallTypeMod = new NewBallTypeMod(m_Loader);
        mods.push_back(m_Loader->m_BallTypeMod);

        for (auto &modDll: m_Loader->m_ModDlls) {
            if (m_Loader->RegisterMod(modDll) == CK_OK && !modDll.dllPath.empty())
                AddDataPath(modDll.dllPath.c_str());
        }

        for (auto *mod: mods) {
            m_Loader->LoadMod(mod);
        }

        std::sort(m_Loader->m_Commands.begin(), m_Loader->m_Commands.end(),
                  [](ICommand *A, ICommand *B) { return A->GetName() < B->GetName(); });

        for (Config *config: m_Loader->m_Configs)
            config->Save();

        m_Loader->BroadcastCallback(&IMod::OnLoadObject, "base.cmo", false,  "", CKCID_3DOBJECT, true, true, true, false, nullptr, nullptr);

        int scriptCnt = m_Context->GetObjectsCountByClassID(CKCID_BEHAVIOR);
        CK_ID *scripts = m_Context->GetObjectsListByClassID(CKCID_BEHAVIOR);
        for (int i = 0; i < scriptCnt; i++) {
            auto *behavior = (CKBehavior *) m_Context->GetObject(scripts[i]);
            if (behavior->GetType() == CKBEHAVIORTYPE_SCRIPT) {
                m_Loader->BroadcastCallback(&IMod::OnLoadScript, "base.cmo", behavior);
            }
        }

        m_Initialized = true;
    }

    m_Loader->m_IsReset = false;
    return CK_OK;
}

CKERROR ModManager::PostProcess() {
    m_Loader->m_SkipRender = false;
    for (auto iter = m_Loader->m_Timers.begin(); iter != m_Loader->m_Timers.end();) {
        if (!iter->Process(m_Loader->GetTimeManager()->GetMainTickCount(),
                           m_Loader->GetTimeManager()->GetAbsoluteTime()))
            iter = m_Loader->m_Timers.erase(iter);
        else
            iter++;
    }

    m_Loader->BroadcastCallback(&IMod::OnProcess);
    if (m_Loader->GetInputManager()->IsKeyDown(CKKEY_F) && m_Loader->IsCheatEnabled())
        m_Loader->SkipRenderForNextTick();

    m_Loader->GetInputManager()->Process();

    if (m_Loader->m_Exiting)
        m_Loader->SendMessageBroadcast("Exit Game");
    return CK_OK;
}

CKERROR ModManager::OnPreRender(CKRenderContext *dev) {
    if (m_Loader->IsSkipRender()) {
        dev->ChangeCurrentRenderOptions(0, CK_RENDER_DEFAULTSETTINGS);
    } else {
        dev->ChangeCurrentRenderOptions(CK_RENDER_DEFAULTSETTINGS, 0);
    }
    return CK_OK;
}

CKERROR ModManager::OnPostRender(CKRenderContext *dev) {
    auto flags = static_cast<CK_RENDER_FLAGS>(dev->GetCurrentRenderOptions());
    m_Loader->BroadcastCallback(&IMod::OnRender, flags);
    return CK_OK;
}