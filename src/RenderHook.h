#ifndef BML_RENDERHOOK_H
#define BML_RENDERHOOK_H

#include "CKRenderManager.h"
#include "CKRenderContext.h"
#include "CKRasterizer.h"

#include "HookUtils.h"
#include "VTables.h"

struct VxCallBack {
    void *callback;
    void *argument;
    CKBOOL temp;
};

struct VxOption {
    CKDWORD Value;
    XString Key;

    void Set(XString &key, CKDWORD &value) {
        Key = key;
        Value = value;
    }

    void Set(const char *key, CKDWORD value) {
        Key = key;
        Value = value;
    }
};

struct VxDriverDesc2 {
    CKBOOL CapsUpToDate;
    CKDWORD DriverId;
    VxDriverDesc Desc;
    CKRasterizer *Rasterizer;
    CKRasterizerDriver *RasterizerDriver;
};

void UpdateDriverDescCaps(VxDriverDesc2 *desc);

class CKCallbacksContainer {
public:
    XClassArray<VxCallBack> m_PreCallBacks;
    VxCallBack *m_OnCallBack;
    XClassArray<VxCallBack> m_PostCallBacks;
};

struct UserDrawPrimitiveDataClass : public VxDrawPrimitiveData {
    CKDWORD field_0[29];
};

struct CKRenderContextSettings {
    CKRECT m_Rect;
    int m_Bpp;
    int m_Zbpp;
    int m_StencilBpp;

    explicit CKRenderContextSettings(CKRasterizerContext *rst);
};

struct CKObjectExtents {
    VxRect m_Rect;
    CK_RENDER_FLAGS m_Flags = CK_RENDER_USECURRENTSETTINGS;
    CK_ID m_Camera = 0;
};

class CKSceneGraphNode;

struct CKTransparentObject {
    CKSceneGraphNode *m_Node = nullptr;
    float m_ZhMin = 0.0f;
    float m_ZhMax = 0.0f;

    CKTransparentObject() = default;
    explicit CKTransparentObject(CKSceneGraphNode *node) : m_Node(node) {}
};

class CKRenderedScene {
public:
    explicit CKRenderedScene(CKRenderContext *rc);
    ~CKRenderedScene();

    CKERROR Draw(CK_RENDER_FLAGS Flags);

    void SetDefaultRenderStates(CKRasterizerContext *rst);
    void SetupLights(CKRasterizerContext *rst);

    void PrepareCameras(CK_RENDER_FLAGS Flags);
    void ForceCameraSettingsUpdate();

    void UpdateViewportSize(CKBOOL updateProjection, CK_RENDER_FLAGS Flags);
    void ResizeViewport(const VxRect &rect);

    void AddObject(CKRenderObject *obj);
    void RemoveObject(CKRenderObject *obj);
    void DetachAll();

    static bool Hook(void *base);
    static bool Unhook(void *base);

    CKRenderContext *m_RenderContext;
    CKContext *m_Context;
    CKMaterial *m_BackgroundMaterial;
    CK3dEntity *m_RootEntity;
    CKCamera *m_AttachedCamera;
    XArray<CK3dEntity *> m_3DEntities;
    XArray<CKCamera *> m_Cameras;
    XArray<CKLight *> m_Lights;
    CKDWORD m_FogMode;
    float m_FogStart;
    float m_FogEnd;
    float m_FogDensity;
    CKDWORD m_FogColor;
    CKDWORD m_AmbientLight;
    int m_LightCount;
    XArray<CK2dEntity *> m_2DEntities;

    CP_DECLARE_METHOD_PTRS(CKRenderedScene, CKERROR, Draw, (CK_RENDER_FLAGS Flags));
    CP_DECLARE_METHOD_PTRS(CKRenderedScene, void, SetDefaultRenderStates, (CKRasterizerContext *rst));
    CP_DECLARE_METHOD_PTRS(CKRenderedScene, void, SetupLights, (CKRasterizerContext *rst));
    CP_DECLARE_METHOD_PTRS(CKRenderedScene, void, PrepareCameras, (CK_RENDER_FLAGS Flags));
    CP_DECLARE_METHOD_PTRS(CKRenderedScene, void, ForceCameraSettingsUpdate, ());
    CP_DECLARE_METHOD_PTRS(CKRenderedScene, void, UpdateViewportSize, (CKBOOL updateProjection, CK_RENDER_FLAGS Flags));
    CP_DECLARE_METHOD_PTRS(CKRenderedScene, void, ResizeViewport, (const VxRect &rect));
    CP_DECLARE_METHOD_PTRS(CKRenderedScene, void, AddObject, (CKRenderObject *obj));
    CP_DECLARE_METHOD_PTRS(CKRenderedScene, void, RemoveObject, (CKRenderObject *obj));
    CP_DECLARE_METHOD_PTRS(CKRenderedScene, void, DetachAll, ());
};

