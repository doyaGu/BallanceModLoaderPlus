#ifndef BML_OBJECT_LOAD_BEHAVIORS_H
#define BML_OBJECT_LOAD_BEHAVIORS_H

#include "BML/Behavior.h"
#include "BML/Guids/Narratives.h"

namespace bml {

struct ObjectLoadDesc {
    const char *file       = "";
    const char *masterName = "";
    CK_CLASSID  filter     = CKCID_3DOBJECT;
    CKBOOL      addToScene = true;
    CKBOOL      reuseMesh  = true;
    CKBOOL      reuseMtl   = true;
    CKBOOL      dynamic    = true;
};

struct ObjectLoadResult {
    XObjectArray *objects;
    CKObject     *masterObject;
};

class ObjectLoadCache {
public:
    ObjectLoadCache() = default;
    explicit ObjectLoadCache(CKContext *ctx) : m_Ctx(ctx) {}

    bool Init(CKBehavior *owner) {
        if (!owner) return false;
        if (!m_Ctx) m_Ctx = owner->GetCKContext();

        auto *beh = static_cast<CKBehavior *>(m_Ctx->CreateObject(CKCID_BEHAVIOR));
        beh->InitFromGuid(VT_NARRATIVES_OBJECTLOAD);
        owner->AddSubBehavior(beh);
        detail::WireInputSources(beh, owner, false);
        m_Loader = BehaviorRunner(beh);
        return static_cast<bool>(m_Loader);
    }

    ObjectLoadResult Load(const ObjectLoadDesc &d) {
        m_Loader.Input(0, d.file)
                .Input(1, d.masterName)
                .Input(2, d.filter)
                .Input(3, d.addToScene)
                .Input(4, d.reuseMesh)
                .Input(5, d.reuseMtl)
                .Local(0, d.dynamic)
                .Execute();

        return {
            *static_cast<XObjectArray **>(m_Loader.ReadOutputPtr(0)),
            m_Loader.ReadOutputObj(1)
        };
    }

private:
    CKContext      *m_Ctx = nullptr;
    BehaviorRunner  m_Loader;
};

inline CKBehavior *BuildObjectLoad(CKBehavior *script, const ObjectLoadDesc &d) {
    return BehaviorFactory(script, VT_NARRATIVES_OBJECTLOAD)
        .Input(0, d.file)
        .Input(1, d.masterName)
        .Input(2, d.filter)
        .Input(3, d.addToScene)
        .Input(4, d.reuseMesh)
        .Input(5, d.reuseMtl)
        .Local(0, d.dynamic)
        .Build();
}

} // namespace bml

#endif // BML_OBJECT_LOAD_BEHAVIORS_H
