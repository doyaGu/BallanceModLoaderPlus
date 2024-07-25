#ifndef BML_VTABLES_H
#define BML_VTABLES_H

#include "CKBaseManager.h"
#include "CKRenderManager.h"
#include "CKInputManager.h"

#include "CKObject.h"
#include "CKSceneObject.h"
#include "CKBeObject.h"
#include "CKRenderContext.h"
//#include "CKKinematicChain.h"
#include "CKMaterial.h"
#include "CKTexture.h"
#include "CKMesh.h"
#include "CKPatchMesh.h"
//#include "CKAnimation.h"
//#include "CKKeyedAnimation.h"
//#include "CKObjectAnimation.h"
//#include "CKLayer.h"
#include "CKRenderObject.h"
#include "CK2dEntity.h"
#include "CK3dEntity.h"
#include "CKCamera.h"
#include "CKLight.h"
//#include "CKCurvePoint.h"
//#include "CKCurve.h"
#include "CK3dObject.h"
#include "CKSprite3D.h"
//#include "CKCharacter.h"
#include "CKPlace.h"
#include "CKGrid.h"
//#include "CKBodyPart.h"
#include "CKTargetCamera.h"
#include "CKTargetLight.h"
#include "CKSprite.h"
#include "CKSpriteText.h"

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
struct CP_CLASS_VTABLE_NAME(CKRenderManager) : public CP_CLASS_VTABLE_NAME(CKBaseManager)<CKRenderManager> {
    CP_DECLARE_METHOD_PTR(T, int, GetRenderDriverCount, ());
    CP_DECLARE_METHOD_PTR(T, VxDriverDesc *, GetRenderDriverDescription, (int Driver));

    CP_DECLARE_METHOD_PTR(T, void, GetDesiredTexturesVideoFormat, (VxImageDescEx &VideoFormat));
    CP_DECLARE_METHOD_PTR(T, void, SetDesiredTexturesVideoFormat, (VxImageDescEx &VideoFormat));
    CP_DECLARE_METHOD_PTR(T, CKRenderContext *,GetRenderContext, (int pos));
    CP_DECLARE_METHOD_PTR(T, CKRenderContext *,GetRenderContextFromPoint, (CKPOINT &pt));
    CP_DECLARE_METHOD_PTR(T, int, GetRenderContextCount, ());
    CP_DECLARE_METHOD_PTR(T, void, Process, ());
    CP_DECLARE_METHOD_PTR(T, void, FlushTextures, ());
    CP_DECLARE_METHOD_PTR(T, CKRenderContext *, CreateRenderContext, (void *Window, int Driver, CKRECT *rect, CKBOOL Fullscreen, int Bpp, int Zbpp, int StencilBpp, int RefreshRate));
    CP_DECLARE_METHOD_PTR(T, CKERROR, DestroyRenderContext, (CKRenderContext *context));
    CP_DECLARE_METHOD_PTR(T, void, RemoveRenderContext, (CKRenderContext *context));
    CP_DECLARE_METHOD_PTR(T, CKVertexBuffer *,CreateVertexBuffer, ());
    CP_DECLARE_METHOD_PTR(T, void, DestroyVertexBuffer, (CKVertexBuffer *VB));
    CP_DECLARE_METHOD_PTR(T, void, SetRenderOptions, (CKSTRING RenderOptionString, CKDWORD Value));
    CP_DECLARE_METHOD_PTR(T, const VxEffectDescription &, GetEffectDescription, (int EffectIndex));
    CP_DECLARE_METHOD_PTR(T, int, GetEffectCount,());
    CP_DECLARE_METHOD_PTR(T, int, AddEffect, (const VxEffectDescription &NewEffect));
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
struct CP_CLASS_VTABLE_NAME(CKSceneObject) : public CP_CLASS_VTABLE_NAME(CKObject)<CKSceneObject> {
    CP_DECLARE_METHOD_PTR(T, void, AddToScene, (CKScene *scene, CKBOOL dependencies));
    CP_DECLARE_METHOD_PTR(T, void, RemoveFromScene, (CKScene *scene, CKBOOL dependencies));
};

template <class T>
struct CP_CLASS_VTABLE_NAME(CKBeObject) : public CP_CLASS_VTABLE_NAME(CKSceneObject)<CKBeObject> {
    CP_DECLARE_METHOD_PTR(T, void, ApplyPatchForOlderVersion, (int NbObject, CKFileObject *FileObjects));
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

template <class T>
struct CP_CLASS_VTABLE_NAME(CKMaterial) : public CP_CLASS_VTABLE_NAME(CKBeObject)<CKMaterial> {
    CP_DECLARE_METHOD_PTR(T, float, GetPower, ());
    CP_DECLARE_METHOD_PTR(T, void, SetPower, (float Value));
    CP_DECLARE_METHOD_PTR(T, const VxColor &, GetAmbient, ());
    CP_DECLARE_METHOD_PTR(T, void, SetAmbient, (const VxColor &Color));
    CP_DECLARE_METHOD_PTR(T, const VxColor &, GetDiffuse, ());
    CP_DECLARE_METHOD_PTR(T, void, SetDiffuse, (const VxColor &Color));
    CP_DECLARE_METHOD_PTR(T, const VxColor &, GetSpecular, ());
    CP_DECLARE_METHOD_PTR(T, void, SetSpecular, (const VxColor &Color));
    CP_DECLARE_METHOD_PTR(T, const VxColor &, GetEmissive, ());
    CP_DECLARE_METHOD_PTR(T, void, SetEmissive, (const VxColor &Color));
    CP_DECLARE_METHOD_PTR(T, CKTexture *, GetTexture, (int TexIndex));
    CP_DECLARE_METHOD_PTR(T, void, SetTexture, (int TexIndex, CKTexture *Tex));
    CP_DECLARE_METHOD_PTR(T, void, SetTexture0, (CKTexture *Tex));
    CP_DECLARE_METHOD_PTR(T, void, SetTextureBlendMode, (VXTEXTURE_BLENDMODE BlendMode));
    CP_DECLARE_METHOD_PTR(T, VXTEXTURE_BLENDMODE, GetTextureBlendMode, ());
    CP_DECLARE_METHOD_PTR(T, void, SetTextureMinMode, (VXTEXTURE_FILTERMODE FilterMode));
    CP_DECLARE_METHOD_PTR(T, VXTEXTURE_FILTERMODE, GetTextureMinMode, ());
    CP_DECLARE_METHOD_PTR(T, void, SetTextureMagMode, (VXTEXTURE_FILTERMODE FilterMode));
    CP_DECLARE_METHOD_PTR(T, VXTEXTURE_FILTERMODE, GetTextureMagMode, ());
    CP_DECLARE_METHOD_PTR(T, void, SetTextureAddressMode, (VXTEXTURE_ADDRESSMODE Mode));
    CP_DECLARE_METHOD_PTR(T, VXTEXTURE_ADDRESSMODE, GetTextureAddressMode, ());
    CP_DECLARE_METHOD_PTR(T, void, SetTextureBorderColor, (CKDWORD Color));
    CP_DECLARE_METHOD_PTR(T, CKDWORD, GetTextureBorderColor, ());
    CP_DECLARE_METHOD_PTR(T, void, SetSourceBlend, (VXBLEND_MODE BlendMode));
    CP_DECLARE_METHOD_PTR(T, void, SetDestBlend, (VXBLEND_MODE BlendMode));
    CP_DECLARE_METHOD_PTR(T, VXBLEND_MODE, GetSourceBlend, ());
    CP_DECLARE_METHOD_PTR(T, VXBLEND_MODE, GetDestBlend, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsTwoSided, ());
    CP_DECLARE_METHOD_PTR(T, void, SetTwoSided, (CKBOOL TwoSided));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, ZWriteEnabled, ());
    CP_DECLARE_METHOD_PTR(T, void, EnableZWrite, (CKBOOL ZWrite));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, AlphaBlendEnabled, ());
    CP_DECLARE_METHOD_PTR(T, void, EnableAlphaBlend, (CKBOOL Blend));
    CP_DECLARE_METHOD_PTR(T, VXCMPFUNC, GetZFunc, ());
    CP_DECLARE_METHOD_PTR(T, void, SetZFunc, (VXCMPFUNC ZFunc));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, PerspectiveCorrectionEnabled, ());
    CP_DECLARE_METHOD_PTR(T, void, EnablePerpectiveCorrection, (CKBOOL Perspective));
    CP_DECLARE_METHOD_PTR(T, void, SetFillMode, (VXFILL_MODE FillMode));
    CP_DECLARE_METHOD_PTR(T, VXFILL_MODE, GetFillMode, ());
    CP_DECLARE_METHOD_PTR(T, void, SetShadeMode, (VXSHADE_MODE ShadeMode));
    CP_DECLARE_METHOD_PTR(T, VXSHADE_MODE, GetShadeMode, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, SetAsCurrent, (CKRenderContext *, CKBOOL Lit, int TextureStage));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsAlphaTransparent, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, AlphaTestEnabled, ());
    CP_DECLARE_METHOD_PTR(T, void, EnableAlphaTest, (CKBOOL Enable));
    CP_DECLARE_METHOD_PTR(T, VXCMPFUNC, GetAlphaFunc, ());
    CP_DECLARE_METHOD_PTR(T, void, SetAlphaFunc, (VXCMPFUNC AlphaFunc));
    CP_DECLARE_METHOD_PTR(T, CKBYTE, GetAlphaRef, ());
    CP_DECLARE_METHOD_PTR(T, void, SetAlphaRef, (CKBYTE AlphaRef));
    CP_DECLARE_METHOD_PTR(T, void, SetCallback, (CK_MATERIALCALLBACK Fct, void *Argument));
    CP_DECLARE_METHOD_PTR(T, CK_MATERIALCALLBACK, GetCallback, (void **Argument));
    CP_DECLARE_METHOD_PTR(T, void, SetEffect, (VX_EFFECT Effect));
    CP_DECLARE_METHOD_PTR(T, VX_EFFECT, GetEffect, ());
    CP_DECLARE_METHOD_PTR(T, CKParameter *, GetEffectParameter, ());
};

