#ifndef BML_VTABLES_H
#define BML_VTABLES_H

#include "CKBaseManager.h"
#include "CKObject.h"

#include "Macros.h"

template <class T>
struct CP_CLASS_VTABLE_NAME(CKBaseManager) {
    CP_DECLARE_METHOD_PTR(T, void, Destructor, ());
    CP_DECLARE_METHOD_PTR(T, CKStateChunk *, SaveData, (CKFile *SavedFile));
    CP_DECLARE_METHOD_PTR(T, CKERROR, LoadData, (CKStateChunk *chunk, CKFile *LoadedFile));
    CP_DECLARE_METHOD_PTR(T, CKERROR, PreClearAll, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, PostClearAll, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, PreProcess, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, PostProcess, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, SequenceAddedToScene, (CKScene *scn, CK_ID *objids, int count));
    CP_DECLARE_METHOD_PTR(T, CKERROR, SequenceRemovedFromScene, (CKScene *scn, CK_ID *objids, int count));
    CP_DECLARE_METHOD_PTR(T, CKERROR, PreLaunchScene, (CKScene *OldScene, CKScene *NewScene));
    CP_DECLARE_METHOD_PTR(T, CKERROR, PostLaunchScene, (CKScene *OldScene, CKScene *NewScene));
    CP_DECLARE_METHOD_PTR(T, CKERROR, OnCKInit, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, OnCKEnd, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, OnCKReset, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, OnCKPostReset, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, OnCKPause, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, OnCKPlay, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, SequenceToBeDeleted, (CK_ID *objids, int count));
    CP_DECLARE_METHOD_PTR(T, CKERROR, SequenceDeleted, (CK_ID *objids, int count));
    CP_DECLARE_METHOD_PTR(T, CKERROR, PreLoad, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, PostLoad, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, PreSave, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, PostSave, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, OnPreCopy, (CKDependenciesContext &context));
    CP_DECLARE_METHOD_PTR(T, CKERROR, OnPostCopy, (CKDependenciesContext &context));
    CP_DECLARE_METHOD_PTR(T, CKERROR, OnPreRender, (CKRenderContext *dev));
    CP_DECLARE_METHOD_PTR(T, CKERROR, OnPostRender, (CKRenderContext *dev));
    CP_DECLARE_METHOD_PTR(T, CKERROR, OnPostSpriteRender, (CKRenderContext *dev));
    CP_DECLARE_METHOD_PTR(T, int, GetFunctionPriority, (CKMANAGER_FUNCTIONS Function));
    CP_DECLARE_METHOD_PTR(T, CKDWORD, GetValidFunctionsMask, ());
};

template <class T>
struct CP_CLASS_VTABLE_NAME(CKObject) {
    CP_DECLARE_METHOD_PTR(T, void, Show, (CK_OBJECT_SHOWOPTION show));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsHiddenByParent, ());
    CP_DECLARE_METHOD_PTR(T, int, CanBeHide, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsVisible, ());
    CP_DECLARE_METHOD_PTR(T, void, Destructor, ());
    CP_DECLARE_METHOD_PTR(T, CK_CLASSID, GetClassID, ());
    CP_DECLARE_METHOD_PTR(T, void, PreSave, (CKFile *file, CKDWORD flags));
    CP_DECLARE_METHOD_PTR(T, CKStateChunk *, Save, (CKFile *file, CKDWORD flags));
    CP_DECLARE_METHOD_PTR(T, CKERROR, Load, (CKStateChunk *chunk, CKFile *file));
    CP_DECLARE_METHOD_PTR(T, void, PostLoad, ());
    CP_DECLARE_METHOD_PTR(T, void, PreDelete, ());
    CP_DECLARE_METHOD_PTR(T, void, CheckPreDeletion, ());
    CP_DECLARE_METHOD_PTR(T, void, CheckPostDeletion, ());
    CP_DECLARE_METHOD_PTR(T, int, GetMemoryOccupation, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsObjectUsed, (CKObject *obj, CK_CLASSID cid));
    CP_DECLARE_METHOD_PTR(T, CKERROR, PrepareDependencies, (CKDependenciesContext &context, CKBOOL iCaller));
    CP_DECLARE_METHOD_PTR(T, CKERROR, RemapDependencies, (CKDependenciesContext &context));
    CP_DECLARE_METHOD_PTR(T, CKERROR, Copy, (CKObject &o, CKDependenciesContext &context));
};

#endif // BML_VTABLES_H
