#include "BML/Guids/Narratives.h"

#include <cstring>
#include <string>

#include "ModContext.h"

static CKBEHAVIORFCT g_ObjectLoad = nullptr;

int ObjectLoad(const CKBehaviorContext &behcontext) {
    CKBehavior *beh = behcontext.Behavior;
    if (!g_ObjectLoad)
        return CKBR_OK;

    const bool wasLoading = beh->IsInputActive(0) != FALSE;

    CKBOOL dynamic = TRUE;
    CKBOOL addtoscene = TRUE;
    CKBOOL reuseMeshes = FALSE;
    CKBOOL reuseMaterials = FALSE;
    CK_CLASSID cid = CKCID_3DOBJECT;
    std::string fname;
    std::string mastername;

    if (wasLoading) {
        beh->GetLocalParameterValue(0, &dynamic);
        beh->GetInputParameterValue(2, &cid);
        beh->GetInputParameterValue(3, &addtoscene);
        beh->GetInputParameterValue(4, &reuseMeshes);
        beh->GetInputParameterValue(5, &reuseMaterials);

        if (const auto *value = static_cast<const char *>(beh->GetInputParameterReadDataPtr(0)))
            fname = value;
        if (const auto *value = static_cast<const char *>(beh->GetInputParameterReadDataPtr(1)))
            mastername = value;
    }

    const int result = g_ObjectLoad(behcontext);

    if (!wasLoading)
        return result;

    auto *modContext = BML_GetModContext();
    if (!modContext)
        return result;

    XObjectArray *oarray = nullptr;
    if (void *outputArrayPtr = beh->GetOutputParameterWriteDataPtr(0))
        oarray = *static_cast<XObjectArray **>(outputArrayPtr);
    if (!oarray)
        return result;

    CKObject *masterobject = beh->GetOutputParameterObject(1);
    CKBehavior *ownerScript = beh->GetOwnerScript();
    CKBOOL isMap = ownerScript && std::strcmp(ownerScript->GetName(), "Levelinit_build") == 0;

    BML_DataShare *ds = modContext->GetDataShare(nullptr);
    std::string callbackName = fname;
    if (isMap && ds) {
        size_t nameSize = BML_DataShare_SizeOf(ds, "CustomMapName");
        if (nameSize > 0) {
            callbackName.resize(nameSize - 1);
            BML_DataShare_Copy(ds, "CustomMapName", callbackName.data(), nameSize);
        }
    }

    modContext->BroadcastCallback(&IMod::OnLoadObject,
                                  callbackName.c_str(), isMap, mastername.c_str(), cid,
                                  addtoscene, reuseMeshes, reuseMaterials, dynamic, oarray,
                                  masterobject);

    for (CK_ID *id = oarray->Begin(); id != oarray->End(); id++) {
        CKObject *obj = modContext->GetCKContext()->GetObject(*id);
        if (obj && obj->GetClassID() == CKCID_BEHAVIOR) {
            auto *behavior = static_cast<CKBehavior *>(obj);
            if ((behavior->GetType() & CKBEHAVIORTYPE_SCRIPT) != 0)
                modContext->BroadcastCallback(&IMod::OnLoadScript, callbackName.c_str(), behavior);
        }
    }

    if (isMap && ds)
        BML_DataShare_Remove(ds, "CustomMapName");

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
    if (!g_ObjectLoad) return false;
    objectLoadProto->SetFunction(g_ObjectLoad);
    return true;
}