template <class T>
struct CP_CLASS_VTABLE_NAME(CKTexture) : public CP_CLASS_VTABLE_NAME(CKBeObject)<CKTexture> {
    CP_DECLARE_METHOD_PTR(T, CKBOOL, Create, (int Width, int Height, int BPP, int Slot));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, LoadImage, (CKSTRING Name, int Slot));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, LoadMovie, (CKSTRING Name));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, SetAsCurrent, (CKRenderContext *Dev, CKBOOL Clamping, int TextureStage));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, Restore, (CKBOOL Clamp));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, SystemToVideoMemory, (CKRenderContext *Dev, CKBOOL Clamping));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, FreeVideoMemory, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsInVideoMemory, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, CopyContext, (CKRenderContext *ctx, VxRect *Src, VxRect *Dest, int CubeMapFace));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, UseMipmap, (int UseMipMap));
    CP_DECLARE_METHOD_PTR(T, int, GetMipmapCount, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, GetVideoTextureDesc, (VxImageDescEx &desc));
    CP_DECLARE_METHOD_PTR(T, VX_PIXELFORMAT, GetVideoPixelFormat, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, GetSystemTextureDesc, (VxImageDescEx &desc));
    CP_DECLARE_METHOD_PTR(T, void, SetDesiredVideoFormat, (VX_PIXELFORMAT Format));
    CP_DECLARE_METHOD_PTR(T, VX_PIXELFORMAT, GetDesiredVideoFormat, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, SetUserMipMapMode, (CKBOOL UserMipmap));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, GetUserMipMapLevel, (int Level, VxImageDescEx &ResultImage));
    CP_DECLARE_METHOD_PTR(T, int, GetRstTextureIndex, ());
};