class CKSceneGraphNode {
public:
    void NoTestsTraversal(CKRenderContext *Dev, CK_RENDER_FLAGS Flags);

    void AddNode(CKSceneGraphNode *node);
    void RemoveNode(CKSceneGraphNode *node);
    void SortNodes();

    CKDWORD Rebuild();

    void SetAsPotentiallyVisible();
    void SetAsInsideFrustum();

    void SetRenderContextMask(CKDWORD mask, CKBOOL force);

    void SetPriority(int priority, int unused);

    void PrioritiesChanged();
    void EntityFlagsChanged(CKBOOL hierarchically);
    CKBOOL IsToBeParsed();

    CKBOOL ComputeHierarchicalBox();
    void InvalidateBox(CKBOOL hierarchically);

    CKBOOL IsInsideFrustum() const { return (m_Flags & 1) != 0; }
    void sub_100789A0();

    static bool Hook(void *base);
    static bool Unhook(void *base);

    CK3dEntity *m_Entity;
    CKDWORD m_TimeFpsCalc;
    CKDWORD m_Flags;
    int m_Index;
    VxBbox m_Bbox;
    CKWORD m_Priority;
    CKWORD m_MaxPriority;
    CKDWORD m_RenderContextMask;
    CKDWORD m_EntityMask;
    CKSceneGraphNode *m_Parent;
    XArray<CKSceneGraphNode *> m_Children;
    int m_ChildrenCount;

    CP_DECLARE_METHOD_PTRS(CKSceneGraphNode, void, NoTestsTraversal, (CKRenderContext *Dev, CK_RENDER_FLAGS Flags));
    CP_DECLARE_METHOD_PTRS(CKSceneGraphNode, void, AddNode, (CKSceneGraphNode *node));
    CP_DECLARE_METHOD_PTRS(CKSceneGraphNode, void, RemoveNode, (CKSceneGraphNode *node));
    CP_DECLARE_METHOD_PTRS(CKSceneGraphNode, void, SortNodes, ());
    CP_DECLARE_METHOD_PTRS(CKSceneGraphNode, CKDWORD, Rebuild, ());
    CP_DECLARE_METHOD_PTRS(CKSceneGraphNode, void, SetAsPotentiallyVisible, ());
    CP_DECLARE_METHOD_PTRS(CKSceneGraphNode, void, SetAsInsideFrustum, ());
    CP_DECLARE_METHOD_PTRS(CKSceneGraphNode, void, SetRenderContextMask, (CKDWORD mask, CKBOOL b));
    CP_DECLARE_METHOD_PTRS(CKSceneGraphNode, void, SetPriority, (int priority, int unused));
    CP_DECLARE_METHOD_PTRS(CKSceneGraphNode, void, PrioritiesChanged, ());
    CP_DECLARE_METHOD_PTRS(CKSceneGraphNode, void, EntityFlagsChanged, (CKBOOL hierarchically));
    CP_DECLARE_METHOD_PTRS(CKSceneGraphNode, CKBOOL, IsToBeParsed, ());
    CP_DECLARE_METHOD_PTRS(CKSceneGraphNode, CKBOOL, ComputeHierarchicalBox, ());
    CP_DECLARE_METHOD_PTRS(CKSceneGraphNode, void, InvalidateBox, (CKBOOL hierarchically));
};

class CKSceneGraphRootNode : public CKSceneGraphNode {
public:
    void RenderTransparents(CKRenderContext *Dev, CK_RENDER_FLAGS Flags);
    void SortTransparentObjects(CKRenderContext *Dev, CK_RENDER_FLAGS Flags);

    void Check();
    void Clear();

    void AddTransparentObject(CKSceneGraphNode *node);

    static bool Hook(void *base);
    static bool Unhook(void *base);

    XClassArray<CKTransparentObject> m_TransparentObjects;

    CP_DECLARE_METHOD_PTRS(CKSceneGraphRootNode, void, RenderTransparents, (CKRenderContext *Dev, CK_RENDER_FLAGS Flags));
    CP_DECLARE_METHOD_PTRS(CKSceneGraphRootNode, void, SortTransparentObjects, (CKRenderContext *Dev, CK_RENDER_FLAGS Flags));
    CP_DECLARE_METHOD_PTRS(CKSceneGraphRootNode, void, Check, ());
    CP_DECLARE_METHOD_PTRS(CKSceneGraphRootNode, void, Clear, ());
};

