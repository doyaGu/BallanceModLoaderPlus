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
    return ModLoader::GetInstance().OnCKReset();
}

CKERROR ModManager::OnCKPostReset() {
    return ModLoader::GetInstance().OnCKPostReset();
}

CKERROR ModManager::PreProcess() {
    return CK_OK;
}

CKERROR ModManager::PostProcess() {
    return ModLoader::GetInstance().PostProcess();
}

CKERROR ModManager::OnPreRender(CKRenderContext *dev) {
    return CK_OK;
}

CKERROR ModManager::OnPostRender(CKRenderContext *dev) {
    return ModLoader::GetInstance().OnPostRender(dev);
}

CKERROR ModManager::OnPostSpriteRender(CKRenderContext *dev) {
    return ModLoader::GetInstance().OnPostSpriteRender(dev);
}