template <class T>
struct CP_CLASS_VTABLE_NAME(CKMesh) : public CP_CLASS_VTABLE_NAME(CKBeObject)<CKMesh> {
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsTransparent, ());
    CP_DECLARE_METHOD_PTR(T, void, SetTransparent, (CKBOOL Transparency));
    CP_DECLARE_METHOD_PTR(T, void, SetWrapMode, (VXTEXTURE_WRAPMODE Mode));
    CP_DECLARE_METHOD_PTR(T, VXTEXTURE_WRAPMODE, GetWrapMode, ());
    CP_DECLARE_METHOD_PTR(T, void, SetLitMode, (VXMESH_LITMODE Mode));
    CP_DECLARE_METHOD_PTR(T, VXMESH_LITMODE, GetLitMode, ());
    CP_DECLARE_METHOD_PTR(T, CKDWORD, GetFlags, ());
    CP_DECLARE_METHOD_PTR(T, void, SetFlags, (CKDWORD Flags));
    CP_DECLARE_METHOD_PTR(T, CKBYTE *, GetModifierVertices, (CKDWORD *Stride));
    CP_DECLARE_METHOD_PTR(T, int, GetModifierVertexCount, ());
    CP_DECLARE_METHOD_PTR(T, void, ModifierVertexMove, (CKBOOL RebuildNormals, CKBOOL RebuildFaceNormals));
    CP_DECLARE_METHOD_PTR(T, CKBYTE *, GetModifierUVs, (CKDWORD *Stride, int channel));
    CP_DECLARE_METHOD_PTR(T, int, GetModifierUVCount, (int channel));
    CP_DECLARE_METHOD_PTR(T, void, ModifierUVMove, ());
    CP_DECLARE_METHOD_PTR(T, int, GetVertexCount, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, SetVertexCount, (int Count));
    CP_DECLARE_METHOD_PTR(T, void, SetVertexColor, (int Index, CKDWORD Color));
    CP_DECLARE_METHOD_PTR(T, void, SetVertexSpecularColor, (int Index, CKDWORD Color));
    CP_DECLARE_METHOD_PTR(T, void, SetVertexNormal, (int Index, VxVector *Vector));
    CP_DECLARE_METHOD_PTR(T, void, SetVertexPosition, (int Index, VxVector *Vector));
    CP_DECLARE_METHOD_PTR(T, void, SetVertexTextureCoordinates, (int Index, float u, float v, int channel));
    CP_DECLARE_METHOD_PTR(T, void *, GetColorsPtr, (CKDWORD *Stride));
    CP_DECLARE_METHOD_PTR(T, void *, GetSpecularColorsPtr, (CKDWORD *Stride));
    CP_DECLARE_METHOD_PTR(T, void *, GetNormalsPtr, (CKDWORD *Stride));
    CP_DECLARE_METHOD_PTR(T, void *, GetPositionsPtr, (CKDWORD *Stride));
    CP_DECLARE_METHOD_PTR(T, void *, GetTextureCoordinatesPtr, (CKDWORD *Stride, int channel));
    CP_DECLARE_METHOD_PTR(T, CKDWORD, GetVertexColor, (int Index));
    CP_DECLARE_METHOD_PTR(T, CKDWORD, GetVertexSpecularColor, (int Index));
    CP_DECLARE_METHOD_PTR(T, void, GetVertexNormal, (int Index, VxVector *Vector));
    CP_DECLARE_METHOD_PTR(T, void, GetVertexPosition, (int Index, VxVector *Vector));
    CP_DECLARE_METHOD_PTR(T, void, GetVertexTextureCoordinates, (int Index, float *u, float *v, int channel));
    CP_DECLARE_METHOD_PTR(T, void, TranslateVertices, (VxVector *Vector));
    CP_DECLARE_METHOD_PTR(T, void, ScaleVertices, (VxVector *Vector, VxVector *Pivot));
    CP_DECLARE_METHOD_PTR(T, void, ScaleVertices3f, (float X, float Y, float Z, VxVector *Pivot));
    CP_DECLARE_METHOD_PTR(T, void, RotateVertices, (VxVector *Vector, float Angle));
    CP_DECLARE_METHOD_PTR(T, void, VertexMove, ());
    CP_DECLARE_METHOD_PTR(T, void, UVChanged, ());
    CP_DECLARE_METHOD_PTR(T, void, NormalChanged, ());
    CP_DECLARE_METHOD_PTR(T, void, ColorChanged, ());
    CP_DECLARE_METHOD_PTR(T, int, GetFaceCount, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, SetFaceCount, (int Count));
    CP_DECLARE_METHOD_PTR(T, CKWORD *, GetFacesIndices, ());
    CP_DECLARE_METHOD_PTR(T, void, GetFaceVertexIndex, (int Index, int &Vertex1, int &Vertex2, int &Vertex3));
    CP_DECLARE_METHOD_PTR(T, CKMaterial *, GetFaceMaterial, (int Index));
    CP_DECLARE_METHOD_PTR(T, const VxVector &, GetFaceNormal, (int Index));
    CP_DECLARE_METHOD_PTR(T, CKWORD, GetFaceChannelMask, (int FaceIndex));
    CP_DECLARE_METHOD_PTR(T, VxVector &, GetFaceVertex, (int FaceIndex, int VIndex));
    CP_DECLARE_METHOD_PTR(T, CKBYTE *, GetFaceNormalsPtr, (CKDWORD *Stride));
    CP_DECLARE_METHOD_PTR(T, void, SetFaceVertexIndex, (int FaceIndex, int Vertex1, int Vertex2, int Vertex3));
    CP_DECLARE_METHOD_PTR(T, void, SetFaceMaterial, (int FaceIndex, CKMaterial *Mat));
    CP_DECLARE_METHOD_PTR(T, void, SetFaceMaterialEx, (int *FaceIndices, int FaceCount, CKMaterial *Mat));
    CP_DECLARE_METHOD_PTR(T, void, SetFaceChannelMask, (int FaceIndex, CKWORD ChannelMask));
    CP_DECLARE_METHOD_PTR(T, void, ReplaceMaterial, (CKMaterial *oldMat, CKMaterial *newMat));
    CP_DECLARE_METHOD_PTR(T, void, ChangeFaceChannelMask, (int FaceIndex, CKWORD AddChannelMask, CKWORD RemoveChannelMask));
    CP_DECLARE_METHOD_PTR(T, void, ApplyGlobalMaterial, (CKMaterial *Mat));
    CP_DECLARE_METHOD_PTR(T, void, DissociateAllFaces, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, SetLineCount, (int Count));
    CP_DECLARE_METHOD_PTR(T, int, GetLineCount, ());
    CP_DECLARE_METHOD_PTR(T, CKWORD *, GetLineIndices, ());
    CP_DECLARE_METHOD_PTR(T, void, SetLine, (int LineIndex, int VIndex1, int VIndex2));
    CP_DECLARE_METHOD_PTR(T, void, GetLine, (int LineIndex, int *VIndex1, int *VIndex2));
    CP_DECLARE_METHOD_PTR(T, void, CreateLineStrip, (int StartingLine, int Count, int StartingVertexIndex));
    CP_DECLARE_METHOD_PTR(T, void, Clean, (CKBOOL KeepVertices));
    CP_DECLARE_METHOD_PTR(T, void, InverseWinding, ());
    CP_DECLARE_METHOD_PTR(T, void, Consolidate, ());
    CP_DECLARE_METHOD_PTR(T, void, UnOptimize, ());
    CP_DECLARE_METHOD_PTR(T, float, GetRadius, ());
    CP_DECLARE_METHOD_PTR(T, const VxBbox &, GetLocalBox, ());
    CP_DECLARE_METHOD_PTR(T, void, GetBaryCenter, (VxVector *Vector));
    CP_DECLARE_METHOD_PTR(T, int, GetChannelCount, ());
    CP_DECLARE_METHOD_PTR(T, int, AddChannel, (CKMaterial *Mat, CKBOOL CopySrcUv));
    CP_DECLARE_METHOD_PTR(T, void, RemoveChannelMaterial, (CKMaterial *Mat));
    CP_DECLARE_METHOD_PTR(T, void, RemoveChannel, (int Index));
    CP_DECLARE_METHOD_PTR(T, int, GetChannelByMaterial, (CKMaterial *mat));
    CP_DECLARE_METHOD_PTR(T, void, ActivateChannel, (int Index, CKBOOL Active));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsChannelActive, (int Index));
    CP_DECLARE_METHOD_PTR(T, void, ActivateAllChannels, (CKBOOL Active));
    CP_DECLARE_METHOD_PTR(T, void, LitChannel, (int Index, CKBOOL Lit));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsChannelLit, (int Index));
    CP_DECLARE_METHOD_PTR(T, CKDWORD, GetChannelFlags, (int Index));
    CP_DECLARE_METHOD_PTR(T, void, SetChannelFlags, (int Index, CKDWORD Flags));
    CP_DECLARE_METHOD_PTR(T, CKMaterial *, GetChannelMaterial, (int Index));
    CP_DECLARE_METHOD_PTR(T, VXBLEND_MODE, GetChannelSourceBlend, (int Index));
    CP_DECLARE_METHOD_PTR(T, VXBLEND_MODE, GetChannelDestBlend, (int Index));
    CP_DECLARE_METHOD_PTR(T, void, SetChannelMaterial, (int Index, CKMaterial *Mat));
    CP_DECLARE_METHOD_PTR(T, void, SetChannelSourceBlend, (int Index, VXBLEND_MODE BlendMode));
    CP_DECLARE_METHOD_PTR(T, void, SetChannelDestBlend, (int Index, VXBLEND_MODE BlendMode));
    CP_DECLARE_METHOD_PTR(T, void, BuildNormals, ());
    CP_DECLARE_METHOD_PTR(T, void, BuildFaceNormals, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, Render, (CKRenderContext *Dev, CK3dEntity *Mov));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, AddPreRenderCallBack, (CK_MESHRENDERCALLBACK Function, void *Argument, CKBOOL Temporary));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, RemovePreRenderCallBack, (CK_MESHRENDERCALLBACK Function, void *Argument));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, AddPostRenderCallBack, (CK_MESHRENDERCALLBACK Function, void *Argument, CKBOOL Temporary));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, RemovePostRenderCallBack, (CK_MESHRENDERCALLBACK Function, void *Argument));
    CP_DECLARE_METHOD_PTR(T, void, SetRenderCallBack, (CK_MESHRENDERCALLBACK Function, void *Argument));
    CP_DECLARE_METHOD_PTR(T, void, SetDefaultRenderCallBack, ());
    CP_DECLARE_METHOD_PTR(T, void, RemoveAllCallbacks, ());
    CP_DECLARE_METHOD_PTR(T, int, GetMaterialCount, ());
    CP_DECLARE_METHOD_PTR(T, CKMaterial *, GetMaterial, (int index));
    CP_DECLARE_METHOD_PTR(T, int, GetVertexWeightsCount, ());
    CP_DECLARE_METHOD_PTR(T, void, SetVertexWeightsCount, (int count));
    CP_DECLARE_METHOD_PTR(T, float *, GetVertexWeightsPtr, ());
    CP_DECLARE_METHOD_PTR(T, float, GetVertexWeight, (int index));
    CP_DECLARE_METHOD_PTR(T, void, SetVertexWeight, (int index, float w));
    CP_DECLARE_METHOD_PTR(T, void, LoadVertices, (CKStateChunk *chunk));
    CP_DECLARE_METHOD_PTR(T, void, SetVerticesRendered, (int count));
    CP_DECLARE_METHOD_PTR(T, int, GetVerticesRendered, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, CreatePM, ());
    CP_DECLARE_METHOD_PTR(T, void, DestroyPM, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsPM, ());
    CP_DECLARE_METHOD_PTR(T, void, EnablePMGeoMorph, (CKBOOL enable));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsPMGeoMorphEnabled, ());
    CP_DECLARE_METHOD_PTR(T, void, SetPMGeoMorphStep, (int gs));
    CP_DECLARE_METHOD_PTR(T, int, GetPMGeoMorphStep, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, AddSubMeshPreRenderCallBack, (CK_SUBMESHRENDERCALLBACK Function, void *Argument, CKBOOL Temporary));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, RemoveSubMeshPreRenderCallBack, (CK_SUBMESHRENDERCALLBACK Function, void *Argument));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, AddSubMeshPostRenderCallBack, (CK_SUBMESHRENDERCALLBACK Function, void *Argument, CKBOOL Temporary));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, RemoveSubMeshPostRenderCallBack, (CK_SUBMESHRENDERCALLBACK Function, void *Argument));
};