class CP_HOOK_CLASS_NAME(CKRenderManager) : public CKRenderManager {
public:
    CP_DECLARE_METHOD_HOOK(int, GetRenderDriverCount, ());
    CP_DECLARE_METHOD_HOOK(VxDriverDesc *, GetRenderDriverDescription, (int Driver));
    CP_DECLARE_METHOD_HOOK(void, GetDesiredTexturesVideoFormat, (VxImageDescEx &VideoFormat));
    CP_DECLARE_METHOD_HOOK(void, SetDesiredTexturesVideoFormat, (VxImageDescEx &VideoFormat));
    CP_DECLARE_METHOD_HOOK(CKRenderContext *,GetRenderContext, (int pos));
    CP_DECLARE_METHOD_HOOK(CKRenderContext *,GetRenderContextFromPoint, (CKPOINT &pt));
    CP_DECLARE_METHOD_HOOK(int, GetRenderContextCount, ());
    CP_DECLARE_METHOD_HOOK(void, Process, ());
    CP_DECLARE_METHOD_HOOK(void, FlushTextures, ());
    CP_DECLARE_METHOD_HOOK(CKRenderContext *, CreateRenderContext, (void *Window, int Driver, CKRECT *Rect, CKBOOL Fullscreen, int Bpp, int Zbpp, int StencilBpp, int RefreshRate));
    CP_DECLARE_METHOD_HOOK(CKERROR, DestroyRenderContext, (CKRenderContext *context));
    CP_DECLARE_METHOD_HOOK(void, RemoveRenderContext, (CKRenderContext *context));
    CP_DECLARE_METHOD_HOOK(CKVertexBuffer *,CreateVertexBuffer, ());
    CP_DECLARE_METHOD_HOOK(void, DestroyVertexBuffer, (CKVertexBuffer *VB));
    CP_DECLARE_METHOD_HOOK(void, SetRenderOptions, (CKSTRING RenderOptionString, CKDWORD Value));
    CP_DECLARE_METHOD_HOOK(const VxEffectDescription &, GetEffectDescription, (int EffectIndex));
    CP_DECLARE_METHOD_HOOK(int, GetEffectCount,());
    CP_DECLARE_METHOD_HOOK(int, AddEffect, (const VxEffectDescription &NewEffect));

    CKRasterizerContext *GetFullscreenContext();

    CP_DECLARE_METHOD_PTRS(CKRenderManager, CKRasterizerContext *, GetFullscreenContext, ());

    static bool Hook(void *base);
    static bool Unhook(void *base);

    XClassArray<VxCallBack> m_TemporaryPreRenderCallbacks;
    XClassArray<VxCallBack> m_TemporaryPostRenderCallbacks;
    XSObjectArray m_RenderContexts;
    XArray<CKRasterizer *> m_Rasterizers;
    VxDriverDesc2 *m_Drivers;
    int m_DriverCount;
    CKMaterial *m_DefaultMat;
    CKDWORD m_RenderContextMaskFree;
    CKSceneGraphRootNode m_SceneGraphRootNode;
    XObjectPointerArray m_MovedEntities;
    XObjectPointerArray m_Objects;
    CKDWORD field_D0;
    CKDWORD field_D4;
    CKDWORD field_D8;
    CKDWORD field_DC;
    CKDWORD field_E0;
    CKDWORD field_E4;
    CKDWORD field_E8;
    CKDWORD field_EC;
    CKDWORD field_F0;
    CKDWORD field_F4;
    CKDWORD field_F8;
    CKDWORD field_FC;
    CKDWORD field_100;
    CKDWORD field_104;
    CKDWORD field_108;
    CKDWORD field_10C;
    CKDWORD field_110;
    CKDWORD field_114;
    CKDWORD field_118;
    CKDWORD field_11C;
    CKDWORD field_120;
    CKDWORD field_124;
    CKDWORD field_128;
    CKDWORD field_12C;
    CKDWORD field_130;
    CKDWORD field_134;
    CKDWORD field_138;
    XArray<CKVertexBuffer *> m_VertexBuffers;
    VxOption m_ForceLinearFog;
    VxOption m_ForceSoftware;
    VxOption m_EnsureVertexShader;
    VxOption m_DisableFilter;
    VxOption m_DisableDithering;
    VxOption m_Antialias;
    VxOption m_DisableMipmap;
    VxOption m_DisableSpecular;
    VxOption m_UseIndexBuffers;
    VxOption m_EnableScreenDump;
    VxOption m_EnableDebugMode;
    VxOption m_VertexCache;
    VxOption m_SortTransparentObjects;
    VxOption m_TextureCacheManagement;
    VxOption m_DisablePerspectiveCorrection;
    VxOption m_TextureVideoFormat;
    VxOption m_SpriteVideoFormat;
    XArray<VxOption *> m_Options;
    CK2dEntity *m_2DRootFore;
    CK2dEntity *m_2DRootBack;
    CKDWORD m_2DRootBackName;
    CKDWORD m_2DRootForeName;
    XClassArray<VxEffectDescription> m_Effects;

