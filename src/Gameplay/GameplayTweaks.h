#ifndef BML_GAMEPLAYTWEAKS_H
#define BML_GAMEPLAYTWEAKS_H

#include "BML/Behavior.hpp"

class ModContext;
class Config;
class Logger;
class Property;

class GameplayTweaks {
public:
    void InitConfig(Config &config);
    bool OnModifyConfig(const char *category, const char *key, Property *property);

    void OnLoad(ModContext &bml, Logger &logger);
    void OnUnload();
    void OnExitGame();

private:
    void ApplyLanternAlphaTest(bool enabled);
    bool ApplyOverclock(bool enabled, bool warnIfUnavailable);
    bool SetPlanEnabled(BML::Behavior::Plan &plan, bool enabled,
                        const char *name, bool warnIfUnavailable);
    void ClosePlan(BML::Behavior::Plan &plan, const char *name);

    ModContext *m_BML = nullptr;
    Logger *m_Logger = nullptr;

    Property *m_LanternAlphaTest = nullptr;
    Property *m_FixLifeBall = nullptr;
    Property *m_Overclock = nullptr;

    BML::Behavior::Session m_Behavior;
    BML::Behavior::Submission m_Lantern;
    BML::Behavior::Edit::Port m_LanternInput;
    BML::Behavior::Plan m_OverclockPlan;
    BML::Behavior::Plan m_ExtraLifePlan;
};

#endif // BML_GAMEPLAYTWEAKS_H
