#ifndef BML_GAMEEVENTHOOKS_H
#define BML_GAMEEVENTHOOKS_H

#include <array>

#include "BML/Behavior.hpp"

class CKBehavior;
class IBML;
class ILogger;
class IMessageReceiver;

class GameEventHooks {
public:
    void OnLoad(IBML &bml, ILogger &logger);
    void OnUnload();
    void OnLoadScript(CKBehavior *script);

private:
    void PatchBaseEventHandler(const BML::Behavior::Graph &script);
    void PatchGameplayIngame(const BML::Behavior::Graph &script);
    void PatchGameplayEnergy(const BML::Behavior::Graph &script);
    void PatchGameplayEvents(const BML::Behavior::Graph &script);
    bool ReplacePlan(const char *scriptName);
    void RejectPatch(const char *scriptName, const char *reason) const;

    BML::Behavior::Session m_Behavior;
    BML::Behavior::Plan m_Plan;
    std::array<BML::Behavior::Edit, 4> m_Edits;
    IBML *m_BML = nullptr;
    IMessageReceiver *m_Receiver = nullptr;
    ILogger *m_Logger = nullptr;
};

#endif // BML_GAMEEVENTHOOKS_H