    static CP_CLASS_VTABLE_NAME(CKRenderManager)<CKRenderManager> s_VTable;
};

class CP_HOOK_CLASS_NAME(CKRenderContext) : public CKRenderContext {
public:
    CP_DECLARE_METHOD_HOOK(void, AddObject, (CKRenderObject * obj));
    CP_DECLARE_METHOD_HOOK(void, AddObjectWithHierarchy, (CKRenderObject * obj));
    CP_DECLARE_METHOD_HOOK(void, RemoveObject, (CKRenderObject * obj));
    CP_DECLARE_METHOD_HOOK(CKBOOL, IsObjectAttached, (CKRenderObject * obj));
    CP_DECLARE_METHOD_HOOK(const XObjectArray &, Compute3dRootObjects, ());
    CP_DECLARE_METHOD_HOOK(const XObjectArray &, Compute2dRootObjects, ());
    CP_DECLARE_METHOD_HOOK(CK2dEntity *, Get2dRoot, (CKBOOL background));
    CP_DECLARE_METHOD_HOOK(void, DetachAll, ());
    CP_DECLARE_METHOD_HOOK(void, ForceCameraSettingsUpdate, ());
    CP_DECLARE_METHOD_HOOK(void, PrepareCameras, (CK_RENDER_FLAGS Flags));
    CP_DECLARE_METHOD_HOOK(CKERROR, Clear, (CK_RENDER_FLAGS Flags, CKDWORD Stencil));
    CP_DECLARE_METHOD_HOOK(CKERROR, DrawScene, (CK_RENDER_FLAGS Flags));
    CP_DECLARE_METHOD_HOOK(CKERROR, BackToFront, (CK_RENDER_FLAGS Flags));
    CP_DECLARE_METHOD_HOOK(CKERROR, Render, (CK_RENDER_FLAGS Flags));
    CP_DECLARE_METHOD_HOOK(void, AddPreRenderCallBack, (CK_RENDERCALLBACK Function, void * Argument, CKBOOL Temporary));
    CP_DECLARE_METHOD_HOOK(void, RemovePreRenderCallBack, (CK_RENDERCALLBACK Function, void * Argument));
    CP_DECLARE_METHOD_HOOK(void, AddPostRenderCallBack, (CK_RENDERCALLBACK Function, void * Argument, CKBOOL Temporary, CKBOOL BeforeTransparent));
    CP_DECLARE_METHOD_HOOK(void, RemovePostRenderCallBack, (CK_RENDERCALLBACK Function, void * Argument));
    CP_DECLARE_METHOD_HOOK(void, AddPostSpriteRenderCallBack, (CK_RENDERCALLBACK Function, void * Argument, CKBOOL Temporary));
    CP_DECLARE_METHOD_HOOK(void, RemovePostSpriteRenderCallBack, (CK_RENDERCALLBACK Function, void * Argument));
    CP_DECLARE_METHOD_HOOK(VxDrawPrimitiveData *, GetDrawPrimitiveStructure, (CKRST_DPFLAGS Flags, int VertexCount));
    CP_DECLARE_METHOD_HOOK(CKWORD *, GetDrawPrimitiveIndices, (int IndicesCount));
    CP_DECLARE_METHOD_HOOK(void, Transform, (VxVector * Dest, VxVector * Src, CK3dEntity * Ref));
    CP_DECLARE_METHOD_HOOK(void, TransformVertices, (int VertexCount, VxTransformData * data, CK3dEntity * Ref));
    CP_DECLARE_METHOD_HOOK(CKERROR, GoFullScreen, (int Width, int Height, int Bpp, int Driver, int RefreshRate));
    CP_DECLARE_METHOD_HOOK(CKERROR, StopFullScreen, ());
    CP_DECLARE_METHOD_HOOK(CKBOOL, IsFullScreen, ());
    CP_DECLARE_METHOD_HOOK(int, GetDriverIndex, ());
    CP_DECLARE_METHOD_HOOK(CKBOOL, ChangeDriver, (int NewDriver));
    CP_DECLARE_METHOD_HOOK(WIN_HANDLE, GetWindowHandle, ());
    CP_DECLARE_METHOD_HOOK(void, ScreenToClient, (Vx2DVector * ioPoint));
    CP_DECLARE_METHOD_HOOK(void, ClientToScreen, (Vx2DVector * ioPoint));
    CP_DECLARE_METHOD_HOOK(CKERROR, SetWindowRect, (VxRect & rect, CKDWORD Flags));
    CP_DECLARE_METHOD_HOOK(void, GetWindowRect, (VxRect & rect, CKBOOL ScreenRelative));
    CP_DECLARE_METHOD_HOOK(int, GetHeight, ());
    CP_DECLARE_METHOD_HOOK(int, GetWidth, ());
    CP_DECLARE_METHOD_HOOK(CKERROR, Resize, (int PosX, int PosY, int SizeX, int SizeY, CKDWORD Flags));
    CP_DECLARE_METHOD_HOOK(void, SetViewRect, (VxRect & rect));
    CP_DECLARE_METHOD_HOOK(void, GetViewRect, (VxRect & rect));
    CP_DECLARE_METHOD_HOOK(VX_PIXELFORMAT, GetPixelFormat, (int * Bpp, int * Zbpp, int * StencilBpp));
    CP_DECLARE_METHOD_HOOK(void, SetState, (VXRENDERSTATETYPE State, CKDWORD Value));
    CP_DECLARE_METHOD_HOOK(CKDWORD, GetState, (VXRENDERSTATETYPE State));
    CP_DECLARE_METHOD_HOOK(CKBOOL, SetTexture, (CKTexture * tex, CKBOOL Clamped, int Stage));
    CP_DECLARE_METHOD_HOOK(CKBOOL, SetTextureStageState, (CKRST_TEXTURESTAGESTATETYPE State, CKDWORD Value, int Stage));
    CP_DECLARE_METHOD_HOOK(CKRasterizerContext *, GetRasterizerContext, ());
    CP_DECLARE_METHOD_HOOK(void, SetClearBackground, (CKBOOL ClearBack));
    CP_DECLARE_METHOD_HOOK(CKBOOL, GetClearBackground, ());
    CP_DECLARE_METHOD_HOOK(void, SetClearZBuffer, (CKBOOL ClearZ));
    CP_DECLARE_METHOD_HOOK(CKBOOL, GetClearZBuffer, ());
    CP_DECLARE_METHOD_HOOK(void, GetGlobalRenderMode, (VxShadeType * Shading, CKBOOL * Texture, CKBOOL * Wireframe));
    CP_DECLARE_METHOD_HOOK(void, SetGlobalRenderMode, (VxShadeType Shading, CKBOOL Texture, CKBOOL Wireframe));
    CP_DECLARE_METHOD_HOOK(void, SetCurrentRenderOptions, (CKDWORD flags));
    CP_DECLARE_METHOD_HOOK(CKDWORD, GetCurrentRenderOptions, ());
    CP_DECLARE_METHOD_HOOK(void, ChangeCurrentRenderOptions, (CKDWORD Add, CKDWORD Remove));
    CP_DECLARE_METHOD_HOOK(void, SetCurrentExtents, (VxRect & extents));
    CP_DECLARE_METHOD_HOOK(void, GetCurrentExtents, (VxRect & extents));
    CP_DECLARE_METHOD_HOOK(void, SetAmbientLightRGB, (float R, float G, float B));
    CP_DECLARE_METHOD_HOOK(void, SetAmbientLight, (CKDWORD Color));
    CP_DECLARE_METHOD_HOOK(CKDWORD, GetAmbientLight, ());
    CP_DECLARE_METHOD_HOOK(void, SetFogMode, (VXFOG_MODE Mode));
    CP_DECLARE_METHOD_HOOK(void, SetFogStart, (float Start));
    CP_DECLARE_METHOD_HOOK(void, SetFogEnd, (float End));
    CP_DECLARE_METHOD_HOOK(void, SetFogDensity, (float Density));
    CP_DECLARE_METHOD_HOOK(void, SetFogColor, (CKDWORD Color));
    CP_DECLARE_METHOD_HOOK(VXFOG_MODE, GetFogMode, ());
    CP_DECLARE_METHOD_HOOK(float, GetFogStart, ());
    CP_DECLARE_METHOD_HOOK(float, GetFogEnd, ());
    CP_DECLARE_METHOD_HOOK(float, GetFogDensity, ());
    CP_DECLARE_METHOD_HOOK(CKDWORD, GetFogColor, ());
    CP_DECLARE_METHOD_HOOK(CKBOOL, DrawPrimitive, (VXPRIMITIVETYPE pType, CKWORD * indices, int indexcount, VxDrawPrimitiveData * data));
    CP_DECLARE_METHOD_HOOK(void, SetWorldTransformationMatrix, (const VxMatrix &M));
    CP_DECLARE_METHOD_HOOK(void, SetProjectionTransformationMatrix, (const VxMatrix &M));
    CP_DECLARE_METHOD_HOOK(void, SetViewTransformationMatrix, (const VxMatrix &M));
    CP_DECLARE_METHOD_HOOK(const VxMatrix &, GetWorldTransformationMatrix, ());
    CP_DECLARE_METHOD_HOOK(const VxMatrix &, GetProjectionTransformationMatrix, ());
    CP_DECLARE_METHOD_HOOK(const VxMatrix &, GetViewTransformationMatrix, ());
    CP_DECLARE_METHOD_HOOK(CKBOOL, SetUserClipPlane, (CKDWORD ClipPlaneIndex, const VxPlane &PlaneEquation));
    CP_DECLARE_METHOD_HOOK(CKBOOL, GetUserClipPlane, (CKDWORD ClipPlaneIndex, VxPlane & PlaneEquation));
    CP_DECLARE_METHOD_HOOK(CKRenderObject *, Pick, (int x, int y, CKPICKRESULT * oRes, CKBOOL iIgnoreUnpickable));
    CP_DECLARE_METHOD_HOOK(CKRenderObject *, PointPick, (CKPOINT pt, CKPICKRESULT * oRes, CKBOOL iIgnoreUnpickable));
    CP_DECLARE_METHOD_HOOK(CKERROR, RectPick, (const VxRect &r, XObjectPointerArray &oObjects, CKBOOL Intersect));
    CP_DECLARE_METHOD_HOOK(void, AttachViewpointToCamera, (CKCamera * cam));
    CP_DECLARE_METHOD_HOOK(void, DetachViewpointFromCamera, ());
    CP_DECLARE_METHOD_HOOK(CKCamera *, GetAttachedCamera, ());
    CP_DECLARE_METHOD_HOOK(CK3dEntity *, GetViewpoint, ());
    CP_DECLARE_METHOD_HOOK(CKMaterial *, GetBackgroundMaterial, ());
    CP_DECLARE_METHOD_HOOK(void, GetBoundingBox, (VxBbox * BBox));
    CP_DECLARE_METHOD_HOOK(void, GetStats, (VxStats * stats));
    CP_DECLARE_METHOD_HOOK(void, SetCurrentMaterial, (CKMaterial * mat, CKBOOL Lit));
    CP_DECLARE_METHOD_HOOK(void, Activate, (CKBOOL active));
    CP_DECLARE_METHOD_HOOK(int, DumpToMemory, (const VxRect *iRect, VXBUFFER_TYPE buffer, VxImageDescEx &desc));
    CP_DECLARE_METHOD_HOOK(int, CopyToVideo, (const VxRect *iRect, VXBUFFER_TYPE buffer, VxImageDescEx &desc));
    CP_DECLARE_METHOD_HOOK(CKERROR, DumpToFile, (CKSTRING filename, const VxRect *rect, VXBUFFER_TYPE buffer));
    CP_DECLARE_METHOD_HOOK(VxDirectXData *, GetDirectXInfo, ());
    CP_DECLARE_METHOD_HOOK(void, WarnEnterThread, ());
    CP_DECLARE_METHOD_HOOK(void, WarnExitThread, ());
    CP_DECLARE_METHOD_HOOK(CK2dEntity *, Pick2D, (const Vx2DVector &v));
    CP_DECLARE_METHOD_HOOK(CKBOOL, SetRenderTarget, (CKTexture * texture, int CubeMapFace));
    CP_DECLARE_METHOD_HOOK(void, AddRemoveSequence, (CKBOOL Start));
    CP_DECLARE_METHOD_HOOK(void, SetTransparentMode, (CKBOOL Trans));
    CP_DECLARE_METHOD_HOOK(void, AddDirtyRect, (CKRECT * Rect));
    CP_DECLARE_METHOD_HOOK(void, RestoreScreenBackup, ());
    CP_DECLARE_METHOD_HOOK(CKDWORD, GetStencilFreeMask, ());
    CP_DECLARE_METHOD_HOOK(void, UsedStencilBits, (CKDWORD stencilBits));
    CP_DECLARE_METHOD_HOOK(int, GetFirstFreeStencilBits, ());
    CP_DECLARE_METHOD_HOOK(VxDrawPrimitiveData *, LockCurrentVB, (CKDWORD VertexCount));
    CP_DECLARE_METHOD_HOOK(CKBOOL, ReleaseCurrentVB, ());
    CP_DECLARE_METHOD_HOOK(void, SetTextureMatrix, (const VxMatrix &M, int Stage));
    CP_DECLARE_METHOD_HOOK(void, SetStereoParameters, (float EyeSeparation, float FocalLength));
    CP_DECLARE_METHOD_HOOK(void, GetStereoParameters, (float & EyeSeparation, float & FocalLength));

