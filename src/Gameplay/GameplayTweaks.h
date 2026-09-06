#ifndef BML_GAMEPLAYTWEAKS_H
#define BML_GAMEPLAYTWEAKS_H

#include <vector>

#include "BML/Behavior.hpp"

class CKBehavior;
class CKBehaviorIO;
class CKBehaviorLink;
class IBML;
class IConfig;
class ILogger;
class IProperty;

class GameplayTweaks {
public:
    void InitConfig(IConfig &config);
    bool OnModifyConfig(const char *category, const char *key, IProperty *property);

    void OnLoad(IBML &bml, ILogger &logger);
    void OnUnload();
    void OnLoadScript(CKBehavior *script);
    void OnExitGame();

private:
    void ApplyLanternAlphaTest(bool enabled);
    void PatchLanternAlphaTest(CKBehavior *script);
    void PatchExtraLife(CKBehavior *script);

    void DiscoverOverclockPatch(CKBehavior *script);
    void CompleteOverclockPatch(CKBehavior *script);
    bool ApplyOverclock(bool enabled, bool warnIfUnavailable);
    void ClearOverclockPatch();
    void RejectOverclockPatch(const char *reason);

    IBML *m_BML = nullptr;
    ILogger *m_Logger = nullptr;

    IProperty *m_LanternAlphaTest = nullptr;
    IProperty *m_FixLifeBall = nullptr;
    IProperty *m_Overclock = nullptr;

    CKBehaviorLink *m_OverclockLinks[3] = {};
    CKBehaviorIO *m_OverclockLinkIO[3][2] = {};

    BML::Behavior::Session m_Behavior;
    std::vector<BML::Behavior::Patch> m_ExtraLifePatches;
};

#endif // BML_GAMEPLAYTWEAKS_H
