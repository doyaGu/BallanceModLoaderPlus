#ifndef BML_GAMEEVENTHOOKS_H
#define BML_GAMEEVENTHOOKS_H

#include "BML/Behavior.hpp"

class IBML;
class ILogger;
class IMessageReceiver;

class GameEventHooks {
public:
    void OnLoad(IBML &bml, ILogger &logger);
    void OnUnload();

private:
    void RejectPlan(const char *reason) const;

    BML::Behavior::Session m_Behavior;
    BML::Behavior::Plan m_Plan;
    IMessageReceiver *m_Receiver = nullptr;
    ILogger *m_Logger = nullptr;
};

#endif // BML_GAMEEVENTHOOKS_H
