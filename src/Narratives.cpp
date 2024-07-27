#include "Narratives.h"

CKERROR Narratives::LaunchScene(CKScene *scene, CK_SCENEOBJECTACTIVITY_FLAGS activityFlags, CK_SCENEOBJECTRESET_FLAGS resetFlags) {
    if (!scene)
        return CKERR_INVALIDPARAMETER;

    CKContext *context = scene->GetCKContext();
    if (!context)
        return CKERR_NOTFOUND;

    CKLevel *level = context->GetCurrentLevel();
    if (!level)
        return CKERR_NOCURRENTLEVEL;

    if (scene != level->GetCurrentScene() || (resetFlags == CK_SCENEOBJECTRESET_RESET))
        level->SetNextActiveScene(scene, activityFlags, resetFlags);

    return CK_OK;
}

CKERROR Narratives::AddToScene(CKSceneObject *sceneObject, CKScene *scene, CKBOOL copyActivation, CK_SCENEOBJECTACTIVITY_FLAGS activityFlags) {
    if (!sceneObject)
        return CKERR_INVALIDPARAMETER;

    CKContext *context = sceneObject->GetCKContext();
    if (!context)
        return CKERR_NOTFOUND;

    CKScene *currentScene = context->GetCurrentScene();
    if (!currentScene)
        return CKERR_NOTFOUND;

    if (!scene)
        scene = currentScene;

    if (copyActivation && (scene != currentScene)) {
        // we shall copy the activation flags of the current scene
        // to the other scene for the object and its scripts

        // first the beobject
        CK_SCENEOBJECT_FLAGS oflags = currentScene->GetObjectFlags(sceneObject);
        scene->SetObjectFlags(sceneObject, oflags);

        // then the scripts
        CKBeObject *beo = CKBeObject::Cast(sceneObject);
        if (beo) {
            int scount = beo->GetScriptCount();
            for (int i = 0; i < scount; ++i) {
                CKBehavior *script = beo->GetScript(i);

                CK_SCENEOBJECT_FLAGS sflags = currentScene->GetObjectFlags(script);
                scene->SetObjectFlags(script, sflags);
            }
        }
    } else {
        if (activityFlags != CK_SCENEOBJECTACTIVITY_SCENEDEFAULT) {
            CKDWORD oflags = scene->GetObjectFlags(sceneObject);
            oflags &= ~(CK_SCENEOBJECT_START_ACTIVATE | CK_SCENEOBJECT_START_DEACTIVATE | CK_SCENEOBJECT_START_LEAVE); // clear all traces of activity

            switch (activityFlags) {
                case CK_SCENEOBJECTACTIVITY_SCENEDEFAULT:
                    oflags |= CK_SCENEOBJECT_START_ACTIVATE;
                    break;
                case CK_SCENEOBJECTACTIVITY_ACTIVATE:
                    oflags |= CK_SCENEOBJECT_START_ACTIVATE;
                    break;
                case CK_SCENEOBJECTACTIVITY_DEACTIVATE:
                    oflags |= CK_SCENEOBJECT_START_DEACTIVATE;
                    break;
                case CK_SCENEOBJECTACTIVITY_DONOTHING:
                    oflags |= CK_SCENEOBJECT_START_LEAVE;
                    break;
            }

            scene->SetObjectFlags(sceneObject, (CK_SCENEOBJECT_FLAGS) oflags);
        }
    }

    return CK_OK;
}

CKERROR Narratives::RemoveFromScene(CKSceneObject *sceneObject, CKScene *scene) {
    if (!sceneObject)
        return CKERR_INVALIDPARAMETER;

    CKContext *context = sceneObject->GetCKContext();
    if (!context)
        return CKERR_NOTFOUND;

    if (!scene) {
        scene = context->GetCurrentScene();
        if (!scene)
            return CKERR_NOTFOUND;
    }

    scene->RemoveObjectFromScene(sceneObject);

    return CK_OK;
}

