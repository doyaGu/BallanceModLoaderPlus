#include "BML/Guids/Narratives.h"

#include <cstring>
#include <string>
#include <vector>

#include "BML/ILogger.h"
#include "CustomMaps/CustomMapLoad.h"
#include "Hooks/BehaviorFunctionPatch.h"
#include "Hooks/HookLifecycle.h"
#include "Loader/ModContext.h"

namespace {

BehaviorFunctionPatch g_ObjectLoadPatch;

bool ReadSharedString(const BML_DataShare *share, const char *key,
                      std::string &value) {
    std::size_t size = 0;
    (void) BML_DataShare_CopyEx(share, key, nullptr, 0, &size);
    for (int retry = 0; retry != 3 && size != 0; ++retry) {
        std::vector<char> buffer(size, '\0');
        std::size_t fullSize = 0;
        const int copied = BML_DataShare_CopyEx(
            share, key, buffer.data(), buffer.size(), &fullSize);
        if (copied == 1 && fullSize == buffer.size() && buffer.back() == '\0') {
            value.assign(buffer.data(), buffer.size() - 1u);
            return true;
        }
        size = fullSize;
    }
    return false;
}

void PublishCustomMapResult(ModContext &context, BML_DataShare *share,
                            const CustomMapLoad::Request &request,
                            CustomMapLoad::Outcome outcome) {
    if (!CustomMapLoad::WriteResult(share, request.Attempt, outcome)) {
        if (ILogger *logger = context.GetLogger())
            logger->Error("Failed to publish custom map Object Load result");
    }
    BML_DataShare_Remove(share, CustomMapLoad::RequestKey);
    BML_DataShare_Remove(share, CustomMapLoad::NameKey);
}

int ObjectLoad(const CKBehaviorContext &behcontext) {
    CKBehavior *beh = behcontext.Behavior;
    const CKBEHAVIORFCT original = g_ObjectLoadPatch.Original();
    if (!original)
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

    auto *modContext = BML_GetModContext();
    CKBehavior *ownerScript = beh->GetOwnerScript();
    const bool isMap = ownerScript && ownerScript->GetName() &&
        std::strcmp(ownerScript->GetName(), "Levelinit_build") == 0;
    BML_DataShare *dataShare = modContext ? modContext->GetDataShare(nullptr) : nullptr;
    CustomMapLoad::Request request;
    const bool customMap = wasLoading && isMap && dataShare &&
        CustomMapLoad::ReadRequest(dataShare, request);
    std::string callbackName = fname;
    if (wasLoading && isMap && dataShare)
        (void) ReadSharedString(dataShare, CustomMapLoad::NameKey, callbackName);

    const int result = original(behcontext);

    if (!wasLoading || !modContext)
        return result;

    XObjectArray *oarray = nullptr;
    if (void *outputArrayPtr = beh->GetOutputParameterWriteDataPtr(0))
        oarray = *static_cast<XObjectArray **>(outputArrayPtr);
    const bool loaded = oarray && beh->IsOutputActive(0) != FALSE &&
        beh->IsOutputActive(2) == FALSE;
    if (!loaded) {
        if (customMap) {
            PublishCustomMapResult(
                *modContext, dataShare, request, CustomMapLoad::Outcome::Failed);
        }
        return result;
    }

    CKObject *masterobject = beh->GetOutputParameterObject(1);

    CKContext *ckContext = modContext->GetCKContext();
    modContext->BroadcastCallback(&IMod::OnLoadObject,
                                  callbackName.c_str(), isMap, mastername.c_str(), cid,
                                  addtoscene, reuseMeshes, reuseMaterials, dynamic, oarray,
                                  masterobject);

    for (CK_ID *id = oarray->Begin(); ckContext && id != oarray->End(); id++) {
        CKObject *obj = ckContext->GetObject(*id);
        if (obj && obj->GetClassID() == CKCID_BEHAVIOR) {
            auto *behavior = static_cast<CKBehavior *>(obj);
            if ((behavior->GetType() & CKBEHAVIORTYPE_SCRIPT) != 0) {
                modContext->BehaviorScriptLoaded(behavior);
                modContext->BroadcastCallback(&IMod::OnLoadScript, callbackName.c_str(), behavior);
            }
        }
    }

    if (customMap) {
        PublishCustomMapResult(
            *modContext, dataShare, request, CustomMapLoad::Outcome::Loaded);
    } else if (isMap && dataShare) {
        BML_DataShare_Remove(dataShare, CustomMapLoad::NameKey);
    }

    return result;
}

} // namespace

bool HookObjectLoad() {
    const BehaviorFunctionPatchResult result = g_ObjectLoadPatch.Install(
        VT_NARRATIVES_OBJECTLOAD, &ObjectLoad);
    if (!result) {
        if (ModContext *context = BML_GetModContext()) {
            if (ILogger *logger = context->GetLogger())
                logger->Error("Object Load hook installation failed: %s",
                              BehaviorFunctionPatch::GetErrorName(result.Code));
        }
    }
    return static_cast<bool>(result);
}

bool UnhookObjectLoad() {
    const BehaviorFunctionPatchResult result = g_ObjectLoadPatch.Remove();
    if (result.Code == BehaviorFunctionPatchError::OwnershipLost) {
        if (ModContext *context = BML_GetModContext()) {
            if (ILogger *logger = context->GetLogger())
                logger->Warn("Object Load hook ownership changed; preserving the current function");
        }
        return true;
    }
    return static_cast<bool>(result);
}

bool IsObjectLoadHookInstalled() {
    return g_ObjectLoadPatch.IsInstalled();
}
