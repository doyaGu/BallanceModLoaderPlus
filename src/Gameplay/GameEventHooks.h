#ifndef BML_GAMEEVENTHOOKS_H
#define BML_GAMEEVENTHOOKS_H

#include <cstdint>
#include <vector>

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

    // Behavior Patch ids, kept as the plain handle type so this header stays
    // free of the Loader internals. Each one holds the hooks of one graph and
    // is closed when this Mod stops receiving events.
    std::vector<std::uintptr_t> m_Installed;
    IBML *m_BML = nullptr;
    IMessageReceiver *m_Receiver = nullptr;
    ILogger *m_Logger = nullptr;
};

#endif // BML_GAMEEVENTHOOKS_H
