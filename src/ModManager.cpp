#include "ModManager.h"
#include "ModLoader.h"

ModManager::ModManager(CKContext *ctx) : CKBaseManager(ctx, BML_MODMANAGER_GUID, "Ballance Mod Manager") {
    ctx->RegisterNewManager(this);
    m_Loader = &ModLoader::GetInstance();
}

ModManager::~ModManager() = default;

CKERROR ModManager::OnCKInit() {
    m_Loader->Init(m_Context);
    if (!m_Loader->IsInitialized())
        return CKERR_NOTINITIALIZED;
    return CK_OK;
}

CKERROR ModManager::OnCKEnd() {
    if (m_Loader->IsInitialized()) {
        m_Loader->UnloadMods();
        m_Loader->Release();
    }
    return CK_OK;
}

CKERROR ModManager::OnCKReset() {
    m_Loader->SetReset();
    return CK_OK;
}

CKERROR ModManager::OnCKPostReset() {
    if (m_Loader->GetModCount() == 0)
        m_Loader->LoadMods();
    return CK_OK;
}

CKERROR ModManager::PreProcess() {
    return m_Loader->OnPreProcess();
}

CKERROR ModManager::PostProcess() {
    return m_Loader->OnProcess();
}

CKERROR ModManager::OnPreRender(CKRenderContext *dev) {
    return m_Loader->OnPreRender(dev);
}

CKERROR ModManager::OnPostRender(CKRenderContext *dev) {
    return m_Loader->OnPostRender(dev);
}
