// Auto-generated from Logics.dll -- do not edit manually.
#ifndef BML_BEHAVIORS_LOGICS_H
#define BML_BEHAVIORS_LOGICS_H

#include "BML/Behavior.h"
#include "CKAll.h"

namespace bml::Logics {

// ============================================================================
// SendMessage
//
// Sends a message to objects
// ============================================================================

class SendMessage {
public:
    static constexpr CKGUID Guid = CKGUID(0x73657502, 0x5381C0FE);

    SendMessage &Message(const char* v) { m_Message = v; return *this; }
    SendMessage &Destination(CKBeObject* v) { m_Destination = v; return *this; }

    void Run(BehaviorCache &cache, int slot = 0) const {
        auto runner = cache.Get(Guid, false);
        Run(runner, slot);
    }

    void Run(BehaviorRunner &runner, int slot = 0) const {
        Apply(runner);
        runner.Execute(slot);
    }

    CKBehavior *Build(CKBehavior *script) const {
        BehaviorFactory factory(script, Guid, false);
        Apply(factory);
        return factory.Build();
    }

private:
    template <typename Builder>
    void Apply(Builder &bb) const {
        bb.Input(0, m_Message)
          .Input(1, m_Destination);
    }

    const char* m_Message = "";
    CKBeObject* m_Destination = nullptr;
};

} // namespace bml::Logics

#endif // BML_BEHAVIORS_LOGICS_H