    CKERROR Create(WIN_HANDLE Window, int Driver, CKRECT *Rect, CKBOOL Fullscreen, int Bpp, int Zbpp, int StencilBpp, int RefreshRate);
    CKBOOL DestroyDevice();
    void CallSprite3DBatches();
    CKBOOL UpdateProjection(CKBOOL force);
    void SetClipRect(VxRect &rect);
    void AddExtents2D(const VxRect &rect, CKObject *obj);

    void SetFullViewport(CKViewportData &data, int width, int height);

    CP_DECLARE_METHOD_PTRS(CKRenderContext, CKERROR, Create, (WIN_HANDLE Window, int Driver, CKRECT *Rect, CKBOOL Fullscreen, int Bpp, int Zbpp, int StencilBpp, int RefreshRate));
    CP_DECLARE_METHOD_PTRS(CKRenderContext, CKBOOL, DestroyDevice, ());
    CP_DECLARE_METHOD_PTRS(CKRenderContext, void, CallSprite3DBatches, ());
    CP_DECLARE_METHOD_PTRS(CKRenderContext, CKBOOL, UpdateProjection, (CKBOOL force));
    CP_DECLARE_METHOD_PTRS(CKRenderContext, void, SetClipRect, (VxRect &rect));
    CP_DECLARE_METHOD_PTRS(CKRenderContext, void, AddExtents2D, (const VxRect &rect, CKObject *obj));

