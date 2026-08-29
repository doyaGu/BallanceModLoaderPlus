#ifndef BML_GAMEEVENTHOOKS_H
#define BML_GAMEEVENTHOOKS_H

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
    void PatchBaseEventHandler(CKBehavior *script);
    void PatchGameplayIngame(CKBehavior *script);
    void PatchGameplayEnergy(CKBehavior *script);
    void PatchGameplayEvents(CKBehavior *script);
    void RejectPatch(const char *scriptName, const char *reason) const;

    IBML *m_BML = nullptr;
    IMessageReceiver *m_Receiver = nullptr;
    ILogger *m_Logger = nullptr;
};

#endif // BML_GAMEEVENTHOOKS_H
