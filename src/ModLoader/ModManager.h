/**
 * @file ModManager.h
 * @brief ModLoader CK Manager
 * 
 * ModManager is a CKBaseManager that integrates with the Virtools engine
 * lifecycle. It handles:
 * - Engine context initialization and shutdown
 * - Module loading coordination
 * - IMC event publishing for game lifecycle events
 * - Frame processing coordination
 * 
 * This replaces the ModManager from BMLPlus.dll, focusing purely on
 * engine integration without legacy mod management concerns.
 */

#ifndef BML_MOD_MANAGER_H
#define BML_MOD_MANAGER_H

#include "CKBaseManager.h"
#include "CKContext.h"

// ModLoader Manager GUID
#ifndef MOD_MANAGER_GUID
#define MOD_MANAGER_GUID CKGUID(0x32a40332, 0x3bf12a51)
#endif

/**
 * @brief CK Manager for ModLoader engine integration
 * 
 * Responsibilities:
 * - Publish engine lifecycle events via IMC
 * - Coordinate module loading after CKContext is available
 * - Manage frame processing hooks
 */
class ModManager : public CKBaseManager {
public:
    explicit ModManager(CKContext *context);
    ~ModManager() override;

    // CKBaseManager lifecycle overrides
    CKERROR OnCKInit() override;
    CKERROR OnCKEnd() override;
    CKERROR OnCKPlay() override;
    CKERROR OnCKPause() override;
    CKERROR OnCKReset() override;
    CKERROR OnCKPostReset() override;
    CKERROR PreProcess() override;
    CKERROR PostProcess() override;
    CKERROR OnPreRender(CKRenderContext *dev) override;
    CKERROR OnPostRender(CKRenderContext *dev) override;
    CKERROR OnPostSpriteRender(CKRenderContext *dev) override;

    CKDWORD GetValidFunctionsMask() override {
        return CKMANAGER_FUNC_OnCKInit |
            CKMANAGER_FUNC_OnCKEnd |
            CKMANAGER_FUNC_OnCKPlay |
            CKMANAGER_FUNC_OnCKPause |
            CKMANAGER_FUNC_OnCKReset |
            CKMANAGER_FUNC_OnCKPostReset |
            CKMANAGER_FUNC_PreProcess |
            CKMANAGER_FUNC_PostProcess |
            CKMANAGER_FUNC_OnPreRender |
            CKMANAGER_FUNC_OnPostRender |
            CKMANAGER_FUNC_OnPostSpriteRender;
    }

    int GetFunctionPriority(CKMANAGER_FUNCTIONS Function) override {
        // ModLoader runs before other managers to ensure hooks are ready
        if (Function == CKMANAGER_FUNC_PreProcess)
            return -10000; // Very low priority (runs early)
        else if (Function == CKMANAGER_FUNC_PostProcess)
            return 10000; // Very high priority (runs late)
        else
            return 0;
    }

    /**
     * @brief Get the ModManager instance from a CKContext
     */
    static ModManager *GetManager(CKContext *context) {
        return (ModManager *) context->GetManagerByGuid(MOD_MANAGER_GUID);
    }

    /**
     * @brief Check if engine is in play state (game running)
     */
    bool IsEngineReady() const { return m_EngineReady; }

    /**
     * @brief Get the render context
     */
    CKRenderContext *GetRenderContext() const { return m_RenderContext; }

private:
    // Initialize IMC topic IDs
    void InitializeIMCTopics();

    // Register Virtools objects to BML context
    void RegisterVirtoolsObjects();

    // Publish lifecycle events
    void PublishLifecycleEvent(const char *topic);
    void PublishProcessEvent(float deltaTime);

    CKRenderContext *m_RenderContext = nullptr;
    float m_LastTime = 0.0f;
    bool m_EngineReady = false;
};

#endif // BML_MOD_MANAGER_H