template <class T>
struct CP_CLASS_VTABLE_NAME(CKPatchMesh) : public CP_CLASS_VTABLE_NAME(CKMesh)<CKPatchMesh> {
    CP_DECLARE_METHOD_PTR(T, CKERROR, FromMesh, (CKMesh *m));
    CP_DECLARE_METHOD_PTR(T, CKERROR, ToMesh, (CKMesh *m, int stepcount));
    CP_DECLARE_METHOD_PTR(T, void, SetIterationCount, (int count));
    CP_DECLARE_METHOD_PTR(T, int, GetIterationCount, ());
    CP_DECLARE_METHOD_PTR(T, void, BuildRenderMesh, ());
    CP_DECLARE_METHOD_PTR(T, void, CleanRenderMesh, ());
    CP_DECLARE_METHOD_PTR(T, void, Clear, ());
    CP_DECLARE_METHOD_PTR(T, void, ComputePatchAux, (int index));
    CP_DECLARE_METHOD_PTR(T, void, ComputePatchInteriors, (int index));
    CP_DECLARE_METHOD_PTR(T, CKDWORD, GetPatchFlags, ());
    CP_DECLARE_METHOD_PTR(T, void, SetPatchFlags, (CKDWORD Flags));
    CP_DECLARE_METHOD_PTR(T, void, SetVertVecCount, (int VertCount, int VecCount));
    CP_DECLARE_METHOD_PTR(T, int, GetVertCount, ());
    CP_DECLARE_METHOD_PTR(T, void, SetVert, (int index, VxVector *cp));
    CP_DECLARE_METHOD_PTR(T, void, GetVert, (int index, VxVector *cp));
    CP_DECLARE_METHOD_PTR(T, VxVector *, GetVerts, ());
    CP_DECLARE_METHOD_PTR(T, int, GetVecCount, ());
    CP_DECLARE_METHOD_PTR(T, void, SetVec, (int index, VxVector *cp));
    CP_DECLARE_METHOD_PTR(T, void, GetVec, (int index, VxVector *cp));
    CP_DECLARE_METHOD_PTR(T, VxVector *, GetVecs, ());
    CP_DECLARE_METHOD_PTR(T, void, SetEdgeCount, (int count));
    CP_DECLARE_METHOD_PTR(T, int, GetEdgeCount, ());
    CP_DECLARE_METHOD_PTR(T, void, SetEdge, (int index, CKPatchEdge *edge));
    CP_DECLARE_METHOD_PTR(T, void, GetEdge, (int index, CKPatchEdge *edge));
    CP_DECLARE_METHOD_PTR(T, CKPatchEdge *, GetEdges, ());
    CP_DECLARE_METHOD_PTR(T, void, SetPatchCount, (int count));
    CP_DECLARE_METHOD_PTR(T, int, GetPatchCount, ());
    CP_DECLARE_METHOD_PTR(T, void, SetPatch, (int index, CKPatch *p));
    CP_DECLARE_METHOD_PTR(T, void, GetPatch, (int index, CKPatch *p));
    CP_DECLARE_METHOD_PTR(T, CKDWORD, GetPatchSM, (int index));
    CP_DECLARE_METHOD_PTR(T, void, SetPatchSM, (int index, CKDWORD smoothing));
    CP_DECLARE_METHOD_PTR(T, CKMaterial *, GetPatchMaterial, (int index));
    CP_DECLARE_METHOD_PTR(T, void, SetPatchMaterial, (int index, CKMaterial *mat));
    CP_DECLARE_METHOD_PTR(T, CKPatch *, GetPatches, ());
    CP_DECLARE_METHOD_PTR(T, void, SetTVPatchCount, (int count, int Channel));
    CP_DECLARE_METHOD_PTR(T, int, GetTVPatchCount, (int Channel));
    CP_DECLARE_METHOD_PTR(T, void, SetTVPatch, (int index, CKTVPatch *tvpatch, int Channel));
    CP_DECLARE_METHOD_PTR(T, void, GetTVPatch, (int index, CKTVPatch *tvpatch, int Channel));
    CP_DECLARE_METHOD_PTR(T, CKTVPatch *, GetTVPatches, (int Channel));
    CP_DECLARE_METHOD_PTR(T, void, SetTVCount, (int count, int Channel));
    CP_DECLARE_METHOD_PTR(T, int, GetTVCount, (int Channel));
    CP_DECLARE_METHOD_PTR(T, void, SetTV, (int index, float u, float v, int Channel));
    CP_DECLARE_METHOD_PTR(T, void, GetTV, (int index, float *u, float *v, int Channel));
    CP_DECLARE_METHOD_PTR(T, VxUV *, GetTVs, (int Channel));
};

template <class T>
struct CP_CLASS_VTABLE_NAME(CKRenderObject) : public CP_CLASS_VTABLE_NAME(CKBeObject)<CKRenderObject> {
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsInRenderContext, (CKRenderContext *context));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsRootObject, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsToBeRendered, ());
    CP_DECLARE_METHOD_PTR(T, void, SetZOrder, (int Z));
    CP_DECLARE_METHOD_PTR(T, int, GetZOrder, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsToBeRenderedLast, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, AddPreRenderCallBack, (CK_RENDEROBJECT_CALLBACK Function, void *Argument, CKBOOL Temp, CKBOOL ModifyRenderPipeline));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, RemovePreRenderCallBack, (CK_RENDEROBJECT_CALLBACK Function, void *Argument));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, SetRenderCallBack, (CK_RENDEROBJECT_CALLBACK Function, void *Argument));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, RemoveRenderCallBack, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, AddPostRenderCallBack, (CK_RENDEROBJECT_CALLBACK Function, void *Argument, CKBOOL Temp, CKBOOL ModifyRenderPipeline));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, RemovePostRenderCallBack, (CK_RENDEROBJECT_CALLBACK Function, void *Argument));
    CP_DECLARE_METHOD_PTR(T, void, RemoveAllCallbacks, ());
};

