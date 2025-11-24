#ifndef BML_EVENTHOOK_H
#define BML_EVENTHOOK_H

class BMLMod;
class CKBehavior;
class IBML;
class IMessageReceiver;

class EventHookRegistrar {
public:
    EventHookRegistrar(BMLMod &mod, IBML &bml);

    void RegisterBaseEventHandler(CKBehavior *script);
    void RegisterGameplayIngame(CKBehavior *script);
    void RegisterGameplayEnergy(CKBehavior *script);
    void RegisterGameplayEvents(CKBehavior *script);

private:
    BMLMod &m_Mod;
    IBML &m_BML;
    IMessageReceiver &m_Receiver;
};

#endif // BML_EVENTHOOK_H
