#ifndef BML2_MODMANAGER_H
#define BML2_MODMANAGER_H

#include <vector>
#include <unordered_map>
#include <type_traits>

#include "CKContext.h"
#include "CKBaseManager.h"

#include "Defines.h"
#include "ILogger.h"

class IBML;
class IMod;
struct BModDll;
class ModLoader;

template<typename T>
void *func_addr(T func) {
    return *reinterpret_cast<void **>(&func);
}

class ModManager : public CKBaseManager {
    friend class ModLoader;
public:
    explicit ModManager(CKContext *ctx);
    ~ModManager() override;

    CKERROR OnCKInit() override;
    CKERROR OnCKEnd() override;
    
    CKERROR OnCKReset() override;
    CKERROR OnCKPostReset() override;

    CKERROR PostProcess() override;

    CKERROR OnPreRender(CKRenderContext *dev) override;
    CKERROR OnPostRender(CKRenderContext *dev) override;

    CKDWORD GetValidFunctionsMask() override {
        return CKMANAGER_FUNC_PostProcess |
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

    static const char *Name;

protected:
    bool m_Initialized = false;

    ILogger *m_Logger = nullptr;
    ModLoader *m_Loader = nullptr;
};


#endif //BML2_MODMANAGER_H
