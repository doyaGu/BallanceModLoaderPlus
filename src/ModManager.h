#ifndef BML_MODMANAGER_H
#define BML_MODMANAGER_H

#include "CKBaseManager.h"
#include "CKContext.h"

#ifndef MOD_MANAGER_GUID
#define MOD_MANAGER_GUID CKGUID(0x32a40332, 0x3bf12a51)
#endif

class ModManager : public CKBaseManager {
public:
    explicit ModManager(CKContext *context);
    ~ModManager() override;

    CKERROR OnCKInit() override;
    CKERROR OnCKEnd() override;

    CKERROR OnCKReset() override;
    CKERROR OnCKPostReset() override;

    CKERROR PreProcess() override;
    CKERROR PostProcess() override;

    CKERROR OnPostRender(CKRenderContext *dev) override;
    CKERROR OnPostSpriteRender(CKRenderContext *dev) override;

    CKDWORD GetValidFunctionsMask() override {
        return CKMANAGER_FUNC_OnCKInit |
               CKMANAGER_FUNC_OnCKEnd |
               CKMANAGER_FUNC_OnCKReset |
               CKMANAGER_FUNC_OnCKPostReset |
               CKMANAGER_FUNC_PreProcess |
               CKMANAGER_FUNC_PostProcess |
               CKMANAGER_FUNC_OnPostRender |
               CKMANAGER_FUNC_OnPostSpriteRender;
    }

    int GetFunctionPriority(CKMANAGER_FUNCTIONS Function) override {
        if (Function == CKMANAGER_FUNC_PreProcess)
            return -10000; // Low Priority
        else if (Function == CKMANAGER_FUNC_PostProcess)
            return 10000; // High Priority
        else
            return 0;
    }

    static ModManager *GetManager(CKContext *context) {
        return (ModManager *)context->GetManagerByGuid(MOD_MANAGER_GUID);
    }
};

#endif // BML_MODMANAGER_H
