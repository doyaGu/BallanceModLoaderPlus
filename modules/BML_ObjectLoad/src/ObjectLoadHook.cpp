/**
 * @file ObjectLoadHook.cpp
 * @brief Object loading hook implementation
 */

#include "ObjectLoadHook.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "CKAll.h"
#include "BML/Guids/Narratives.h"

#include "bml_virtools_payloads.h"
#include "bml_topics.h"

namespace BML_ObjectLoad {
    //-----------------------------------------------------------------------------
    // Hook State
    //-----------------------------------------------------------------------------

    namespace {
    BML_HookContext s_Hook = BML_HOOK_CONTEXT_INIT;
    }

    static CKContext *s_Context = nullptr;
    static CKBEHAVIORFCT g_OriginalObjectLoad = nullptr;
    static bool s_InitialSnapshotPublished = false;

    // Cached topic IDs
    static BML_TopicId s_TopicLoadObject = 0;
    static BML_TopicId s_TopicLoadScript = 0;
    static BML_TopicId s_TopicCustomMapName = 0;
    static constexpr const char *kInitialLoadFilename = "base.cmo";

    static void Log(BML_LogSeverity severity, const char *message) {
        if (!s_Hook.logging || !s_Hook.logging->Log || !message) {
            return;
        }
        s_Hook.logging->Log(s_Hook.owner, s_Hook.global_context, severity,
                            s_Hook.log_category ? s_Hook.log_category : "BML_ObjectLoad",
                            "%s", message);
    }

    //-----------------------------------------------------------------------------
    // ObjectLoad Behavior Hook
    //-----------------------------------------------------------------------------

    static bool PublishObjectLoadEvent(const char *filename,
                                       const char *mastername,
                                       CK_CLASSID cid,
                                       CKBOOL addtoscene,
                                       CKBOOL reuseMeshes,
                                       CKBOOL reuseMaterials,
                                       CKBOOL dynamic,
                                       CKObject *masterobject,
                                       CKBOOL isMap,
                                       const CK_ID *objectIds,
                                       uint32_t objectCount) {
        auto *imcBus = s_Hook.imc_bus;
        if (!imcBus || !imcBus->PublishBuffer || !s_Hook.owner || s_TopicLoadObject == 0)
            return false;

        const size_t filenameSize = filename ? std::strlen(filename) + 1 : 0;
        const size_t masterNameSize = mastername ? std::strlen(mastername) + 1 : 0;
        const size_t objectIdsSize = static_cast<size_t>(objectCount) * sizeof(CK_ID);
        const size_t totalSize = sizeof(BML_ObjectLoadEvent) + filenameSize + masterNameSize + objectIdsSize;
        auto *storage = static_cast<char *>(std::malloc(totalSize));
        if (!storage) {
            return false;
        }

        auto *event = reinterpret_cast<BML_ObjectLoadEvent *>(storage);
        std::memset(event, 0, sizeof(BML_ObjectLoadEvent));
        char *cursor = storage + sizeof(BML_ObjectLoadEvent);

        if (filenameSize > 0) {
            event->filename = cursor;
            std::memcpy(cursor, filename, filenameSize);
            cursor += filenameSize;
        }
        if (masterNameSize > 0) {
            event->master_name = cursor;
            std::memcpy(cursor, mastername, masterNameSize);
            cursor += masterNameSize;
        }
        if (objectIdsSize > 0 && objectIds) {
            event->object_ids = reinterpret_cast<const CK_ID *>(cursor);
            std::memcpy(cursor, objectIds, objectIdsSize);
        }

        event->class_id = cid;
        event->add_to_scene = addtoscene;
        event->reuse_meshes = reuseMeshes;
        event->reuse_materials = reuseMaterials;
        event->dynamic = dynamic;
        event->master_object = masterobject;
        event->is_map = isMap;
        event->object_count = objectCount;

        BML_ImcBuffer buffer = BML_IMC_BUFFER_INIT;
        buffer.data = event;
        buffer.size = totalSize;
        buffer.cleanup = [](const void *, size_t, void *user_data) {
            std::free(user_data);
        };
        buffer.cleanup_user_data = storage;
        return imcBus->PublishBuffer(s_Hook.owner, s_TopicLoadObject, &buffer) == BML_RESULT_OK;
    }

    static bool PublishScriptLoadEvent(const char *filename, CKBehavior *script, CKBOOL isMap) {
        auto *imcBus = s_Hook.imc_bus;
        if (!imcBus || !imcBus->PublishBuffer || !s_Hook.owner || s_TopicLoadScript == 0 || !script)
            return false;

        const size_t filenameSize = filename ? std::strlen(filename) + 1 : 0;
        const size_t totalSize = sizeof(BML_ScriptLoadEvent) + filenameSize;
        auto *storage = static_cast<char *>(std::malloc(totalSize));
        if (!storage) {
            return false;
        }

        auto *event = reinterpret_cast<BML_ScriptLoadEvent *>(storage);
        std::memset(event, 0, sizeof(BML_ScriptLoadEvent));
        if (filenameSize > 0) {
            event->filename = storage + sizeof(BML_ScriptLoadEvent);
            std::memcpy(const_cast<char *>(event->filename), filename, filenameSize);
        }
        event->script = script;
        event->is_map = isMap;

        BML_ImcBuffer buffer = BML_IMC_BUFFER_INIT;
        buffer.data = event;
        buffer.size = totalSize;
        buffer.cleanup = [](const void *, size_t, void *user_data) {
            std::free(user_data);
        };
        buffer.cleanup_user_data = storage;
        return imcBus->PublishBuffer(s_Hook.owner, s_TopicLoadScript, &buffer) == BML_RESULT_OK;
    }

