/**
 * @file ObjectLoadMod.cpp
 * @brief BML ObjectLoad Module - intercepts ObjectLoad behavior to notify mods.
 *
 * Hooks the Virtools ObjectLoad BB to publish IMC events when objects and
 * scripts are loaded, and emits a synthetic snapshot for base-game scripts.
 */

#define BML_LOADER_IMPLEMENTATION
#include "bml_hook_module.hpp"
#include "bml_imc_topic.hpp"
#include "bml_imc_message.hpp"
#include "bml_virtools.h"
#include "bml_virtools.hpp"
#include "bml_virtools_payloads.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <cstdlib>
#include <cstring>

#include "CKAll.h"
#include "BML/Guids/Narratives.h"

class ObjectLoadMod : public bml::HookModule {
    CKBEHAVIORFCT m_OriginalFunc = nullptr;
    bool m_SnapshotPublished = false;
    bml::imc::Topic m_TopicLoadObject;
    bml::imc::Topic m_TopicLoadScript;
    bml::imc::Topic m_TopicCustomMapName;

    static constexpr const char *kBaseFilename = "base.cmo";

    // -------------------------------------------------------------------------
    // HookModule overrides
    // -------------------------------------------------------------------------

    const char *HookLogCategory() const override { return "BML_ObjectLoad"; }

    bool InitHook(CKContext *ctx) override {
        if (!ctx) return false;

        auto *imcBus = Services().Interfaces().ImcBus;
        auto owner = Services().Handle();
        m_TopicLoadObject = bml::imc::Topic(BML_TOPIC_OBJECTLOAD_LOAD_OBJECT, imcBus, owner);
        m_TopicLoadScript = bml::imc::Topic(BML_TOPIC_OBJECTLOAD_LOAD_SCRIPT, imcBus, owner);
        m_TopicCustomMapName = bml::imc::Topic(BML_TOPIC_STATE_CUSTOM_MAP_NAME, imcBus, owner);

        CKBehaviorPrototype *proto = CKGetPrototypeFromGuid(VT_NARRATIVES_OBJECTLOAD);
        if (!proto) return false;

        if (!m_OriginalFunc)
            m_OriginalFunc = proto->GetFunction();
        proto->SetFunction(&ObjectLoadCallback);
        return true;
    }

    void ShutdownHook() override {
        CKBehaviorPrototype *proto = CKGetPrototypeFromGuid(VT_NARRATIVES_OBJECTLOAD);
        if (proto && m_OriginalFunc)
            proto->SetFunction(m_OriginalFunc);

        m_OriginalFunc = nullptr;
        m_SnapshotPublished = false;
        m_TopicLoadObject = {};
        m_TopicLoadScript = {};
        m_TopicCustomMapName = {};
    }

    BML_Result OnModuleAttach() override {
        CKContext *ctx = bml::virtools::GetCKContext(Services());
        TryInitHook(ctx);

        // Publish initial snapshot on Engine/Play -- by this point all modules
        // have attached and registered their subscriptions.
        m_Subs.Add(BML_TOPIC_ENGINE_PLAY, [this](const bml::imc::Message &msg) {
            auto *payload = bml::ValidateEnginePayload<BML_EnginePlayEvent>(msg);
            if (!payload) return;
            TryInitHook(payload->context);
            if (m_HookReady)
                PublishInitialSnapshot(payload->context);
        });

        return BML_RESULT_OK;
    }

    // -------------------------------------------------------------------------
    // ObjectLoad BB callback
    // -------------------------------------------------------------------------

    static int ObjectLoadCallback(const CKBehaviorContext &behcontext) {
        auto *self = bml::GetModuleInstance<ObjectLoadMod>();
        if (self)
            return self->HandleObjectLoad(behcontext);
        return CKBR_OK;
    }

