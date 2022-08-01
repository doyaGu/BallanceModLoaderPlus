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
    for (auto &mod: m_Mods) {
        if (mod) delete mod;
    }
    delete m_Logger;
}

int ModManager::GetModCount() {
    return m_Mods.size();
}

IMod *ModManager::GetMod(int modIdx) {
    return m_Mods[modIdx];
}

IMod *ModManager::GetModByID(const char *modId) {
    auto it = std::find_if(m_Mods.begin(), m_Mods.end(), [modId](IMod *mod) { return mod->GetID() == modId; });
    if (it != m_Mods.end())
        return *it;
    else
        return nullptr;
}

int ModManager::GetModIndex(IMod *mod) {
    int i;
    int c = m_Mods.size();
    for (i = 0; i < c; ++i)
        if (m_Mods[i] == mod)
            break;
    if (i == c)
        return -1;
    return i;
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

    BroadcastMessage("OnUnload", &IMod::OnUnload);

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

        m_Loader->m_BMLMod = new BMLMod(m_Loader);
        m_Mods.push_back(m_Loader->m_BMLMod);

        m_Loader->m_BallTypeMod = new NewBallTypeMod(m_Loader);
        m_Mods.push_back(m_Loader->m_BallTypeMod);

        for (auto &modDll: m_Loader->m_ModDlls) {
            if (RegisterMod(modDll, m_Loader) == CK_OK && !modDll.dllPath.empty())
                AddDataPath(modDll.dllPath.c_str());
        }

        for (auto *mod: m_Mods) {
            LoadMod(mod);
        }

        std::sort(m_Loader->m_Commands.begin(), m_Loader->m_Commands.end(),
                  [](ICommand *A, ICommand *B) { return A->GetName() < B->GetName(); });

        for (Config *config: m_Loader->m_Configs)
            config->Save();

        BroadcastCallback(&IMod::OnLoadObject, "base.cmo", false,  "", CKCID_3DOBJECT, true, true, true, false, nullptr, nullptr);

        int scriptCnt = m_Context->GetObjectsCountByClassID(CKCID_BEHAVIOR);
        CK_ID *scripts = m_Context->GetObjectsListByClassID(CKCID_BEHAVIOR);
        for (int i = 0; i < scriptCnt; i++) {
            auto *behavior = (CKBehavior *) m_Context->GetObject(scripts[i]);
            if (behavior->GetType() == CKBEHAVIORTYPE_SCRIPT) {
                BroadcastCallback(&IMod::OnLoadScript, "base.cmo", behavior);
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

    BroadcastCallback(&IMod::OnProcess);
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
    BroadcastCallback(&IMod::OnRender, flags);
    return CK_OK;
}

CKERROR ModManager::RegisterMod(BModDll &modDll, IBML *bml) {
    if (!modDll.entry)
        return CKERR_NOTFOUND;

    IMod *mod = modDll.entry(bml);
    BMLVersion curVer;
    BMLVersion reqVer = mod->GetBMLVersion();
    if (curVer < reqVer) {
        m_Logger->Warn("Mod %s[%s] requires BML %d.%d.%d", mod->GetID(), mod->GetName(), reqVer.major, reqVer.minor, reqVer.build);
        return CKERR_INVALIDPLUGIN;
    }
    m_Mods.push_back(mod);
    return CK_OK;
}

CKERROR ModManager::LoadMod(IMod *mod) {
    m_Logger->Info("Loading Mod %s[%s] v%s by %s", mod->GetID(), mod->GetName(), mod->GetVersion(), mod->GetAuthor());
    FillCallbackMap(mod);
    mod->OnLoad();
    return CK_OK;
}

void ModManager::FillCallbackMap(IMod *mod) {
    static class BlankMod : IMod {
    public:
        explicit BlankMod(IBML *bml) : IMod(bml) {}

        const char *GetID() override { return ""; }
        const char *GetVersion() override { return ""; }
        const char *GetName() override { return ""; }
        const char *GetAuthor() override { return ""; }
        const char *GetDescription() override { return ""; }
        DECLARE_BML_VERSION;
    } blank(m_Loader);

    void **vtable[2] = {
        *reinterpret_cast<void ***>(&blank),
        *reinterpret_cast<void ***>(mod)};

    int index = 0;
#define CHECK_V_FUNC(IDX, FUNC)                             \
    {                                                       \
        auto idx = IDX;                                     \
        if (vtable[0][idx] != vtable[1][idx])               \
            m_CallbackMap[func_addr(FUNC)].push_back(mod);  \
    }

    CHECK_V_FUNC(index++, &IMessageReceiver::OnPreStartMenu);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPostStartMenu);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnExitGame);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPreLoadLevel);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPostLoadLevel);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnStartLevel);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPreResetLevel);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPostResetLevel);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPauseLevel);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnUnpauseLevel);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPreExitLevel);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPostExitLevel);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPreNextLevel);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPostNextLevel);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnDead);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPreEndLevel);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPostEndLevel);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnCounterActive);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnCounterInactive);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnBallNavActive);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnBallNavInactive);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnCamNavActive);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnCamNavInactive);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnBallOff);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPreCheckpointReached);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPostCheckpointReached);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnLevelFinish);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnGameOver);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnExtraPoint);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPreSubLife);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPostSubLife);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPreLifeUp);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPostLifeUp);

    index += 7;

    CHECK_V_FUNC(index++, &IMod::OnLoad);
    CHECK_V_FUNC(index++, &IMod::OnUnload);
    CHECK_V_FUNC(index++, &IMod::OnModifyConfig);
    CHECK_V_FUNC(index++, &IMod::OnLoadObject);
    CHECK_V_FUNC(index++, &IMod::OnLoadScript);
    CHECK_V_FUNC(index++, &IMod::OnProcess);
    CHECK_V_FUNC(index++, &IMod::OnRender);
    CHECK_V_FUNC(index++, &IMod::OnCheatEnabled);

    CHECK_V_FUNC(index++, &IMod::OnPhysicalize);
    CHECK_V_FUNC(index++, &IMod::OnUnphysicalize);

#undef CHECK_V_FUNC
}