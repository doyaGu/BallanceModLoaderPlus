#include "ModLoader.h"

namespace {
    CKBEHAVIORFCT g_ObjectLoad = nullptr;
}

int ObjectLoad(const CKBehaviorContext &behcontext) {
    CKBehavior *beh = behcontext.Behavior;
    bool active = beh->IsInputActive(0);

    int result = g_ObjectLoad(behcontext);

    if (active) {
        const char *filename = (const char *) beh->GetInputParameterReadDataPtr(0);
        const char *mastername = (const char *) beh->GetInputParameterReadDataPtr(1);
        CK_CLASSID cid = CKCID_3DOBJECT;
        beh->GetInputParameterValue(2, &cid);
        CKBOOL addToScene = TRUE, reuseMeshes = FALSE, reuseMaterials = FALSE;
        beh->GetInputParameterValue(3, &addToScene);
        beh->GetInputParameterValue(4, &reuseMeshes);
        beh->GetInputParameterValue(5, &reuseMaterials);

        CKBOOL dynamic = TRUE;
        beh->GetLocalParameterValue(0, &dynamic);

        XObjectArray *oarray = *(XObjectArray **) beh->GetOutputParameterWriteDataPtr(0);
        CKObject *masterobject = beh->GetOutputParameterObject(1);
        CKBOOL isMap = !strcmp(beh->GetOwnerScript()->GetName(), "Levelinit_build");

        ModLoader::GetInstance().BroadcastCallback(&IMod::OnLoadObject,
                                                   filename, isMap, mastername, cid,
                                                   addToScene, reuseMeshes,
                                                   reuseMaterials, dynamic, oarray,
                                                   masterobject);

        for (CK_ID *id = oarray->Begin(); id != oarray->End(); id++) {
            CKObject *obj = ModLoader::GetInstance().GetCKContext()->GetObject(*id);
            if (obj->GetClassID() == CKCID_BEHAVIOR) {
                auto *behavior = (CKBehavior *) obj;
                if (behavior->GetType() == CKBEHAVIORTYPE_SCRIPT) {
                    ModLoader::GetInstance().BroadcastCallback(&IMod::OnLoadScript, filename, behavior);
                }
            }
        }
    }

    return result;
}

bool HookObjectLoad() {
    CKBehaviorPrototype *objectLoadProto = CKGetPrototypeFromGuid(VT_NARRATIVES_OBJECTLOAD);
    if (!objectLoadProto) return false;
    if (!g_ObjectLoad) g_ObjectLoad = objectLoadProto->GetFunction();
    objectLoadProto->SetFunction(&ObjectLoad);
    return true;
}

bool UnhookObjectLoad() {
    CKBehaviorPrototype *objectLoadProto = CKGetPrototypeFromGuid(VT_NARRATIVES_OBJECTLOAD);
    if (!objectLoadProto) return false;
    objectLoadProto->SetFunction(g_ObjectLoad);
    return true;
}