template <class T>
struct CP_CLASS_VTABLE_NAME(CK2dEntity) : public CP_CLASS_VTABLE_NAME(CKRenderObject)<CK2dEntity> {
    CP_DECLARE_METHOD_PTR(T, CKERROR, GetPosition, (Vx2DVector &vect, CKBOOL hom, CK2dEntity *ref));
    CP_DECLARE_METHOD_PTR(T, void, SetPosition, (const Vx2DVector &vect, CKBOOL hom, CKBOOL KeepChildren, CK2dEntity *ref));
    CP_DECLARE_METHOD_PTR(T, CKERROR, GetSize, (Vx2DVector &vect, CKBOOL hom));
    CP_DECLARE_METHOD_PTR(T, void, SetSize, (const Vx2DVector &vect, CKBOOL hom, CKBOOL KeepChildren));
    CP_DECLARE_METHOD_PTR(T, void, SetRect, (const VxRect &rect, CKBOOL KeepChildren));
    CP_DECLARE_METHOD_PTR(T, void, GetRect, (VxRect &rect));
    CP_DECLARE_METHOD_PTR(T, CKERROR, SetHomogeneousRect, (const VxRect &rect, CKBOOL KeepChildren));
    CP_DECLARE_METHOD_PTR(T, CKERROR, GetHomogeneousRect, (VxRect &rect));
    CP_DECLARE_METHOD_PTR(T, void, SetSourceRect, (VxRect &rect));
    CP_DECLARE_METHOD_PTR(T, void, GetSourceRect, (VxRect &rect));
    CP_DECLARE_METHOD_PTR(T, void, UseSourceRect, (CKBOOL Use));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsUsingSourceRect, ());
    CP_DECLARE_METHOD_PTR(T, void, SetPickable, (CKBOOL Pick));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsPickable, ());
    CP_DECLARE_METHOD_PTR(T, void, SetBackground, (CKBOOL back));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsBackground, ());
    CP_DECLARE_METHOD_PTR(T, void, SetClipToParent, (CKBOOL clip));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsClipToParent, ());
    CP_DECLARE_METHOD_PTR(T, void, SetFlags, (CKDWORD Flags));
    CP_DECLARE_METHOD_PTR(T, void, ModifyFlags, (CKDWORD add, CKDWORD remove));
    CP_DECLARE_METHOD_PTR(T, CKDWORD, GetFlags, ());
    CP_DECLARE_METHOD_PTR(T, void, EnableRatioOffset, (CKBOOL Ratio));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsRatioOffset, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, SetParent, (CK2dEntity *parent));
    CP_DECLARE_METHOD_PTR(T, CK2dEntity *, GetParent, () const);
    CP_DECLARE_METHOD_PTR(T, int, GetChildrenCount, () const);
    CP_DECLARE_METHOD_PTR(T, CK2dEntity *, GetChild, (int i) const);
    CP_DECLARE_METHOD_PTR(T, CK2dEntity *, HierarchyParser, (CK2dEntity *current) const);
    CP_DECLARE_METHOD_PTR(T, void, SetMaterial, (CKMaterial *mat));
    CP_DECLARE_METHOD_PTR(T, CKMaterial *, GetMaterial, ());
    CP_DECLARE_METHOD_PTR(T, void, SetHomogeneousCoordinates, (CKBOOL Coord));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsHomogeneousCoordinates, ());
    CP_DECLARE_METHOD_PTR(T, void, EnableClipToCamera, (CKBOOL Clip));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsClippedToCamera, ());
    CP_DECLARE_METHOD_PTR(T, CKERROR, Render, (CKRenderContext *context));
    CP_DECLARE_METHOD_PTR(T, CKERROR, Draw, (CKRenderContext *context));
    CP_DECLARE_METHOD_PTR(T, void, GetExtents, (VxRect &srcrect, VxRect &rect));
    CP_DECLARE_METHOD_PTR(T, void, SetExtents, (const VxRect &srcrect, const VxRect &rect));
    CP_DECLARE_METHOD_PTR(T, void, RestoreInitialSize, ());
};