CKERROR Narratives::SaveIC(CKBeObject *beo) {
    if (!beo)
        return CKERR_INVALIDPARAMETER;

    CKContext *context = beo->GetCKContext();
    if (!context)
        return CKERR_NOTFOUND;

    CKScene *scene = context->GetCurrentScene();
    if (!scene)
        return CKERR_NOTFOUND;

    if (beo->IsInScene(scene)) {
        CKStateChunk *chunk = CKSaveObjectState(beo);
        scene->SetObjectInitialValue(beo, chunk);
    }

    return CK_OK;
}

CKERROR Narratives::RestoreIC(CKBeObject *beo) {
    if (!beo)
        return CKERR_INVALIDPARAMETER;

    CKContext *context = beo->GetCKContext();
    if (!context)
        return CKERR_NOTFOUND;

    CKScene *scene = context->GetCurrentScene();
    if (!scene)
        return CKERR_NOTFOUND;

    if (beo->IsInScene(scene)) {
        CKStateChunk *chunk = scene->GetObjectInitialValue(beo);
        if (chunk)
            CKReadObjectState(beo, chunk);
    }

    return CK_OK;
}

CKERROR Narratives::ObjectCreator(CKContext *context, CKObject **pObject, CK_CLASSID classId, const char *name, CKBOOL dynamic) {
    if (!context)
        return CKERR_INVALIDPARAMETER;

    if (CKIsChildClassOf(classId, CKCID_LEVEL))
        return CKERR_INVALIDOPERATION;

    CK_OBJECTCREATION_OPTIONS options = CK_OBJECTCREATION_NONAMECHECK;
    if (dynamic)
        options = CK_OBJECTCREATION_DYNAMIC;

    // The Creation
    CKObject *obj = context->CreateObject(classId, const_cast<CKSTRING>(name), options);
    if (!obj)
        return CKERR_OUTOFMEMORY;

    CKLevel *level = context->GetCurrentLevel();
    if (level) {
        if (CKIsChildClassOf(classId, CKCID_SCENE)) {
            level->AddScene((CKScene *) obj);
        } else {
            level->AddObject(obj);
        }
    }

    *pObject = obj;
    return CK_OK;
}

