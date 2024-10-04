#ifndef BML_VTABLES_H
#define BML_VTABLES_H

#include "CKBaseManager.h"
#include "CKInputManager.h"

#include "CKObject.h"
#include "CKRenderContext.h"

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
struct CP_CLASS_VTABLE_NAME(CKInputManager) : public CP_CLASS_VTABLE_NAME(CKBaseManager)<CKInputManager> {
    CP_DECLARE_METHOD_PTR(T, void, EnableKeyboardRepetition, (CKBOOL iEnable));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsKeyboardRepetitionEnabled, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsKeyDown, (CKDWORD iKey, CKDWORD *oStamp));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsKeyUp, (CKDWORD iKey));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsKeyToggled ,(CKDWORD iKey, CKDWORD *oStamp));
    CP_DECLARE_METHOD_PTR(T, void, GetKeyName, (CKDWORD iKey, CKSTRING oKeyName));
    CP_DECLARE_METHOD_PTR(T, CKDWORD, GetKeyFromName, (CKSTRING iKeyName));
    CP_DECLARE_METHOD_PTR(T, unsigned char *, GetKeyboardState, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsKeyboardAttached, ());
    CP_DECLARE_METHOD_PTR(T, int, GetNumberOfKeyInBuffer, ());
    CP_DECLARE_METHOD_PTR(T, int, GetKeyFromBuffer, (int i, CKDWORD &oKey, CKDWORD *oTimeStamp));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsMouseButtonDown, (CK_MOUSEBUTTON iButton));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsMouseClicked, (CK_MOUSEBUTTON iButton));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsMouseToggled, (CK_MOUSEBUTTON iButton));
    CP_DECLARE_METHOD_PTR(T, void, GetMouseButtonsState, (CKBYTE oStates[4]));
    CP_DECLARE_METHOD_PTR(T, void, GetMousePosition, (Vx2DVector &oPosition, CKBOOL iAbsolute));
    CP_DECLARE_METHOD_PTR(T, void, GetMouseRelativePosition, (VxVector &oPosition));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsMouseAttached, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsJoystickAttached, (int iJoystick));
    CP_DECLARE_METHOD_PTR(T, void, GetJoystickPosition, (int iJoystick, VxVector *oPosition));
    CP_DECLARE_METHOD_PTR(T, void, GetJoystickRotation, (int iJoystick, VxVector *oRotation));
    CP_DECLARE_METHOD_PTR(T, void, GetJoystickSliders, (int iJoystick, Vx2DVector *oPosition));
    CP_DECLARE_METHOD_PTR(T, void, GetJoystickPointOfViewAngle, (int iJoystick, float *oAngle));
    CP_DECLARE_METHOD_PTR(T, CKDWORD, GetJoystickButtonsState, (int iJoystick));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsJoystickButtonDown, (int iJoystick, int iButton));
    CP_DECLARE_METHOD_PTR(T, void, Pause, (CKBOOL pause));
    CP_DECLARE_METHOD_PTR(T, void, ShowCursor, (CKBOOL iShow));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, GetCursorVisibility, ());
    CP_DECLARE_METHOD_PTR(T, VXCURSOR_POINTER, GetSystemCursor, ());
    CP_DECLARE_METHOD_PTR(T, void, SetSystemCursor, (VXCURSOR_POINTER cursor));
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