    static bool Hook(void *base);
    static bool Unhook(void *base);

    WIN_HANDLE m_WinHandle;
    WIN_HANDLE m_AppHandle;
    CKRECT m_WinRect;
    CK_RENDER_FLAGS m_RenderFlags;
    CKRenderedScene *m_RenderedScene;
    CKBOOL m_Fullscreen;
    CKBOOL m_Active;
    CKBOOL m_Perspective;
    CKBOOL m_ProjectionUpdated;
    CKBOOL m_Start;
    CKBOOL m_TransparentMode;
    CKBOOL m_DeviceValid;
    CKCallbacksContainer m_PreRenderCallBacks;
    CKCallbacksContainer m_PreRenderTempCallBacks;
    CKCallbacksContainer m_PostRenderCallBacks;
    CKRenderManager *m_RenderManager;
    CKRasterizerContext *m_RasterizerContext;
    CKRasterizerDriver *m_RasterizerDriver;
    int m_DriverIndex;
    CKDWORD m_Shading;
    CKDWORD m_TextureEnabled;
    CKDWORD m_DisplayWireframe;
    VxFrustum m_Frustum;
    float m_Fov;
    float m_Zoom;
    float m_NearPlane;
    float m_FarPlane;
    VxMatrix m_ProjectionMatrix;
    CKViewportData m_ViewportData;
    CKRenderContextSettings m_Settings;
    CKRenderContextSettings m_FullscreenSettings;
    VxRect m_CurrentExtents;
    CKDWORD field_21C;
    CKDWORD m_TimeFpsCalc;
    VxTimeProfiler m_RenderTimeProfiler;
    float m_SmoothedFps;
    VxStats m_Stats;
    VxTimeProfiler m_DevicePreCallbacksTimeProfiler;
    VxTimeProfiler m_DevicePostCallbacksTimeProfiler;
    VxTimeProfiler m_ObjectsCallbacksTimeProfiler;
    VxTimeProfiler m_SpriteCallbacksTimeProfiler;
    VxTimeProfiler m_ObjectsRenderTimeProfiler;
    VxTimeProfiler m_SceneTraversalTimeProfiler;
    VxTimeProfiler m_SkinTimeProfiler;
    VxTimeProfiler m_SpriteTimeProfiler;
    VxTimeProfiler m_TransparentObjectsSortTimeProfiler;
    CK3dEntity *m_Current3dEntity;
    CKTexture *m_BufferTexture;
    CKDWORD m_CubeMapFace;
    float m_FocalLength;
    float m_EyeSeparation;
    CKDWORD m_Flags;
    CKDWORD m_FpsInterval;
    XString m_CurrentObjectDesc;
    XString m_StateString;
    CKDWORD m_SceneTraversalCalls;
    CKDWORD m_DrawSceneCalls;
    CKBOOL m_SortTransparentObjects;
    XVoidArray m_Sprite3DBatches;
    XClassArray<CKTransparentObject> m_TransparentObjects;
    int m_StencilFreeMask;
    UserDrawPrimitiveDataClass *m_UserDrawPrimitiveData;
    CKDWORD m_Mask;
    CKDWORD m_VertexBufferIndex;
    CKDWORD m_StartIndex;
    CKDWORD m_DpFlags;
    CKDWORD m_VertexBufferCount;
    XClassArray<CKObjectExtents> m_ObjectExtents;
    XClassArray<CKObjectExtents> m_Extents;
    XVoidArray m_RootObjects;
    CKCamera *m_Camera;
    CKTexture *m_NCUTex;
    VxTimeProfiler m_PVTimeProfiler;
    CKDWORD m_PVInformation;