CKERROR Narratives::ObjectLoader(CKContext *context, XObjectArray &objects, CKObject **pMasterObject, const char *filename, const char *masterObjectName,
                                 CK_CLASSID filterClass, CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic) {
    if (!context)
        return CKERR_INVALIDPARAMETER;

    if (!filename || filename[0] == '\0')
        return CKERR_INVALIDPARAMETER;

    CKLevel *level = context->GetCurrentLevel();
    CKScene *scene = context->GetCurrentScene();
    if (level && level->GetLevelScene() == scene)
        addToScene = FALSE;

    auto loadOptions = CK_LOAD_FLAGS(CK_LOAD_DEFAULT | CK_LOAD_AUTOMATICMODE);
    if (dynamic)
        loadOptions = (CK_LOAD_FLAGS) (loadOptions | CK_LOAD_AS_DYNAMIC_OBJECT);

    CKObjectArray *array = CreateCKObjectArray();

    context->SetAutomaticLoadMode(CKLOAD_OK, CKLOAD_OK, reuseMeshes ? CKLOAD_USECURRENT : CKLOAD_OK, reuseMaterials ? CKLOAD_USECURRENT : CKLOAD_OK);

    XString fileName(filename);
    context->GetPathManager()->ResolveFileName(fileName, DATA_PATH_IDX, -1);
    CKERROR err = context->Load(fileName.Str(), array, loadOptions);
    if (err != CK_OK)
        return err;

    CKObject *masterObject = nullptr;
    CKLevel *loadedLevel = nullptr;

    objects.Clear();
    for (array->Reset(); !array->EndOfList(); array->Next()) {
        CKObject *o = array->GetData(context);
        if (CKIsChildClassOf(o, CKCID_LEVEL)) {
            loadedLevel = (CKLevel *) o;
        }

        // We search here for the master object
        if (CKIsChildClassOf(o, filterClass)) {
            if (masterObjectName && masterObjectName[0] != '\0') {
                char *objectName = o->GetName();
                if (objectName && !strcmp(objectName, masterObjectName)) {
                    masterObject = o;
                }
            } else {
                if (CKIsChildClassOf(o, CKCID_3DENTITY)) {
                    if (!((CK3dEntity *) o)->GetParent())
                        masterObject = o;
                } else if (CKIsChildClassOf(o, CKCID_2DENTITY)) {
                    if (!((CK2dEntity *) o)->GetParent())
                        masterObject = o;
                }
            }
        }

        objects.PushBack(o->GetID());
    }

    if (level) {
        if (loadedLevel) {
            // If a level is loaded, do a merge
            level->Merge(loadedLevel, FALSE);
            objects.RemoveObject(loadedLevel);
            context->DestroyObject(loadedLevel);
        } else {
            // else add everything to the level / scene
            level->BeginAddSequence(TRUE);

            for (array->Reset(); !array->EndOfList(); array->Next()) {
                CKObject *o = array->GetData(context);
                if (CKIsChildClassOf(o, CKCID_SCENE)) {
                    level->AddScene((CKScene *) o);
                } else {
                    level->AddObject(o);
                }

                if (addToScene && CKIsChildClassOf(o, CKCID_SCENEOBJECT) && !(CKIsChildClassOf(o, CKCID_LEVEL) || CKIsChildClassOf(o, CKCID_SCENE)))
                    scene->AddObjectToScene((CKSceneObject *) o);
            }

            level->BeginAddSequence(FALSE);
        }
    }

    DeleteCKObjectArray(array);
    if (pMasterObject)
        *pMasterObject = masterObject;

    return CK_OK;
}

CKERROR Narratives::ObjectCopier(CKContext *context, XObjectArray &srcObjects, XObjectArray &destObjects, CKDependencies *dependencies, CKBOOL dynamic,
                                 CKBOOL awakeObject) {
    if (!context)
        return CKERR_INVALIDPARAMETER;

    if (srcObjects.IsEmpty())
        return CK_OK;

    for (auto *it = srcObjects.Begin(); it != srcObjects.End();) {
        CKObject *obj = srcObjects.GetObject(context, *it);
        if (CKIsChildClassOf(obj, CKCID_LEVEL)) {
            it = srcObjects.Remove(it);
        } else {
            ++it;
        }
    }

    CKDWORD options = 0;
    if (awakeObject)
        options |= CK_OBJECTCREATION_ACTIVATE;
    if (dynamic)
        options |= CK_OBJECTCREATION_DYNAMIC;

    destObjects = context->CopyObjects(srcObjects, dependencies, (CK_OBJECTCREATION_OPTIONS) options);

    return CK_OK;
}

CKERROR Narratives::ObjectDeleter(CKContext *context, XObjectArray &objects, CKDWORD flags, CKDependencies *dependencies) {
    if (!context)
        return CKERR_INVALIDPARAMETER;

    CKERROR err = context->DestroyObjects(objects.Begin(), objects.Size(), flags, dependencies);
    if (err != CK_OK)
        return err;

    objects.Clear();
    return CK_OK;
}

CKERROR Narratives::TextureLoader(CKTexture *texture, const char *filename, int slot) {
    if (!texture)
        return CKERR_INVALIDPARAMETER;

    if (!filename || filename[0] == '\0')
        return CKERR_INVALIDPARAMETER;

    CKContext *context = texture->GetCKContext();
    if (!context)
        return CKERR_NOTFOUND;

    XString fileName(filename);
    context->GetPathManager()->ResolveFileName(fileName, BITMAP_PATH_IDX);

    CKERROR err = texture->LoadImage(fileName.Str(), slot);
    if (err != CK_OK)
        return CKERR_INVALIDFILE;

    return CK_OK;
}

