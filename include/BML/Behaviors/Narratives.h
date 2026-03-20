// Auto-generated from Narratives.dll -- do not edit manually.
#ifndef BML_BEHAVIORS_NARRATIVES_H
#define BML_BEHAVIORS_NARRATIVES_H

#include "BML/Behavior.h"
#include "CKAll.h"

namespace bml::Narratives {

// ============================================================================
// ObjectLoad
//
// Loads objects from a NMO file
// ============================================================================

class ObjectLoad {
public:
    static constexpr CKGUID Guid = CKGUID(0x30DE0800, 0x5381C0FE);

    struct Result {
        int Objects = 0;  // Unknown type: CKPGUID_OBJECTARRAY
        CKObject* MasterObject = nullptr;
    };

    ObjectLoad &File(const char* v) { m_File = v; return *this; }
    ObjectLoad &MasterObjectName(const char* v) { m_MasterObjectName = v; return *this; }
    ObjectLoad &ClassID(CK_CLASSID v) { m_ClassID = v; return *this; }
    ObjectLoad &AddToScene(CKBOOL v) { m_AddToScene = v; return *this; }
    ObjectLoad &ReuseMesh(CKBOOL v) { m_ReuseMesh = v; return *this; }
    ObjectLoad &ReuseMaterial(CKBOOL v) { m_ReuseMaterial = v; return *this; }

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

    Result RunWithResult(BehaviorCache &cache, int slot = 0) const {
        auto runner = cache.Get(Guid, false);
        Apply(runner);
        runner.Execute(slot);
        Result r;
        r.Objects = runner.ReadOutput<int>(0);
        r.MasterObject = static_cast<CKObject*>(runner.ReadOutputObj(1));
        return r;
    }

private:
    template <typename Builder>
    void Apply(Builder &bb) const {
        bb.Input(0, m_File)
          .Input(1, m_MasterObjectName)
          .Input(2, m_ClassID)
          .Input(3, m_AddToScene)
          .Input(4, m_ReuseMesh)
          .Input(5, m_ReuseMaterial);
    }

    const char* m_File = "";
    const char* m_MasterObjectName = "";
    CK_CLASSID m_ClassID = CKCID_3DOBJECT;
    CKBOOL m_AddToScene = FALSE;
    CKBOOL m_ReuseMesh = FALSE;
    CKBOOL m_ReuseMaterial = FALSE;
};

// ============================================================================
// SendMessage
//
// Sends a message to a specific object
// ============================================================================

class SendMessage {
public:
    static constexpr CKGUID Guid = CKGUID(0x73657501, 0x5381C0FE);

    SendMessage &Message(int v) { m_Message = v; return *this; }
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

    int m_Message = 0;
    CKBeObject* m_Destination = nullptr;
};

} // namespace bml::Narratives

#endif // BML_BEHAVIORS_NARRATIVES_H