    static bool s_DisableRender;
    static bool s_EnableWidescreenFix;
    static CP_CLASS_VTABLE_NAME(CKRenderContext)<CKRenderContext> s_VTable;
};

class CP_HOOK_CLASS_NAME(CKRenderObject) : public CKRenderObject {
public:
    void RemoveFromRenderContext(CKRenderContext *rc);

    CKDWORD m_Mask;
    CKCallbacksContainer *m_Callbacks;
};

class CP_HOOK_CLASS_NAME(CK2dEntity) : public CP_HOOK_CLASS_NAME(CKRenderObject) {
public:

#define CK_3DIMPLEMENTATION
#include "CK2dEntity.h"
#undef CK_3DIMPLEMENTATION

    CP_DECLARE_METHOD_HOOK(CKERROR, Render, (CKRenderContext *Dev));
    CP_DECLARE_METHOD_HOOK(CKERROR, Draw, (CKRenderContext *Dev));

    CKBOOL UpdateExtents(CKRenderContext *Dev);

    static bool Hook(void *base);
    static bool Unhook(void *base);

    VxRect m_Rect;
    VxRect *m_HomogeneousRect;
    VxRect m_VtxPos;
    VxRect m_SrcRect;
    CKDWORD m_Flags;
    CK2dEntity *m_Parent;
    CKMaterial *m_Material;
    XArray<CK2dEntity *> m_Children;
    VxRect m_SourceRect;
    CKDWORD m_ZOrder;