CKERROR Narratives::SpriteLoader(CKSprite *sprite, const char *filename, int slot) {
    if (!sprite)
        return CKERR_INVALIDPARAMETER;

    if (!filename || filename[0] == '\0')
        return CKERR_INVALIDPARAMETER;

    CKContext *context = sprite->GetCKContext();
    if (!context)
        return CKERR_NOTFOUND;

    XString fileName(filename);
    context->GetPathManager()->ResolveFileName(fileName, BITMAP_PATH_IDX);

    if (sprite->LoadImage(fileName.Str(), slot) != CK_OK)
        return CKERR_INVALIDFILE;

    return CK_OK;
}

CKERROR Narratives::SoundLoader(CKWaveSound *sound, const char *filename, CKBOOL streamed) {
    if (!sound)
        return CKERR_INVALIDPARAMETER;

    if (!filename || filename[0] == '\0')
        return CKERR_INVALIDPARAMETER;

    CKContext *context = sound->GetCKContext();
    if (!context)
        return CKERR_NOTFOUND;

    XString fileName(filename);
    context->GetPathManager()->ResolveFileName(fileName, SOUND_PATH_IDX);

    sound->Stop();
    if (sound->SetSoundFileName(fileName.Str()) != CK_OK)
        return CKERR_INVALIDFILE;

    sound->SetFileStreaming(streamed, TRUE);

    return CK_OK;
}


CKERROR Narratives::ActivateObject(CKBeObject *beo, CKBOOL reset, CKBOOL activeAllScripts, CKBOOL resetScripts) {
    if (!beo)
        return CKERR_INVALIDPARAMETER;

    CKContext *context = beo->GetCKContext();
    if (!context)
        return CKERR_NOTFOUND;

    CKScene *scene = context->GetCurrentScene();
    if (!scene)
        return CKERR_NOTFOUND;

    scene->Activate(beo, reset);

    if (activeAllScripts) {
        for (int i = 0; i < beo->GetScriptCount(); ++i) {
            CKBehavior *script = beo->GetScript(i);
            scene->Activate(script, resetScripts);
        }
    }

    return CK_OK;
}

CKERROR Narratives::ActivateScript(CKBehavior *script, CKBOOL reset, CKBOOL awakeObject) {
    if (!script)
        return CKERR_INVALIDPARAMETER;

    CKContext *context = script->GetCKContext();
    if (!context)
        return CKERR_NOTFOUND;

    CKScene *scene = context->GetCurrentScene();
    if (!scene)
        return CKERR_NOTFOUND;

    scene->Activate(script, reset);

    if (awakeObject) {
        CKBeObject *beo = script->GetOwner();
        if (!beo->IsActiveInCurrentScene())
            scene->Activate(beo, FALSE);
    }

    return CK_OK;
}

CKERROR Narratives::ActivateScripts(CKContext *context, XObjectArray &scripts, CKBOOL reset, CKBOOL awakeObject) {
    if (!context)
        return CKERR_INVALIDPARAMETER;

    if (scripts.IsEmpty())
        return CK_OK;

    CKScene *scene = context->GetCurrentScene();
    if (!scene)
        return CKERR_INVALIDPARAMETER;

    for (auto it = scripts.Begin(); it != scripts.End(); ++it) {
        CKObject *obj = context->GetObject(*it);
        if (CKIsChildClassOf(obj, CKCID_BEHAVIOR)) {
            auto *script = (CKBehavior *) obj;
            scene->Activate(script, reset);
            if (awakeObject) {
                CKBeObject *beo = script->GetOwner();
                if (!beo->IsActiveInCurrentScene())
                    scene->Activate(beo, FALSE);
            }
        }
    }

    return CK_OK;
}

