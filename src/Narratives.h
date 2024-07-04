#ifndef BML_NARRATIVES_H
#define BML_NARRATIVES_H

#include "CKAll.h"

namespace Narratives {
    // Scene Management
    CKERROR LaunchScene(CKScene *scene,
                        CK_SCENEOBJECTACTIVITY_FLAGS activityFlags = CK_SCENEOBJECTACTIVITY_SCENEDEFAULT,
                        CK_SCENEOBJECTRESET_FLAGS resetFlags = CK_SCENEOBJECTRESET_SCENEDEFAULT);
    CKERROR AddToScene(CKSceneObject *sceneObject, CKScene *scene = nullptr,
                       CKBOOL copyActivation = FALSE, CK_SCENEOBJECTACTIVITY_FLAGS activityFlags = CK_SCENEOBJECTACTIVITY_SCENEDEFAULT);
    CKERROR RemoveFromScene(CKSceneObject *sceneObject, CKScene *scene = nullptr);

    CKERROR SaveIC(CKBeObject *beo);
    CKERROR RestoreIC(CKBeObject *beo);

    // Object Management
    CKERROR ObjectCreator(CKContext *context, CKObject **pObject, CK_CLASSID classId, const char *name, CKBOOL dynamic = TRUE);
    CKERROR ObjectLoader(CKContext *context, XObjectArray &objects, CKObject **pMasterObject,
                         const char *file, const char *masterObjectName, CK_CLASSID filterClass = CKCID_3DOBJECT,
                         CKBOOL addToScene = true, CKBOOL reuseMeshes = true,
                         CKBOOL reuseMaterials = true, CKBOOL dynamic = true);
    CKERROR ObjectCopier(CKContext *context, XObjectArray &srcObjects, XObjectArray &destObjects,
                         CKDependencies *dependencies = nullptr, CKBOOL dynamic = TRUE, CKBOOL awakeObject = TRUE);
    CKERROR ObjectDeleter(CKContext *context, XObjectArray &objects, CKDWORD flags = 0, CKDependencies *dependencies = nullptr);

    CKERROR TextureLoader(CKTexture *texture, const char *filename, int slot = 0);
    CKERROR SpriteLoader(CKSprite *sprite, const char *filename, int slot = 0);
    CKERROR SoundLoader(CKWaveSound *sound, const char *filename, CKBOOL streamed = FALSE);

    // Scripts Management
    CKERROR ActivateObject(CKBeObject *beo, CKBOOL reset = FALSE, CKBOOL activeAllScripts = FALSE, CKBOOL resetScripts = FALSE);
    CKERROR ActivateScript(CKBehavior *script, CKBOOL reset = TRUE, CKBOOL awakeObject = TRUE);
    CKERROR ActivateScripts(CKContext *context, XObjectArray &scripts, CKBOOL reset = TRUE, CKBOOL awakeObject = TRUE);
    CKERROR DeactivateObject(CKBeObject *beo);
    CKERROR DeactivateScript(CKBehavior *script);
    CKERROR ExecuteScript(CKBehavior *script, CKBOOL waitForCompletion = FALSE);
    CKERROR AttachScript(CKBeObject *beo, CKBehavior *script, CKBehavior **pNewScript, CKBOOL dynamic = TRUE, CKBOOL activate = TRUE, CKBOOL reset = TRUE);
    CKERROR DetachScript(CKBeObject *beo, CKBehavior *script, CKBOOL destroy = TRUE);
}

#endif // BML_NARRATIVES_H
