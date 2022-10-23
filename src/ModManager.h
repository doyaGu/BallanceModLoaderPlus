#ifndef BML_MODMANAGER_H
#define BML_MODMANAGER_H

#include "CKContext.h"
#include "CKBaseManager.h"

#ifndef BML_MODMANAGER_GUID
#define BML_MODMANAGER_GUID CKGUID(0x32a40332, 0x3bf12a51)
#endif

class ModLoader;

class ModManager : public CKBaseManager {
public:
    explicit ModManager(CKContext *ctx);
    ~ModManager() override;

    CKERROR OnCKInit() override;
    CKERROR OnCKEnd() override;
    
    CKERROR OnCKReset() override;
    CKERROR OnCKPostReset() override;

    CKERROR PreProcess() override;
    CKERROR PostProcess() override;

    CKERROR OnPreRender(CKRenderContext *dev) override;
    CKERROR OnPostRender(CKRenderContext *dev) override;

    int GetFunctionPriority(CKMANAGER_FUNCTIONS Function) override { return -1000; }

    CKDWORD GetValidFunctionsMask() override {
        return CKMANAGER_FUNC_PreProcess |
               CKMANAGER_FUNC_PostProcess |
               CKMANAGER_FUNC_OnCKInit |
               CKMANAGER_FUNC_OnCKEnd |
               CKMANAGER_FUNC_OnCKReset |
               CKMANAGER_FUNC_OnCKPostReset |
               CKMANAGER_FUNC_OnPreRender |
               CKMANAGER_FUNC_OnPostRender;
    }

    static ModManager *GetManager(CKContext *context) {
        return (ModManager *)context->GetManagerByGuid(BML_MODMANAGER_GUID);
    }

protected:
    ModLoader *m_Loader;
};


#endif //BML_MODMANAGER_H
