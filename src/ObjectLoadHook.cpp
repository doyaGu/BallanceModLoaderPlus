#include "ModManager.h"

static CKBEHAVIORFCT g_ObjectLoad = nullptr;

int ObjectLoad(const CKBehaviorContext &behcontext) {
    CKBehavior *beh = behcontext.Behavior;
    CKContext *ctx = behcontext.Context;

    // Loading
    if (beh->IsInputActive(0)) {
        beh->ActivateInput(0, FALSE);

        // Dynamic ?
        CKBOOL dynamic = TRUE;
        beh->GetLocalParameterValue(0, &dynamic);

        CKBOOL addtoscene = TRUE;
        beh->GetInputParameterValue(3, &addtoscene);

        CKScene *scene = ctx->GetCurrentScene();
        if (ctx->GetCurrentLevel()->GetLevelScene() == scene)
            addtoscene = FALSE;

        auto fname = (const char *) beh->GetInputParameterReadDataPtr(0);
        auto mastername = (const char *) beh->GetInputParameterReadDataPtr(1);
        CK_CLASSID cid = CKCID_3DOBJECT;
        beh->GetInputParameterValue(2, &cid);

        auto loadoptions = (CK_LOAD_FLAGS) (CK_LOAD_DEFAULT | CK_LOAD_AUTOMATICMODE);
        if (dynamic)
            loadoptions = (CK_LOAD_FLAGS) (loadoptions | CK_LOAD_AS_DYNAMIC_OBJECT);

        CKObjectArray *array = CreateCKObjectArray();

        CKBOOL reuseMeshes = FALSE;
        beh->GetInputParameterValue(4, &reuseMeshes);

        CKBOOL reuseMaterials = FALSE;
        beh->GetInputParameterValue(5, &reuseMaterials);

        ctx->SetAutomaticLoadMode(CKLOAD_OK, CKLOAD_OK, reuseMeshes ? CKLOAD_USECURRENT : CKLOAD_OK,
                                  reuseMaterials ? CKLOAD_USECURRENT : CKLOAD_OK);

        // We load the file
        XString filename(fname);
        if (ctx->GetPathManager()->ResolveFileName(filename, DATA_PATH_IDX, -1) != CK_OK) {
            ctx->OutputToConsoleEx("Unable to find %s", fname);
        }

        if (ctx->Load(filename.Str(), array, loadoptions) != CK_OK) {
            beh->ActivateOutput(2);
            return CKBR_OK;
        } else {
            beh->ActivateOutput(0);
        }

        XObjectArray *oarray = *(XObjectArray **) beh->GetOutputParameterWriteDataPtr(0);
        oarray->Clear();

        CKLevel *level = behcontext.CurrentLevel;
        CKObject *masterobject = NULL;

        CKLevel *loadedLevel = NULL;

        // If there is a level in the Loaded Object
        for (array->Reset(); !array->EndOfList(); array->Next()) {
            CKObject *o = array->GetData(ctx);
            if (CKIsChildClassOf(o, CKCID_LEVEL)) {
                loadedLevel = (CKLevel *) o;
            }

            // We search here for the master object
            if (CKIsChildClassOf(o, cid)) {
                if (mastername && *mastername != 0) {
                    char *objectName = o->GetName();
                    if (objectName && !strcmp(objectName, mastername)) {
                        masterobject = o;
                    }
                } else {
                    if (CKIsChildClassOf(o, CKCID_3DENTITY)) {
                        if (!((CK3dEntity *) o)->GetParent())
                            masterobject = o;
                    } else if (CKIsChildClassOf(o, CKCID_2DENTITY)) {
                        if (!((CK2dEntity *) o)->GetParent())
                            masterobject = o;
                    }
                }
            }

            oarray->PushBack(o->GetID());
        }

        if (loadedLevel) {
            // If a level is loaded, do a merge
            level->Merge(loadedLevel, FALSE);
            oarray->RemoveObject(loadedLevel);
            ctx->DestroyObject(loadedLevel);
        } else {
            // else add everything to the level / scene

            level->BeginAddSequence(TRUE);

            for (array->Reset(); !array->EndOfList(); array->Next()) {
                CKObject *o = array->GetData(ctx);
                if (CKIsChildClassOf(o, CKCID_SCENE)) {
                    level->AddScene((CKScene *) o);
                } else {
                    level->AddObject(o);
                }

                if (addtoscene && CKIsChildClassOf(o, CKCID_SCENEOBJECT) &&
                    !(CKIsChildClassOf(o, CKCID_LEVEL) || CKIsChildClassOf(o, CKCID_SCENE)))
                    scene->AddObjectToScene((CKSceneObject *) o);
            }

            level->BeginAddSequence(FALSE);
        }

        DeleteCKObjectArray(array);
        beh->SetOutputParameterObject(1, masterobject);

        CKBOOL isMap = strcmp(beh->GetOwnerScript()->GetName(), "Levelinit_build") == 0;

        auto ds = BML_GetModManager()->GetDataShare(nullptr);

        if (isMap) {
            auto mapName = (const char *) ds->Get("CustomMapName", nullptr);
            if (mapName) {
                fname = mapName;
            }
        }

        BML_GetModManager()->BroadcastCallback(&IMod::OnLoadObject,
                                               fname, isMap, mastername, cid,
                                               addtoscene, reuseMeshes,
                                               reuseMaterials, dynamic, oarray,
                                               masterobject);

        for (CK_ID *id = oarray->Begin(); id != oarray->End(); id++) {
            CKObject *obj = BML_GetModManager()->GetCKContext()->GetObject(*id);
            if (obj && obj->GetClassID() == CKCID_BEHAVIOR) {
                auto *behavior = (CKBehavior *) obj;
                if ((behavior->GetType() & CKBEHAVIORTYPE_SCRIPT) != 0) {
                    BML_GetModManager()->BroadcastCallback(&IMod::OnLoadScript, fname, behavior);
                }
            }
        }

        if (isMap) {
            ds->Remove("CustomMapName");
        }
    }

    // Unloading
    if (beh->IsInputActive(1)) {
        beh->ActivateInput(1, FALSE);
        beh->ActivateOutput(1);

        XObjectArray *oarray = *(XObjectArray **) beh->GetOutputParameterWriteDataPtr(0);
        ctx->DestroyObjects(oarray->Begin(), oarray->Size());
        oarray->Clear();
    }

    beh->GetOutputParameter(0)->DataChanged();
    return CKBR_OK;
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