    int HandleObjectLoad(const CKBehaviorContext &behcontext) {
        CKBehavior *beh = behcontext.Behavior;
        CKContext *ctx = behcontext.Context;

        if (beh->IsInputActive(0)) {
            beh->ActivateInput(0, FALSE);

            CKBOOL dynamic = TRUE;
            beh->GetLocalParameterValue(0, &dynamic);

            CKBOOL addtoscene = TRUE;
            beh->GetInputParameterValue(3, &addtoscene);

            CKScene *scene = ctx->GetCurrentScene();
            CKLevel *currentLevel = ctx->GetCurrentLevel();
            if (currentLevel && currentLevel->GetLevelScene() == scene)
                addtoscene = FALSE;

            auto fname = (const char *)beh->GetInputParameterReadDataPtr(0);
            auto mastername = (const char *)beh->GetInputParameterReadDataPtr(1);
            CK_CLASSID cid = CKCID_3DOBJECT;
            beh->GetInputParameterValue(2, &cid);

            auto loadoptions = (CK_LOAD_FLAGS)(CK_LOAD_DEFAULT | CK_LOAD_AUTOMATICMODE);
            if (dynamic)
                loadoptions = (CK_LOAD_FLAGS)(loadoptions | CK_LOAD_AS_DYNAMIC_OBJECT);

            CKObjectArray *array = CreateCKObjectArray();

            CKBOOL reuseMeshes = FALSE;
            beh->GetInputParameterValue(4, &reuseMeshes);

            CKBOOL reuseMaterials = FALSE;
            beh->GetInputParameterValue(5, &reuseMaterials);

            ctx->SetAutomaticLoadMode(CKLOAD_OK, CKLOAD_OK,
                                      reuseMeshes ? CKLOAD_USECURRENT : CKLOAD_OK,
                                      reuseMaterials ? CKLOAD_USECURRENT : CKLOAD_OK);

            XString filename(fname);
            if (ctx->GetPathManager()->ResolveFileName(filename, DATA_PATH_IDX, -1) != CK_OK)
                ctx->OutputToConsoleEx("Unable to find %s", fname);

            if (ctx->Load(filename.Str(), array, loadoptions) != CK_OK) {
                DeleteCKObjectArray(array);
                beh->ActivateOutput(2);
                return CKBR_OK;
            } else {
                beh->ActivateOutput(0);
            }

            XObjectArray *oarray = *(XObjectArray **)beh->GetOutputParameterWriteDataPtr(0);
            oarray->Clear();

            CKLevel *level = behcontext.CurrentLevel;
            CKObject *masterobject = NULL;
            CKLevel *loadedLevel = NULL;

            for (array->Reset(); !array->EndOfList(); array->Next()) {
                CKObject *o = array->GetData(ctx);
                if (CKIsChildClassOf(o, CKCID_LEVEL))
                    loadedLevel = (CKLevel *)o;

                if (CKIsChildClassOf(o, cid)) {
                    if (mastername && *mastername != 0) {
                        char *objectName = o->GetName();
                        if (objectName && !strcmp(objectName, mastername))
                            masterobject = o;
                    } else {
                        if (CKIsChildClassOf(o, CKCID_3DENTITY)) {
                            if (!((CK3dEntity *)o)->GetParent())
                                masterobject = o;
                        } else if (CKIsChildClassOf(o, CKCID_2DENTITY)) {
                            if (!((CK2dEntity *)o)->GetParent())
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
                        level->AddScene((CKScene *)o);
                    } else {
                        level->AddObject(o);
                    }

                    if (addtoscene && CKIsChildClassOf(o, CKCID_SCENEOBJECT) &&
                        !(CKIsChildClassOf(o, CKCID_LEVEL) || CKIsChildClassOf(o, CKCID_SCENE)))
                        scene->AddObjectToScene((CKSceneObject *)o);
                }

                level->BeginAddSequence(FALSE);
            }

            DeleteCKObjectArray(array);
            beh->SetOutputParameterObject(1, masterobject);

            CKBOOL isMap = FALSE;
            CKBehavior *ownerScript = beh->GetOwnerScript();
            if (ownerScript) {
                const char *scriptName = ownerScript->GetName();
                if (scriptName && strcmp(scriptName, "Levelinit_build") == 0)
                    isMap = TRUE;
            }

            char custom_map_name[MAX_PATH] = {};
            if (isMap && m_TopicCustomMapName) {
                size_t state_size = 0;
                if (m_TopicCustomMapName.CopyState(custom_map_name,
                                                    sizeof(custom_map_name),
                                                    &state_size) == BML_RESULT_OK &&
                    custom_map_name[0] != '\0') {
                    fname = custom_map_name;
                }
            }

            PublishObjectLoadEvent(fname, mastername, cid, addtoscene,
                                   reuseMeshes, reuseMaterials, dynamic,
                                   masterobject, isMap,
                                   oarray->Begin(),
                                   static_cast<uint32_t>(oarray->Size()));

            for (CK_ID *id = oarray->Begin(); id != oarray->End(); id++) {
                CKObject *obj = ctx->GetObject(*id);
                if (IsScriptBehavior(obj))
                    PublishScriptLoadEvent(fname, static_cast<CKBehavior *>(obj), isMap);
            }

            if (isMap && m_TopicCustomMapName)
                m_TopicCustomMapName.ClearState();
        }

        if (beh->IsInputActive(1)) {
            beh->ActivateInput(1, FALSE);
            beh->ActivateOutput(1);

            XObjectArray *oarray = *(XObjectArray **)beh->GetOutputParameterWriteDataPtr(0);
            ctx->DestroyObjects(oarray->Begin(), oarray->Size());
            oarray->Clear();
        }

        beh->GetOutputParameter(0)->DataChanged();
        return CKBR_OK;
    }

    // -------------------------------------------------------------------------
    // IMC event publishing
    // -------------------------------------------------------------------------

    bool PublishObjectLoadEvent(const char *filename, const char *mastername,
                                CK_CLASSID cid, CKBOOL addtoscene,
                                CKBOOL reuseMeshes, CKBOOL reuseMaterials,
                                CKBOOL dynamic, CKObject *masterobject,
                                CKBOOL isMap, const CK_ID *objectIds,
                                uint32_t objectCount) {
        if (!m_TopicLoadObject)
            return false;

        const size_t filenameSize = filename ? std::strlen(filename) + 1 : 0;
        const size_t masterNameSize = mastername ? std::strlen(mastername) + 1 : 0;
        const size_t objectIdsSize = static_cast<size_t>(objectCount) * sizeof(CK_ID);
        const size_t totalSize = sizeof(BML_ObjectLoadEvent) + filenameSize + masterNameSize + objectIdsSize;
        auto *storage = static_cast<char *>(std::malloc(totalSize));
        if (!storage) return false;

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

        auto zcBuf = bml::imc::ZeroCopyBuffer::Create(event, totalSize,
            [](const void *, size_t, void *ud) { std::free(ud); }, storage);
        return m_TopicLoadObject.PublishBuffer(zcBuf);
    }

    bool PublishScriptLoadEvent(const char *filename, CKBehavior *script, CKBOOL isMap) {
        if (!m_TopicLoadScript || !script)
            return false;

        const size_t filenameSize = filename ? std::strlen(filename) + 1 : 0;
        const size_t totalSize = sizeof(BML_ScriptLoadEvent) + filenameSize;
        auto *storage = static_cast<char *>(std::malloc(totalSize));
        if (!storage) return false;

        auto *event = reinterpret_cast<BML_ScriptLoadEvent *>(storage);
        std::memset(event, 0, sizeof(BML_ScriptLoadEvent));
        if (filenameSize > 0) {
            event->filename = storage + sizeof(BML_ScriptLoadEvent);
            std::memcpy(const_cast<char *>(event->filename), filename, filenameSize);
        }
        event->script = script;
        event->is_map = isMap;

        auto zcBuf = bml::imc::ZeroCopyBuffer::Create(event, totalSize,
            [](const void *, size_t, void *ud) { std::free(ud); }, storage);
        return m_TopicLoadScript.PublishBuffer(zcBuf);
    }

    // -------------------------------------------------------------------------
    // Initial snapshot
    // -------------------------------------------------------------------------

    void PublishInitialSnapshot(CKContext *context) {
        if (!context || m_SnapshotPublished) return;

        const int count = context->GetObjectsCountByClassID(CKCID_BEHAVIOR);
        CK_ID *ids = context->GetObjectsListByClassID(CKCID_BEHAVIOR);
        if (count <= 0 || !ids) return;

        if (m_TopicLoadObject) {
            PublishObjectLoadEvent(kBaseFilename, "", CKCID_3DOBJECT,
                                   TRUE, TRUE, TRUE, FALSE,
                                   nullptr, FALSE, nullptr, 0);
        }

        int published = 0;
        for (int i = 0; i < count; ++i) {
            CKObject *obj = context->GetObject(ids[i]);
            if (IsScriptBehavior(obj)) {
                if (PublishScriptLoadEvent(kBaseFilename, static_cast<CKBehavior *>(obj), FALSE))
                    ++published;
            }
        }

        if (published == 0 && !m_TopicLoadObject) return;

        m_SnapshotPublished = true;
        Services().Log().Info("Published initial load snapshot for %d scripts", published);
    }

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    static bool IsScriptBehavior(CKObject *object) {
        if (!object || object->GetClassID() != CKCID_BEHAVIOR) return false;
        return (static_cast<CKBehavior *>(object)->GetType() & CKBEHAVIORTYPE_SCRIPT) != 0;
    }
};

BML_DEFINE_MODULE(ObjectLoadMod)