    static bool IsScriptBehavior(CKObject *object) {
        if (!object || object->GetClassID() != CKCID_BEHAVIOR) {
            return false;
        }

        auto *behavior = static_cast<CKBehavior *>(object);
        return (behavior->GetType() & CKBEHAVIORTYPE_SCRIPT) != 0;
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
            if (s_Hook.imc_bus && isMap && s_TopicCustomMapName != 0) {
                auto *imcBus = s_Hook.imc_bus;
                if (imcBus && imcBus->CopyState) {
                    char custom_map_name[MAX_PATH] = {};
                    size_t state_size = 0;
                    if (imcBus->CopyState(s_TopicCustomMapName,
                                            custom_map_name,
                                            sizeof(custom_map_name),
                                            &state_size,
                                            nullptr) == BML_RESULT_OK &&
                        custom_map_name[0] != '\0') {
                        fname = custom_map_name;
                    }
                }
            }

            // Publish OnLoadObject event via IMC
            PublishObjectLoadEvent(fname,
                                   mastername,
                                   cid,
                                   addtoscene,
                                   reuseMeshes,
                                   reuseMaterials,
                                   dynamic,
                                   masterobject,
                                   isMap ? TRUE : FALSE,
                                   oarray->Begin(),
                                   static_cast<uint32_t>(oarray->Size()));

            // Publish OnLoadScript events for loaded scripts
            for (CK_ID *id = oarray->Begin(); id != oarray->End(); id++) {
                CKObject *obj = ctx->GetObject(*id);
                if (IsScriptBehavior(obj)) {
                    PublishScriptLoadEvent(fname, static_cast<CKBehavior *>(obj), isMap ? TRUE : FALSE);
                }
            }

            if (s_Hook.imc_bus && isMap && s_TopicCustomMapName != 0) {
                auto *imcBus = s_Hook.imc_bus;
                if (imcBus && imcBus->ClearState) {
                    imcBus->ClearState(s_TopicCustomMapName);
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

    bool InitializeObjectLoadHook(CKContext *context, const BML_HookContext *ctx) {
        if (!context)
            return false;

        if (ctx) s_Hook = *ctx;
        s_Context = context;

        auto *imcBus = s_Hook.imc_bus;
        if (imcBus && imcBus->GetTopicId) {
            imcBus->GetTopicId(BML_TOPIC_OBJECTLOAD_LOAD_OBJECT, &s_TopicLoadObject);
            imcBus->GetTopicId(BML_TOPIC_OBJECTLOAD_LOAD_SCRIPT, &s_TopicLoadScript);
            imcBus->GetTopicId(BML_TOPIC_STATE_CUSTOM_MAP_NAME, &s_TopicCustomMapName);
        }

        // Hook ObjectLoad BB
        CKBehaviorPrototype *objectLoadProto = CKGetPrototypeFromGuid(VT_NARRATIVES_OBJECTLOAD);
        if (objectLoadProto) {
            if (!g_OriginalObjectLoad)
                g_OriginalObjectLoad = objectLoadProto->GetFunction();
            objectLoadProto->SetFunction(&ObjectLoadHook);
            PublishInitialLoadSnapshot(context);
            return true;
        }

        return false;
    }

    bool PublishInitialLoadSnapshot(CKContext *context) {
        if (!context) {
            return false;
        }

        if (s_InitialSnapshotPublished) {
            return true;
        }

        const int behaviorCount = context->GetObjectsCountByClassID(CKCID_BEHAVIOR);
        CK_ID *behaviorIds = context->GetObjectsListByClassID(CKCID_BEHAVIOR);
        if (behaviorCount <= 0 || !behaviorIds) {
            return false;
        }

        int publishedScripts = 0;
        if (s_TopicLoadObject != 0) {
            PublishObjectLoadEvent(kInitialLoadFilename,
                                   "",
                                   CKCID_3DOBJECT,
                                   TRUE,
                                   TRUE,
                                   TRUE,
                                   FALSE,
                                   nullptr,
                                   FALSE,
                                   nullptr,
                                   0);
        }

        for (int index = 0; index < behaviorCount; ++index) {
            CKObject *object = context->GetObject(behaviorIds[index]);
            if (!IsScriptBehavior(object)) {
                continue;
            }

            if (PublishScriptLoadEvent(kInitialLoadFilename,
                                       static_cast<CKBehavior *>(object),
                                       FALSE)) {
                ++publishedScripts;
            }
        }

        if (publishedScripts == 0 && s_TopicLoadObject == 0) {
            return false;
        }

        s_InitialSnapshotPublished = true;
        if (s_Hook.logging && s_Hook.logging->Log) {
            char msg[128];
            std::snprintf(msg, sizeof(msg), "Published initial load snapshot for %d scripts", publishedScripts);
            Log(BML_LOG_INFO, msg);
        }
        return true;
    }

    void ShutdownObjectLoadHook() {
        // Restore ObjectLoad BB
        CKBehaviorPrototype *objectLoadProto = CKGetPrototypeFromGuid(VT_NARRATIVES_OBJECTLOAD);
        if (objectLoadProto && g_OriginalObjectLoad) {
            objectLoadProto->SetFunction(g_OriginalObjectLoad);
        }

        s_Context = nullptr;
        s_InitialSnapshotPublished = false;
        s_TopicLoadObject = 0;
        s_TopicLoadScript = 0;
        s_TopicCustomMapName = 0;
        s_Hook = {};
    }
} // namespace BML_ObjectLoad
