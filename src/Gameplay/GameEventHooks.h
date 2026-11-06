#ifndef BML_GAMEEVENTHOOKS_H
#define BML_GAMEEVENTHOOKS_H

#include "BML/Behavior.hpp"

class ModContext;
class Logger;
class ModContext;

class GameEventHooks {
public:
    void OnLoad(ModContext &bml, Logger &logger);
    void OnUnload();

private:
    void RejectPlan(const char *reason) const;

    BML::Behavior::Session m_Behavior;
    BML::Behavior::Plan m_Plan;
    ModContext *m_Receiver = nullptr;
    Logger *m_Logger = nullptr;
};

#endif // BML_GAMEEVENTHOOKS_H