CKERROR Narratives::DeactivateObject(CKBeObject *beo) {
    if (!beo)
        return CKERR_INVALIDPARAMETER;

    CKContext *context = beo->GetCKContext();
    if (!context)
        return CKERR_NOTFOUND;

    CKScene *scene = context->GetCurrentScene();
    if (!scene)
        return CKERR_NOTFOUND;

    scene->DeActivate(beo);
    return CK_OK;
}

CKERROR Narratives::DeactivateScript(CKBehavior *script) {
    if (!script)
        return CKERR_INVALIDPARAMETER;

    CKContext *context = script->GetCKContext();
    if (!context)
        return CKERR_NOTFOUND;

    CKScene *scene = context->GetCurrentScene();
    if (!scene)
        return CKERR_NOTFOUND;

    scene->DeActivate(script);
    return CK_OK;
}

CKERROR Narratives::ExecuteScript(CKBehavior *script, CKBOOL waitForCompletion) {
    if (!script)
        return CKERR_INVALIDPARAMETER;

    CKContext *context = script->GetCKContext();
    if (!context)
        return CKERR_NOTFOUND;

    float delta = context->GetTimeManager()->GetLastDeltaTime();

    if (waitForCompletion) {
        int maxLoop = context->GetBehaviorManager()->GetBehaviorMaxIteration();
        for (int loop = 0; true; ++loop) {
            if (loop > maxLoop) {
                context->OutputToConsoleExBeep((CKSTRING) "Execute Script : Script %s Executed too much times", script->GetName());
                script->Activate(FALSE, FALSE);
                break;
            }
            CKERROR result = script->Execute(delta);
            // The script loop on itself too much times
            if (result == CKBR_INFINITELOOP) {
                context->OutputToConsoleExBeep((CKSTRING) "Execute Script : Script %s Executed too much times", script->GetName());
                script->Activate(FALSE, FALSE);
                break;
            }
            if (!script->IsActive())
                break;
        }
    } else {
        script->Execute(delta);
        script->Activate(FALSE, FALSE);
    }

    return CK_OK;
}

CKERROR Narratives::AttachScript(CKBeObject *beo, CKBehavior *script, CKBehavior **pNewScript, CKBOOL dynamic, CKBOOL activate, CKBOOL reset) {
    if (!beo || !script)
        return CKERR_INVALIDPARAMETER;

    CKContext *context = beo->GetCKContext();
    if (!context)
        return CKERR_NOTFOUND;

    CKScene *scene = context->GetCurrentScene();
    if (!scene)
        return CKERR_NOTFOUND;

    CKDependenciesContext depContext(context);
    if (dynamic)
        depContext.SetCreationMode(CK_OBJECTCREATION_DYNAMIC);
    depContext.StartDependencies(nullptr);
    CK_ID id = script->GetID();
    depContext.AddObjects(&id, 1);
    depContext.Copy();

    auto *newScript = (CKBehavior *)depContext.GetObjects(0);
    *pNewScript = newScript;

    CKBeObject *oldBeo = newScript->GetOwner();
    if (oldBeo)
        oldBeo->RemoveScript(newScript->GetID());

    beo->AddScript(newScript);

    if (activate) {
        scene->Activate(newScript, reset);
        if (!beo->IsActiveInCurrentScene()) {
            scene->Activate(beo, FALSE);
        }
    }

    return CK_OK;
}

CKERROR Narratives::DetachScript(CKBeObject *beo, CKBehavior *script, CKBOOL destroy) {
    if (!beo || !script)
        return CKERR_INVALIDPARAMETER;

    CKContext *context = beo->GetCKContext();
    if (!context)
        return CKERR_NOTFOUND;

    beo->RemoveScript(script->GetID());

    CKScene *scene = context->GetCurrentScene();
    if (scene)
        scene->DeActivate(script);

    if (destroy) {
        context->DestroyObject(script);
    }

    return CK_OK;
}
