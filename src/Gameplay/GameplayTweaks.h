#ifndef BML_GAMEPLAYTWEAKS_H
#define BML_GAMEPLAYTWEAKS_H

#include <array>

#include "BML/Behavior.hpp"

class CKBehavior;
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
    bool ApplyLanternScript(bool enabled, bool warnIfUnavailable);
    void PatchLanternAlphaTest(const BML::Behavior::Graph &script);

    void DiscoverOverclockPatch(const BML::Behavior::Graph &script);
    void CompleteOverclockPatch(const BML::Behavior::Graph &script);
    bool ApplyOverclock(bool enabled, bool warnIfUnavailable);
    bool ReplaceOverclockPlan(bool warnIfUnavailable);
    void RejectOverclockPatch(const char *reason);

    IBML *m_BML = nullptr;
    ILogger *m_Logger = nullptr;

    IProperty *m_LanternAlphaTest = nullptr;
    IProperty *m_FixLifeBall = nullptr;
    IProperty *m_Overclock = nullptr;

    BML::Behavior::Session m_Behavior;
    BML::Behavior::Plan m_LanternPlan;
    BML::Behavior::Plan m_OverclockPlan;
    BML::Behavior::Plan m_ExtraLifePlan;
    std::array<BML::Behavior::Edit, 2> m_LanternEdits;
    std::array<BML::Behavior::Edit, 2> m_OverclockEdits;
    bool m_LanternReady = false;
    bool m_OverclockIngameReady = false;
    bool m_OverclockEnergyReady = false;
};

#endif // BML_GAMEPLAYTWEAKS_H