template <class T>
struct CP_CLASS_VTABLE_NAME(CK3dEntity) : public CP_CLASS_VTABLE_NAME(CKRenderObject)<CK3dEntity> {
    CP_DECLARE_METHOD_PTR(T, int, GetChildrenCount, () const);
    CP_DECLARE_METHOD_PTR(T, CK3dEntity *, GetChild, (int pos) const);
    CP_DECLARE_METHOD_PTR(T, CKBOOL, SetParent, (CK3dEntity *Parent, CKBOOL KeepWorldPos));
    CP_DECLARE_METHOD_PTR(T, CK3dEntity *, GetParent, () const);
    CP_DECLARE_METHOD_PTR(T, CKBOOL, AddChild, (CK3dEntity *Child, CKBOOL KeepWorldPos));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, AddChildren, (const XObjectPointerArray &Children, CKBOOL KeepWorldPos));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, RemoveChild, (CK3dEntity *Mov));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, CheckIfSameKindOfHierarchy, (CK3dEntity *Mov, CKBOOL SameOrder) const);
    CP_DECLARE_METHOD_PTR(T, CK3dEntity *, HierarchyParser, (CK3dEntity *current) const);
    CP_DECLARE_METHOD_PTR(T, CKDWORD, GetFlags, () const);
    CP_DECLARE_METHOD_PTR(T, void, SetFlags, (CKDWORD Flags));
    CP_DECLARE_METHOD_PTR(T, void, SetPickable,(CKBOOL Pick));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsPickable, () const);
    CP_DECLARE_METHOD_PTR(T, void, SetRenderChannels, (CKBOOL RenderChannels));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, AreRenderChannelsVisible, () const);
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsInViewFrustrum, (CKRenderContext *Dev, CKDWORD Flags));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsInViewFrustrumHierarchic, (CKRenderContext *Dev));
    CP_DECLARE_METHOD_PTR(T, void, IgnoreAnimations, (CKBOOL ignore));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, AreAnimationIgnored, () const);
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsAllInsideFrustrum, () const);
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsAllOutsideFrustrum, () const);
    CP_DECLARE_METHOD_PTR(T, void, SetRenderAsTransparent, (CKBOOL Trans));
    CP_DECLARE_METHOD_PTR(T, CKDWORD, GetMoveableFlags, () const);
    CP_DECLARE_METHOD_PTR(T, void, SetMoveableFlags, (CKDWORD flags));
    CP_DECLARE_METHOD_PTR(T, CKDWORD, ModifyMoveableFlags, (CKDWORD Add, CKDWORD Remove));
    CP_DECLARE_METHOD_PTR(T, CKMesh *, GetCurrentMesh, () const);
    CP_DECLARE_METHOD_PTR(T, CKMesh *, SetCurrentMesh, (CKMesh *m, CKBOOL add_if_not_here));
    CP_DECLARE_METHOD_PTR(T, int, GetMeshCount, () const);
    CP_DECLARE_METHOD_PTR(T, CKMesh *, GetMesh, (int pos) const);
    CP_DECLARE_METHOD_PTR(T, CKERROR, AddMesh, (CKMesh *mesh));
    CP_DECLARE_METHOD_PTR(T, CKERROR, RemoveMesh, (CKMesh *mesh));
    CP_DECLARE_METHOD_PTR(T, void, LookAt, (const VxVector *Pos, CK3dEntity *Ref, CKBOOL KeepChildren));
    CP_DECLARE_METHOD_PTR(T, void, Rotate3f, (float X, float Y, float Z, float Angle, CK3dEntity *Ref, CKBOOL KeepChildren));
    CP_DECLARE_METHOD_PTR(T, void, Rotate, (const VxVector *Axis, float Angle, CK3dEntity *Ref, CKBOOL KeepChildren));
    CP_DECLARE_METHOD_PTR(T, void, Translate3f, (float X, float Y, float Z, CK3dEntity *Ref, CKBOOL KeepChildren));
    CP_DECLARE_METHOD_PTR(T, void, Translate, (const VxVector *Vect, CK3dEntity *Ref, CKBOOL KeepChildren));
    CP_DECLARE_METHOD_PTR(T, void, AddScale3f, (float X, float Y, float Z, CKBOOL KeepChildren, CKBOOL Local));
    CP_DECLARE_METHOD_PTR(T, void, AddScale, (const VxVector *Scale, CKBOOL KeepChildren, CKBOOL Local));
    CP_DECLARE_METHOD_PTR(T, void, SetPosition3f, (float X, float Y, float Z, CK3dEntity *Ref, CKBOOL KeepChildren));
    CP_DECLARE_METHOD_PTR(T, void, SetPosition, (const VxVector *Pos, CK3dEntity *Ref, CKBOOL KeepChildren));
    CP_DECLARE_METHOD_PTR(T, void, GetPosition, (VxVector *Pos, CK3dEntity *Ref) const);
    CP_DECLARE_METHOD_PTR(T, void, SetOrientation, (const VxVector *Dir, const VxVector *Up, const VxVector *Right, CK3dEntity *Ref, CKBOOL KeepChildren));
    CP_DECLARE_METHOD_PTR(T, void, GetOrientation, (VxVector *Dir, VxVector *Up, VxVector *Right, CK3dEntity *Ref));
    CP_DECLARE_METHOD_PTR(T, void, SetQuaternion, (const VxQuaternion *Quat, CK3dEntity *Ref, CKBOOL KeepChildren, CKBOOL KeepScale));
    CP_DECLARE_METHOD_PTR(T, void, GetQuaternion, (VxQuaternion *Quat, CK3dEntity *Ref));
    CP_DECLARE_METHOD_PTR(T, void, SetScale3f, (float X, float Y, float Z, CKBOOL KeepChildren, CKBOOL Local));
    CP_DECLARE_METHOD_PTR(T, void, SetScale, (const VxVector *Scale, CKBOOL KeepChildren, CKBOOL Local));
    CP_DECLARE_METHOD_PTR(T, void, GetScale, (VxVector *Scale, CKBOOL Local));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, ConstructWorldMatrix, (const VxVector *Pos, const VxVector *Scale, const VxQuaternion *Quat));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, ConstructWorldMatrixEx, (const VxVector *Pos, const VxVector *Scale, const VxQuaternion *Quat, const VxQuaternion *Shear, float Sign));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, ConstructLocalMatrix, (const VxVector *Pos, const VxVector *Scale, const VxQuaternion *Quat));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, ConstructLocalMatrixEx, (const VxVector *Pos, const VxVector *Scale, const VxQuaternion *Quat, const VxQuaternion *Shear, float Sign));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, Render, (CKRenderContext *Dev, CKDWORD Flags));
    CP_DECLARE_METHOD_PTR(T, int, RayIntersection, (const VxVector *Pos1, const VxVector *Pos2, VxIntersectionDesc *Desc, CK3dEntity *Ref, CK_RAYINTERSECTION iOptions));
    CP_DECLARE_METHOD_PTR(T, void, GetRenderExtents, (VxRect &rect) const);
    CP_DECLARE_METHOD_PTR(T, const VxMatrix &, GetLastFrameMatrix, () const);
    CP_DECLARE_METHOD_PTR(T, void, SetLocalMatrix, (const VxMatrix &Mat, CKBOOL KeepChildren));
    CP_DECLARE_METHOD_PTR(T, const VxMatrix &, GetLocalMatrix, () const);
    CP_DECLARE_METHOD_PTR(T, void, SetWorldMatrix, (const VxMatrix &Mat, CKBOOL KeepChildren));
    CP_DECLARE_METHOD_PTR(T, const VxMatrix &, GetWorldMatrix, () const);
    CP_DECLARE_METHOD_PTR(T, const VxMatrix &, GetInverseWorldMatrix, () const);
    CP_DECLARE_METHOD_PTR(T, void, Transform, (VxVector *Dest, const VxVector *Src, CK3dEntity *Ref) const);
    CP_DECLARE_METHOD_PTR(T, void, InverseTransform, (VxVector *Dest, const VxVector *Src, CK3dEntity *Ref) const);
    CP_DECLARE_METHOD_PTR(T, void, TransformVector, (VxVector *Dest, const VxVector *Src, CK3dEntity *Ref) const);
    CP_DECLARE_METHOD_PTR(T, void, InverseTransformVector, (VxVector *Dest, const VxVector *Src, CK3dEntity *Ref) const);
    CP_DECLARE_METHOD_PTR(T, void, TransformMany, (VxVector *Dest, const VxVector *Src, int count, CK3dEntity *Ref) const);
    CP_DECLARE_METHOD_PTR(T, void, InverseTransformMany, (VxVector *Dest, const VxVector *Src, int count, CK3dEntity *Ref) const);
    CP_DECLARE_METHOD_PTR(T, void, ChangeReferential, (CK3dEntity *Ref));
    CP_DECLARE_METHOD_PTR(T, CKPlace *, GetReferencePlace, () const);
    CP_DECLARE_METHOD_PTR(T, void, AddObjectAnimation, (CKObjectAnimation *anim));
    CP_DECLARE_METHOD_PTR(T, void, RemoveObjectAnimation, (CKObjectAnimation *anim));
    CP_DECLARE_METHOD_PTR(T, CKObjectAnimation *, GetObjectAnimation, (int index) const);
    CP_DECLARE_METHOD_PTR(T, int, GetObjectAnimationCount, () const);
    CP_DECLARE_METHOD_PTR(T, CKSkin *,CreateSkin, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, DestroySkin, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, UpdateSkin, ());
    CP_DECLARE_METHOD_PTR(T, CKSkin *, GetSkin, () const);
    CP_DECLARE_METHOD_PTR(T, void, UpdateBox, (CKBOOL World));
    CP_DECLARE_METHOD_PTR(T, const VxBbox &, GetBoundingBox, (CKBOOL Local));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, SetBoundingBox, (const VxBbox *BBox, CKBOOL Local));
    CP_DECLARE_METHOD_PTR(T, const VxBbox &, GetHierarchicalBox, (CKBOOL Local));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, GetBaryCenter, (VxVector *Pos));
    CP_DECLARE_METHOD_PTR(T, float, GetRadius, ());
};

