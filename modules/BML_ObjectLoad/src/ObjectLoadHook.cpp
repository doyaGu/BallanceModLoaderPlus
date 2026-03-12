/**
 * @file ObjectLoadHook.cpp
 * @brief Object loading hook implementation
 */

#include "ObjectLoadHook.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <cstdlib>
#include <cstring>

#include "CKAll.h"
#include "BML/Guids/Narratives.h"

#include "bml_virtools_payloads.h"
#include "bml_topics.h"

// BML Core APIs (via loader)
#include "bml_loader.h"

namespace BML_ObjectLoad {
    //-----------------------------------------------------------------------------
    // Hook State
    //-----------------------------------------------------------------------------

    static CKContext *s_Context = nullptr;
    static CKBEHAVIORFCT g_OriginalObjectLoad = nullptr;

    // Cached topic IDs
    static BML_TopicId s_TopicLoadObject = 0;
    static BML_TopicId s_TopicLoadScript = 0;

    //-----------------------------------------------------------------------------
    // ObjectLoad Behavior Hook
    //-----------------------------------------------------------------------------

    static char *DuplicateCString(const char *value) {
        if (!value) {
            return nullptr;
        }

        const size_t length = std::strlen(value) + 1;
        auto *copy = static_cast<char *>(std::malloc(length));
        if (copy) {
            std::memcpy(copy, value, length);
        }
        return copy;
    }

    static void CleanupObjectLoadPayload(const void *data, size_t size, void *user_data) {
        (void)size;
        (void)user_data;

        auto *payload = static_cast<BML_LegacyObjectLoadPayload *>(const_cast<void *>(data));
        if (!payload) {
            return;
        }

        std::free(const_cast<char *>(payload->filename));
        std::free(const_cast<char *>(payload->master_name));
        delete[] payload->object_ids;
        delete payload;
    }

    static void CleanupScriptLoadPayload(const void *data, size_t size, void *user_data) {
        (void)size;
        (void)user_data;

        auto *payload = static_cast<BML_LegacyScriptLoadPayload *>(const_cast<void *>(data));
        if (!payload) {
            return;
        }

        std::free(const_cast<char *>(payload->filename));
        delete payload;
    }

    /**
     * @brief ObjectLoad behavior hook
     *
     * Intercepts the ObjectLoad BB to publish IMC events when objects
     * and scripts are loaded.
     */
    int ObjectLoadHook(const CKBehaviorContext &behcontext) {
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

            // Resolve and load file
            XString filename(fname);
            if (ctx->GetPathManager()->ResolveFileName(filename, DATA_PATH_IDX, -1) != CK_OK) {
                ctx->OutputToConsoleEx("Unable to find %s", fname);
            }

            if (ctx->Load(filename.Str(), array, loadoptions) != CK_OK) {
                DeleteCKObjectArray(array);
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

            // Process loaded objects
            for (array->Reset(); !array->EndOfList(); array->Next()) {
                CKObject *o = array->GetData(ctx);
                if (CKIsChildClassOf(o, CKCID_LEVEL)) {
                    loadedLevel = (CKLevel *) o;
                }

                // Search for the master object
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
                level->Merge(loadedLevel, FALSE);
                oarray->RemoveObject(loadedLevel);
                ctx->DestroyObject(loadedLevel);
            } else {
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

            // Publish OnLoadObject event via IMC
            if (bmlImcPublishBuffer && s_TopicLoadObject) {
                auto *payload = new BML_LegacyObjectLoadPayload{};
                payload->struct_size = sizeof(BML_LegacyObjectLoadPayload);
                payload->filename = DuplicateCString(fname);
                payload->master_name = DuplicateCString(mastername);
                payload->class_id = cid;
                payload->add_to_scene = addtoscene;
                payload->reuse_meshes = reuseMeshes;
                payload->reuse_materials = reuseMaterials;
                payload->dynamic = dynamic;
                payload->master_object = masterobject;
                payload->is_map = isMap ? TRUE : FALSE;
                payload->object_count = static_cast<uint32_t>(oarray->Size());
                payload->object_ids = payload->object_count > 0 ? new CK_ID[payload->object_count] : nullptr;

                for (uint32_t index = 0; index < payload->object_count; ++index) {
                    payload->object_ids[index] = *(oarray->Begin() + index);
                }

                BML_ImcBuffer buffer = BML_IMC_BUFFER_INIT;
                buffer.data = payload;
                buffer.size = sizeof(BML_LegacyObjectLoadPayload);
                buffer.cleanup = &CleanupObjectLoadPayload;
                bmlImcPublishBuffer(s_TopicLoadObject, &buffer);
            }

            // Publish OnLoadScript events for loaded scripts
            if (bmlImcPublishBuffer && s_TopicLoadScript) {
                for (CK_ID *id = oarray->Begin(); id != oarray->End(); id++) {
                    CKObject *obj = ctx->GetObject(*id);
                    if (obj && obj->GetClassID() == CKCID_BEHAVIOR) {
                        auto *behavior = (CKBehavior *) obj;
                        if ((behavior->GetType() & CKBEHAVIORTYPE_SCRIPT) != 0) {
                            auto *scriptPayload = new BML_LegacyScriptLoadPayload{};
                            scriptPayload->struct_size = sizeof(BML_LegacyScriptLoadPayload);
                            scriptPayload->filename = DuplicateCString(fname);
                            scriptPayload->script = behavior;
                            scriptPayload->is_map = isMap ? TRUE : FALSE;

                            BML_ImcBuffer buffer = BML_IMC_BUFFER_INIT;
                            buffer.data = scriptPayload;
                            buffer.size = sizeof(BML_LegacyScriptLoadPayload);
                            buffer.cleanup = &CleanupScriptLoadPayload;
                            bmlImcPublishBuffer(s_TopicLoadScript, &buffer);
                        }
                    }
                }
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

    //-----------------------------------------------------------------------------
    // Public API
    //-----------------------------------------------------------------------------

    bool InitializeObjectLoadHook(CKContext *context) {
        if (!context)
            return false;

        s_Context = context;

        // Register topics using loader API
        if (bmlImcGetTopicId) {
            bmlImcGetTopicId(BML_TOPIC_OBJECTLOAD_LOAD_OBJECT, &s_TopicLoadObject);
            bmlImcGetTopicId(BML_TOPIC_OBJECTLOAD_LOAD_SCRIPT, &s_TopicLoadScript);
        }

        // Hook ObjectLoad BB
        CKBehaviorPrototype *objectLoadProto = CKGetPrototypeFromGuid(VT_NARRATIVES_OBJECTLOAD);
        if (objectLoadProto) {
            if (!g_OriginalObjectLoad)
                g_OriginalObjectLoad = objectLoadProto->GetFunction();
            objectLoadProto->SetFunction(&ObjectLoadHook);
            return true;
        }

        return false;
    }

    void ShutdownObjectLoadHook() {
        // Restore ObjectLoad BB
        CKBehaviorPrototype *objectLoadProto = CKGetPrototypeFromGuid(VT_NARRATIVES_OBJECTLOAD);
        if (objectLoadProto && g_OriginalObjectLoad) {
            objectLoadProto->SetFunction(g_OriginalObjectLoad);
        }
    }
} // namespace BML_ObjectLoad
