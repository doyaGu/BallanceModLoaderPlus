#include "ModManager.h"

#include "ModLoader.h"

ModManager::ModManager(CKContext *context)  : CKBaseManager(context, MOD_MANAGER_GUID, "Mod Manager") {
    context->RegisterNewManager(this);
}

ModManager::~ModManager() = default;

CKERROR ModManager::OnCKInit() {
    ModLoader::GetInstance().Init(m_Context);
    return CK_OK;
}

CKERROR ModManager::OnCKEnd() {
    ModLoader::GetInstance().Shutdown();
    return CK_OK;
}

CKERROR ModManager::OnCKReset() {
    ModLoader::GetInstance().OnCKReset();
    return CK_OK;
}

CKERROR ModManager::OnCKPostReset() {
    ModLoader::GetInstance().OnCKPostReset();
    return CK_OK;
}

CKERROR ModManager::PreProcess() {
    return CK_OK;
}

CKERROR ModManager::PostProcess() {
    ModLoader::GetInstance().PostProcess();
    return CK_OK;
}

CKERROR ModManager::OnPostRender(CKRenderContext *dev) {
    ModLoader::GetInstance().OnPostRender(dev);
    return CK_OK;
}

CKERROR ModManager::OnPostSpriteRender(CKRenderContext *dev) {
    ModLoader::GetInstance().OnPostSpriteRender(dev);
    return CK_OK;
}