template <class T>
struct CP_CLASS_VTABLE_NAME(CKCamera) : public CP_CLASS_VTABLE_NAME(CK3dEntity)<CKCamera> {
    CP_DECLARE_METHOD_PTR(T, float, GetFrontPlane, ());
    CP_DECLARE_METHOD_PTR(T, void, SetFrontPlane, (float front));
    CP_DECLARE_METHOD_PTR(T, float, GetBackPlane, ());
    CP_DECLARE_METHOD_PTR(T, void, SetBackPlane, (float back));
    CP_DECLARE_METHOD_PTR(T, float, GetFov, ());
    CP_DECLARE_METHOD_PTR(T, void, SetFov, (float fov));
    CP_DECLARE_METHOD_PTR(T, int, GetProjectionType, ());
    CP_DECLARE_METHOD_PTR(T, void, SetProjectionType, (int proj));
    CP_DECLARE_METHOD_PTR(T, void, SetOrthographicZoom, (float zoom));
    CP_DECLARE_METHOD_PTR(T, float, GetOrthographicZoom, ());
    CP_DECLARE_METHOD_PTR(T, void, SetAspectRatio, (int width, int height));
    CP_DECLARE_METHOD_PTR(T, void, GetAspectRatio, (int &width, int &height));
    CP_DECLARE_METHOD_PTR(T, void, ComputeProjectionMatrix, (VxMatrix &mat));
    CP_DECLARE_METHOD_PTR(T, void, ResetRoll, ());
    CP_DECLARE_METHOD_PTR(T, void, Roll, (float angle));
    CP_DECLARE_METHOD_PTR(T, CK3dEntity *, GetTarget, ());
    CP_DECLARE_METHOD_PTR(T, void, SetTarget, (CK3dEntity *target));
};

template <class T>
struct CP_CLASS_VTABLE_NAME(CKLight) : public CP_CLASS_VTABLE_NAME(CK3dEntity)<CKLight> {
    CP_DECLARE_METHOD_PTR(T, void, SetColor, (const VxColor &c));
    CP_DECLARE_METHOD_PTR(T, const VxColor &, GetColor, ());
    CP_DECLARE_METHOD_PTR(T, void, SetConstantAttenuation, (float Value));
    CP_DECLARE_METHOD_PTR(T, void, SetLinearAttenuation, (float Value));
    CP_DECLARE_METHOD_PTR(T, void, SetQuadraticAttenuation, (float Value));
    CP_DECLARE_METHOD_PTR(T, float, GetConstantAttenuation, ());
    CP_DECLARE_METHOD_PTR(T, float, GetLinearAttenuation, ());
    CP_DECLARE_METHOD_PTR(T, float, GetQuadraticAttenuation, ());
    CP_DECLARE_METHOD_PTR(T, VXLIGHT_TYPE, GetType, ());
    CP_DECLARE_METHOD_PTR(T, void, SetType, (VXLIGHT_TYPE Type));
    CP_DECLARE_METHOD_PTR(T, float, GetRange, ());
    CP_DECLARE_METHOD_PTR(T, void, SetRange, (float Value));
    CP_DECLARE_METHOD_PTR(T, float, GetHotSpot, ());
    CP_DECLARE_METHOD_PTR(T, float, GetFallOff, ());
    CP_DECLARE_METHOD_PTR(T, void, SetHotSpot, (float Value));
    CP_DECLARE_METHOD_PTR(T, void, SetFallOff, (float Value));
    CP_DECLARE_METHOD_PTR(T, float, GetFallOffShape, ());
    CP_DECLARE_METHOD_PTR(T, void, SetFallOffShape, (float Value));
    CP_DECLARE_METHOD_PTR(T, void, Active, (CKBOOL Active));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, GetActivity, ());
    CP_DECLARE_METHOD_PTR(T, void, SetSpecularFlag, (CKBOOL Specular));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, GetSpecularFlag, ());
    CP_DECLARE_METHOD_PTR(T, CK3dEntity *, GetTarget, ());
    CP_DECLARE_METHOD_PTR(T, void, SetTarget, (CK3dEntity *target));
    CP_DECLARE_METHOD_PTR(T, float, GetLightPower, ());
    CP_DECLARE_METHOD_PTR(T, void, SetLightPower, (float power));
};

template <class T>
struct CP_CLASS_VTABLE_NAME(CK3dObject) : public CP_CLASS_VTABLE_NAME(CK3dEntity)<CK3dObject> {};

template <class T>
struct CP_CLASS_VTABLE_NAME(CKSprite3D) : public CP_CLASS_VTABLE_NAME(CK3dEntity)<CKSprite3D> {
    CP_DECLARE_METHOD_PTR(T, void, SetMaterial, (CKMaterial *Mat));
    CP_DECLARE_METHOD_PTR(T, CKMaterial *, GetMaterial, ());
    CP_DECLARE_METHOD_PTR(T, void, SetSize, (Vx2DVector &vect));
    CP_DECLARE_METHOD_PTR(T, void, GetSize, (Vx2DVector &vect));
    CP_DECLARE_METHOD_PTR(T, void, SetOffset, (Vx2DVector &vect));
    CP_DECLARE_METHOD_PTR(T, void, GetOffset, (Vx2DVector &vect));
    CP_DECLARE_METHOD_PTR(T, void, SetUVMapping, (VxRect &rect));
    CP_DECLARE_METHOD_PTR(T, void, GetUVMapping, (VxRect &rect));
    CP_DECLARE_METHOD_PTR(T, void, SetMode, (VXSPRITE3D_TYPE Mode));
    CP_DECLARE_METHOD_PTR(T, VXSPRITE3D_TYPE, GetMode, ());
};

template <class T>
struct CP_CLASS_VTABLE_NAME(CKPlace) : public CP_CLASS_VTABLE_NAME(CK3dEntity)<CKPlace> {
    CP_DECLARE_METHOD_PTR(T, CKCamera *, GetDefaultCamera, ());
    CP_DECLARE_METHOD_PTR(T, void, SetDefaultCamera, (CKCamera *cam));
    CP_DECLARE_METHOD_PTR(T, void, AddPortal, (CKPlace *place, CK3dEntity *portal));
    CP_DECLARE_METHOD_PTR(T, void, RemovePortal, (CKPlace *place, CK3dEntity *portal));
    CP_DECLARE_METHOD_PTR(T, int, GetPortalCount, ());
    CP_DECLARE_METHOD_PTR(T, CKPlace *, GetPortal, (int i, CK3dEntity **portal));
    CP_DECLARE_METHOD_PTR(T, VxRect &, ViewportClip, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, ComputeBestFitBBox, (CKPlace *p2, VxMatrix &BBoxMatrix));
};

