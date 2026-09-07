#ifndef BML_GAMEPLAYTWEAKS_H
#define BML_GAMEPLAYTWEAKS_H

#include "BML/Behavior.hpp"

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
    void OnExitGame();

private:
    void ApplyLanternAlphaTest(bool enabled);
    bool ApplyLanternScript(bool enabled, bool warnIfUnavailable);
    bool ApplyOverclock(bool enabled, bool warnIfUnavailable);

    IBML *m_BML = nullptr;
    ILogger *m_Logger = nullptr;

    IProperty *m_LanternAlphaTest = nullptr;
    IProperty *m_FixLifeBall = nullptr;
    IProperty *m_Overclock = nullptr;

    BML::Behavior::Session m_Behavior;
    BML::Behavior::Plan m_LanternPlan;
    BML::Behavior::Plan m_OverclockPlan;
    BML::Behavior::Plan m_ExtraLifePlan;
};

#endif // BML_GAMEPLAYTWEAKS_H