    CP_DECLARE_METHOD_PTRS(CK2dEntity, CKBOOL, UpdateExtents, (CKRenderContext *Dev));

    static CP_CLASS_VTABLE_NAME(CK2dEntity)<CK2dEntity> s_VTable;
};

class CP_HOOK_CLASS_NAME(CK3dEntity) : public CP_HOOK_CLASS_NAME(CKRenderObject) {
public:

#define CK_3DIMPLEMENTATION
#include "CK3dEntity.h"
#undef CK_3DIMPLEMENTATION

    CP_DECLARE_METHOD_HOOK(CKBOOL, Render, (CKRenderContext *Dev, CKDWORD Flags));

    void WorldMatrixChanged(CKBOOL invalidateBox, CKBOOL inverse);

    static bool Hook(void *base);
    static bool Unhook(void *base);

    CK3dEntity *m_Parent;
    XObjectPointerArray m_Meshes;
    CKMesh *m_CurrentMesh;
    CKDWORD field_14;
    XSObjectPointerArray m_ObjectAnimations;
    CKDWORD m_LastFrameMatrix;
    CKSkin *m_Skin;
    VxMatrix m_LocalMatrix;
    VxMatrix m_WorldMatrix;
    CKDWORD m_MoveableFlags;
    VxMatrix m_InverseWorldMatrix;
    XSObjectPointerArray m_Children;
    VxBbox m_LocalBoundingBox;
    VxBbox m_WorldBoundingBox;
    CKDWORD field_124;
    CKDWORD field_128;
    CKDWORD field_12C;
    CKDWORD field_130;
    CKDWORD field_134;
    CKDWORD field_138;
    VxRect m_RenderExtents;
    CKSceneGraphNode *m_SceneGraphNode;

    CP_DECLARE_METHOD_PTRS(CK3dEntity, void, WorldMatrixChanged, (CKBOOL invalidateBox, CKBOOL inverse));

    static CP_CLASS_VTABLE_NAME(CK3dEntity)<CK3dEntity> s_VTable;
};

class CP_HOOK_CLASS_NAME(CKCamera) : public CP_HOOK_CLASS_NAME(CK3dEntity) {
public:

#define CK_3DIMPLEMENTATION
#include "CKCamera.h"
#undef CK_3DIMPLEMENTATION

    float m_Fov;
    float m_FrontPlane;
    float m_BackPlane;
    int m_ProjectionType;
    float m_OrthographicZoom;
    int m_Width;
    int m_Height;
};

class CP_HOOK_CLASS_NAME(CKLight) : public CP_HOOK_CLASS_NAME(CK3dEntity) {
public:

#define CK_3DIMPLEMENTATION
#include "CKLight.h"
#undef CK_3DIMPLEMENTATION

    CKBOOL Setup(CKRasterizerContext *rst, int index);

    static bool Hook(void *base);
    static bool Unhook(void *base);

    CKLightData m_LightData;
    CKDWORD m_Flags;
    float m_LightPower;

    CP_DECLARE_METHOD_PTRS(CKLight, CKBOOL, Setup, (CKRasterizerContext *rst, int index));
};

namespace RenderHook {
    bool HookRenderEngine();
    bool UnhookRenderEngine();

    void DisableRender(bool disable);
    void EnableWidescreenFix(bool enable);
}

#endif // BML_RENDERHOOK_H