template <class T>
struct CP_CLASS_VTABLE_NAME(CKGrid) : public CP_CLASS_VTABLE_NAME(CK3dEntity)<CKGrid> {
    CP_DECLARE_METHOD_PTR(T, void, ConstructMeshTexture, (float opacity));
    CP_DECLARE_METHOD_PTR(T, void, DestroyMeshTexture, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsActive, ());
    CP_DECLARE_METHOD_PTR(T, void, SetHeightValidity, (float HeightValidity));
    CP_DECLARE_METHOD_PTR(T, float, GetHeightValidity, ());
    CP_DECLARE_METHOD_PTR(T, int, GetWidth, ());
    CP_DECLARE_METHOD_PTR(T, int, GetLength, ());
    CP_DECLARE_METHOD_PTR(T, void, SetDimensions, (int width, int length, float sizeX, float sizeY));
    CP_DECLARE_METHOD_PTR(T, float, Get2dCoordsFrom3dPos, (const VxVector *pos3d, int *x, int *y));
    CP_DECLARE_METHOD_PTR(T, void, Get3dPosFrom2dCoords, (VxVector *pos3d, int x, int z));
    CP_DECLARE_METHOD_PTR(T, CKERROR, AddClassification, (int classification));
    CP_DECLARE_METHOD_PTR(T, CKERROR, AddClassificationByName, (CKSTRING ClassificationName));
    CP_DECLARE_METHOD_PTR(T, CKERROR, RemoveClassification, (int classification));
    CP_DECLARE_METHOD_PTR(T, CKERROR, RemoveClassificationByName, (CKSTRING ClassificationName));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, HasCompatibleClass, (CK3dEntity *ent));
    CP_DECLARE_METHOD_PTR(T, void, SetGridPriority, (int Priority));
    CP_DECLARE_METHOD_PTR(T, int, GetGridPriority, ());
    CP_DECLARE_METHOD_PTR(T, void, SetOrientationMode, (CK_GRIDORIENTATION orimode));
    CP_DECLARE_METHOD_PTR(T, CK_GRIDORIENTATION, GetOrientationMode, ());
    CP_DECLARE_METHOD_PTR(T, CKLayer *, AddLayer, (int type, int Format));
    CP_DECLARE_METHOD_PTR(T, CKLayer *, AddLayerByName, (CKSTRING TypeName, int Format));
    CP_DECLARE_METHOD_PTR(T, CKLayer *, GetLayer, (int type));
    CP_DECLARE_METHOD_PTR(T, CKLayer *, GetLayerByName, (CKSTRING TypeName));
    CP_DECLARE_METHOD_PTR(T, int, GetLayerCount, ());
    CP_DECLARE_METHOD_PTR(T, CKLayer *, GetLayerByIndex, (int type));
    CP_DECLARE_METHOD_PTR(T, CKERROR, RemoveLayer, (int type));
    CP_DECLARE_METHOD_PTR(T, CKERROR, RemoveLayerByName, (CKSTRING TypeName));
    CP_DECLARE_METHOD_PTR(T, CKERROR, RemoveAllLayers, ());
};

template <class T>
struct CP_CLASS_VTABLE_NAME(CKTargetCamera) : public CP_CLASS_VTABLE_NAME(CKCamera)<CKTargetCamera> {};

template <class T>
struct CP_CLASS_VTABLE_NAME(CKTargetLight) : public CP_CLASS_VTABLE_NAME(CKLight)<CKTargetLight> {};

template <class T>
struct CP_CLASS_VTABLE_NAME(CKSprite) : public CP_CLASS_VTABLE_NAME(CK2dEntity)<CKSprite> {
    CP_DECLARE_METHOD_PTR(T, CKBOOL, Create, (int Width, int Height, int BPP, int Slot));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, LoadImage, (CKSTRING Name, int Slot));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, SaveImage, (CKSTRING Name, int Slot, CKBOOL CKUseFormat));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, LoadMovie, (CKSTRING Name, int width, int height, int Bpp));
    CP_DECLARE_METHOD_PTR(T, CKSTRING, GetMovieFileName, ());
    CP_DECLARE_METHOD_PTR(T, CKMovieReader *, GetMovieReader, ());
    CP_DECLARE_METHOD_PTR(T, CKBYTE *, LockSurfacePtr, (int Slot));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, ReleaseSurfacePtr, (int Slot));
    CP_DECLARE_METHOD_PTR(T, CKSTRING, GetSlotFileName, (int Slot));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, SetSlotFileName, (int Slot, CKSTRING Filename));
    CP_DECLARE_METHOD_PTR(T, int, GetWidth, ());
    CP_DECLARE_METHOD_PTR(T, int, GetHeight, ());
    CP_DECLARE_METHOD_PTR(T, int, GetBitsPerPixel, ());
    CP_DECLARE_METHOD_PTR(T, int, GetBytesPerLine, ());
    CP_DECLARE_METHOD_PTR(T, int, GetRedMask, ());
    CP_DECLARE_METHOD_PTR(T, int, GetGreenMask, ());
    CP_DECLARE_METHOD_PTR(T, int, GetBlueMask, ());
    CP_DECLARE_METHOD_PTR(T, int, GetAlphaMask, ());
    CP_DECLARE_METHOD_PTR(T, int, GetSlotCount, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, SetSlotCount, (int Count));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, SetCurrentSlot, (int Slot));
    CP_DECLARE_METHOD_PTR(T, int, GetCurrentSlot, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, ReleaseSlot, (int Slot));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, ReleaseAllSlots, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, SetPixel, (int x, int y, CKDWORD col, int slot));
    CP_DECLARE_METHOD_PTR(T, CKDWORD, GetPixel, (int x, int y, int slot));
    CP_DECLARE_METHOD_PTR(T, CKDWORD, GetTransparentColor, ());
    CP_DECLARE_METHOD_PTR(T, void, SetTransparentColor, (CKDWORD Color));
    CP_DECLARE_METHOD_PTR(T, void, SetTransparent, (CKBOOL Transparency));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsTransparent, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, Restore, (CKBOOL Clamp));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, SystemToVideoMemory, (CKRenderContext *Dev, CKBOOL Clamping));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, FreeVideoMemory, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, IsInVideoMemory, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, CopyContext, (CKRenderContext *ctx, VxRect *Src, VxRect *Dest));
    CP_DECLARE_METHOD_PTR(T, CKBOOL, GetVideoTextureDesc, (VxImageDescEx &desc));
    CP_DECLARE_METHOD_PTR(T, VX_PIXELFORMAT, GetVideoPixelFormat, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, GetSystemTextureDesc, (VxImageDescEx &desc));
    CP_DECLARE_METHOD_PTR(T, void, SetDesiredVideoFormat, (VX_PIXELFORMAT pf));
    CP_DECLARE_METHOD_PTR(T, VX_PIXELFORMAT, GetDesiredVideoFormat, ());
    CP_DECLARE_METHOD_PTR(T, CK_BITMAP_SAVEOPTIONS, GetSaveOptions, ());
    CP_DECLARE_METHOD_PTR(T, void, SetSaveOptions, (CK_BITMAP_SAVEOPTIONS Options));
    CP_DECLARE_METHOD_PTR(T, CKBitmapProperties *, GetSaveFormat, ());
    CP_DECLARE_METHOD_PTR(T, void, SetSaveFormat, (CKBitmapProperties *format));
    CP_DECLARE_METHOD_PTR(T, void, SetPickThreshold, (int pt));
    CP_DECLARE_METHOD_PTR(T, int, GetPickThreshold, ());
    CP_DECLARE_METHOD_PTR(T, CKBOOL, ToRestore, ());
};

template <class T>
struct CP_CLASS_VTABLE_NAME(CKSpriteText) : public CP_CLASS_VTABLE_NAME(CKSprite)<CKSpriteText> {
    CP_DECLARE_METHOD_PTR(T, void, SetText, (CKSTRING text));
    CP_DECLARE_METHOD_PTR(T, CKSTRING, GetText, ());
    CP_DECLARE_METHOD_PTR(T, void, SetTextColor, (CKDWORD col));
    CP_DECLARE_METHOD_PTR(T, CKDWORD, GetTextColor, ());
    CP_DECLARE_METHOD_PTR(T, void, SetBackgroundColor, (CKDWORD col));
    CP_DECLARE_METHOD_PTR(T, CKDWORD, GetBackgroundTextColor, ());
    CP_DECLARE_METHOD_PTR(T, void, SetFont, (CKSTRING FontName, int FontSize, int Weight, CKBOOL italic, CKBOOL underline));
    CP_DECLARE_METHOD_PTR(T, void, SetAlign, (CKSPRITETEXT_ALIGNMENT align));
    CP_DECLARE_METHOD_PTR(T, CKSPRITETEXT_ALIGNMENT, GetAlign, ());
};

#endif // BML_VTABLES_H