template <class T>
struct CP_CLASS_VTABLE_NAME(CKRenderContext) : public CP_CLASS_VTABLE_NAME(CKObject)<CKRenderContext> {
    CP_DECLARE_METHOD_PTR(T, void, AddObject, (CKRenderObject *obj));
    CP_DECLARE_METHOD_PTR(T, void, AddObjectWithHierarchy, (CKRenderObject *obj));
    CP_DECLARE_METHOD_PTR(T, void, RemoveObject, (CKRenderObject *obj));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsObjectAttached, (CKRenderObject *obj));
    CP_DECLARE_METHOD_PTR(T, const XObjectArray &, Compute3dRootObjects, ());
    CP_DECLARE_METHOD_PTR(T, const XObjectArray &, Compute2dRootObjects, ());
    CP_DECLARE_METHOD_PTR(T, CK2dEntity *, Get2dRoot, (CKBOOL background));
    CP_DECLARE_METHOD_PTR(T, void, DetachAll, ());
    CP_DECLARE_METHOD_PTR(T, void, ForceCameraSettingsUpdate, ());
    CP_DECLARE_METHOD_PTR(T, void, PrepareCameras, (CK_RENDER_FLAGS Flags));
    CP_DECLARE_METHOD_PTR(T, CKERROR, Clear, (CK_RENDER_FLAGS Flags, CKDWORD Stencil));
    CP_DECLARE_METHOD_PTR(T, CKERROR, DrawScene, (CK_RENDER_FLAGS Flags));
    CP_DECLARE_METHOD_PTR(T, CKERROR, BackToFront, (CK_RENDER_FLAGS Flags));
    CP_DECLARE_METHOD_PTR(T, CKERROR, Render, (CK_RENDER_FLAGS Flags));
    CP_DECLARE_METHOD_PTR(T, void, AddPreRenderCallBack, (CK_RENDERCALLBACK Function, void *Argument, CKBOOL Temporary));
    CP_DECLARE_METHOD_PTR(T, void, RemovePreRenderCallBack, (CK_RENDERCALLBACK Function, void *Argument));
    CP_DECLARE_METHOD_PTR(T, void, AddPostRenderCallBack, (CK_RENDERCALLBACK Function, void *Argument, CKBOOL Temporary, CKBOOL BeforeTransparent));
    CP_DECLARE_METHOD_PTR(T, void, RemovePostRenderCallBack, (CK_RENDERCALLBACK Function, void *Argument));
    CP_DECLARE_METHOD_PTR(T, void, AddPostSpriteRenderCallBack, (CK_RENDERCALLBACK Function, void *Argument, CKBOOL Temporary));
    CP_DECLARE_METHOD_PTR(T, void, RemovePostSpriteRenderCallBack, (CK_RENDERCALLBACK Function, void *Argument));
    CP_DECLARE_METHOD_PTR(T, VxDrawPrimitiveData *, GetDrawPrimitiveStructure, (CKRST_DPFLAGS Flags, int VertexCount));
    CP_DECLARE_METHOD_PTR(T, CKWORD *, GetDrawPrimitiveIndices, (int IndicesCount));
    CP_DECLARE_METHOD_PTR(T, void, Transform, (VxVector *Dest, VxVector *Src, CK3dEntity *Ref));
    CP_DECLARE_METHOD_PTR(T, void, TransformVertices, (int VertexCount, VxTransformData *data, CK3dEntity *Ref));
    CP_DECLARE_METHOD_PTR(T, CKERROR, GoFullScreen, (int Width, int Height, int Bpp, int Driver, int RefreshRate));
    CP_DECLARE_METHOD_PTR(T, CKERROR, StopFullScreen, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsFullScreen, ());
    CP_DECLARE_METHOD_PTR(T, int, GetDriverIndex, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, ChangeDriver, (int NewDriver));
    CP_DECLARE_METHOD_PTR(T, WIN_HANDLE, GetWindowHandle, ());
    CP_DECLARE_METHOD_PTR(T, void, ScreenToClient, (Vx2DVector *ioPoint));
    CP_DECLARE_METHOD_PTR(T, void, ClientToScreen, (Vx2DVector *ioPoint));
    CP_DECLARE_METHOD_PTR(T, CKERROR, SetWindowRect, (VxRect &rect, CKDWORD Flags));
    CP_DECLARE_METHOD_PTR(T, void, GetWindowRect, (VxRect &rect, CKBOOL ScreenRelative));
    CP_DECLARE_METHOD_PTR(T, int, GetHeight, ());
    CP_DECLARE_METHOD_PTR(T, int, GetWidth, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, Resize, (int PosX, int PosY, int SizeX, int SizeY, CKDWORD Flags));
    CP_DECLARE_METHOD_PTR(T, void, SetViewRect, (VxRect &rect));
    CP_DECLARE_METHOD_PTR(T, void, GetViewRect, (VxRect &rect));
    CP_DECLARE_METHOD_PTR(T, VX_PIXELFORMAT, GetPixelFormat, (int *Bpp, int *Zbpp, int *StencilBpp));
    CP_DECLARE_METHOD_PTR(T, void, SetState, (VXRENDERSTATETYPE State, CKDWORD Value));
    CP_DECLARE_METHOD_PTR(T, CKDWORD, GetState, (VXRENDERSTATETYPE State));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, SetTexture, (CKTexture *tex, CKBOOL Clamped, int Stage));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, SetTextureStageState, (CKRST_TEXTURESTAGESTATETYPE State, CKDWORD Value, int Stage));
    CP_DECLARE_METHOD_PTR(T, CKRasterizerContext *, GetRasterizerContext, ());
    CP_DECLARE_METHOD_PTR(T, void, SetClearBackground, (CKBOOL ClearBack));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, GetClearBackground, ());
    CP_DECLARE_METHOD_PTR(T, void, SetClearZBuffer, (CKBOOL ClearZ));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, GetClearZBuffer, ());
    CP_DECLARE_METHOD_PTR(T, void, GetGlobalRenderMode, (VxShadeType *Shading, CKBOOL *Texture, CKBOOL *Wireframe));
    CP_DECLARE_METHOD_PTR(T, void, SetGlobalRenderMode, (VxShadeType Shading, CKBOOL Texture, CKBOOL Wireframe));
    CP_DECLARE_METHOD_PTR(T, void, SetCurrentRenderOptions, (CKDWORD flags));
    CP_DECLARE_METHOD_PTR(T, CKDWORD, GetCurrentRenderOptions, ());
    CP_DECLARE_METHOD_PTR(T, void, ChangeCurrentRenderOptions, (CKDWORD Add, CKDWORD Remove));
    CP_DECLARE_METHOD_PTR(T, void, SetCurrentExtents, (VxRect &extents));
    CP_DECLARE_METHOD_PTR(T, void, GetCurrentExtents, (VxRect &extents));
    CP_DECLARE_METHOD_PTR(T, void, SetAmbientLightRGB, (float R, float G, float B));
    CP_DECLARE_METHOD_PTR(T, void, SetAmbientLight, (CKDWORD Color));
    CP_DECLARE_METHOD_PTR(T, CKDWORD, GetAmbientLight, ());
    CP_DECLARE_METHOD_PTR(T, void, SetFogMode, (VXFOG_MODE Mode));
    CP_DECLARE_METHOD_PTR(T, void, SetFogStart, (float Start));
    CP_DECLARE_METHOD_PTR(T, void, SetFogEnd, (float End));
    CP_DECLARE_METHOD_PTR(T, void, SetFogDensity, (float Density));
    CP_DECLARE_METHOD_PTR(T, void, SetFogColor, (CKDWORD Color));
    CP_DECLARE_METHOD_PTR(T, VXFOG_MODE, GetFogMode, ());
    CP_DECLARE_METHOD_PTR(T, float, GetFogStart, ());
    CP_DECLARE_METHOD_PTR(T, float, GetFogEnd, ());
    CP_DECLARE_METHOD_PTR(T, float, GetFogDensity, ());
    CP_DECLARE_METHOD_PTR(T, CKDWORD, GetFogColor, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, DrawPrimitive, (VXPRIMITIVETYPE pType, CKWORD *indices, int indexcount, VxDrawPrimitiveData *data));
    CP_DECLARE_METHOD_PTR(T, void, SetWorldTransformationMatrix, (const VxMatrix &M));
    CP_DECLARE_METHOD_PTR(T, void, SetProjectionTransformationMatrix, (const VxMatrix &M));
    CP_DECLARE_METHOD_PTR(T, void, SetViewTransformationMatrix, (const VxMatrix &M));
    CP_DECLARE_METHOD_PTR(T, const VxMatrix &, GetWorldTransformationMatrix, ());
    CP_DECLARE_METHOD_PTR(T, const VxMatrix &, GetProjectionTransformationMatrix, ());
    CP_DECLARE_METHOD_PTR(T, const VxMatrix &, GetViewTransformationMatrix, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, SetUserClipPlane, (CKDWORD ClipPlaneIndex, const VxPlane &PlaneEquation));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, GetUserClipPlane, (CKDWORD ClipPlaneIndex, VxPlane &PlaneEquation));
    CP_DECLARE_METHOD_PTR(T, CKRenderObject *, Pick, (int x, int y, CKPICKRESULT *oRes, CKBOOL iIgnoreUnpickable));
    CP_DECLARE_METHOD_PTR(T, CKRenderObject *, PointPick, (CKPOINT pt, CKPICKRESULT *oRes, CKBOOL iIgnoreUnpickable));
    CP_DECLARE_METHOD_PTR(T, CKERROR,  RectPick, (const VxRect &r, XObjectPointerArray &oObjects, CKBOOL Intersect));
    CP_DECLARE_METHOD_PTR(T, void, AttachViewpointToCamera, (CKCamera *cam));
    CP_DECLARE_METHOD_PTR(T, void, DetachViewpointFromCamera, ());
    CP_DECLARE_METHOD_PTR(T, CKCamera *, GetAttachedCamera, ());
    CP_DECLARE_METHOD_PTR(T, CK3dEntity *, GetViewpoint, ());
    CP_DECLARE_METHOD_PTR(T, CKMaterial *, GetBackgroundMaterial, ());
    CP_DECLARE_METHOD_PTR(T, void, GetBoundingBox, (VxBbox *BBox));
    CP_DECLARE_METHOD_PTR(T, void, GetStats, (VxStats *stats));
    CP_DECLARE_METHOD_PTR(T, void, SetCurrentMaterial, (CKMaterial *mat, CKBOOL Lit));
    CP_DECLARE_METHOD_PTR(T, void, Activate, (CKBOOL active));
    CP_DECLARE_METHOD_PTR(T, int, DumpToMemory, (const VxRect *iRect, VXBUFFER_TYPE buffer, VxImageDescEx &desc));
    CP_DECLARE_METHOD_PTR(T, int, CopyToVideo, (const VxRect *iRect, VXBUFFER_TYPE buffer, VxImageDescEx &desc));
    CP_DECLARE_METHOD_PTR(T, CKERROR, DumpToFile, (CKSTRING filename, const VxRect *rect, VXBUFFER_TYPE buffer));
    CP_DECLARE_METHOD_PTR(T, VxDirectXData *, GetDirectXInfo, ());
    CP_DECLARE_METHOD_PTR(T, void, WarnEnterThread, ());
    CP_DECLARE_METHOD_PTR(T, void, WarnExitThread, ());
    CP_DECLARE_METHOD_PTR(T, CK2dEntity *, Pick2D, (const Vx2DVector &v));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, SetRenderTarget, (CKTexture *texture, int CubeMapFace));
    CP_DECLARE_METHOD_PTR(T, void, AddRemoveSequence, (CKBOOL Start));
    CP_DECLARE_METHOD_PTR(T, void, SetTransparentMode, (CKBOOL Trans));
    CP_DECLARE_METHOD_PTR(T, void, AddDirtyRect, (CKRECT *Rect));
    CP_DECLARE_METHOD_PTR(T, void, RestoreScreenBackup, ());
    CP_DECLARE_METHOD_PTR(T, CKDWORD, GetStencilFreeMask, ());
    CP_DECLARE_METHOD_PTR(T, void, UsedStencilBits, (CKDWORD stencilBits));
    CP_DECLARE_METHOD_PTR(T, int, GetFirstFreeStencilBits, ());
    CP_DECLARE_METHOD_PTR(T, VxDrawPrimitiveData *, LockCurrentVB, (CKDWORD VertexCount));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, ReleaseCurrentVB, ());
    CP_DECLARE_METHOD_PTR(T, void, SetTextureMatrix, (const VxMatrix &M, int Stage));
    CP_DECLARE_METHOD_PTR(T, void, SetStereoParameters, (float EyeSeparation, float FocalLength));
    CP_DECLARE_METHOD_PTR(T, void, GetStereoParameters, (float &EyeSeparation, float &FocalLength));
};

#endif // BML_VTABLES_H
