#include "RenderHook.h"

#include "CKTimeManager.h"
#include "CKAttributeManager.h"
#include "CKParameterOut.h"
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

struct VxDriverDescEx {
    CKBOOL CapsUpToDate;
    CKDWORD DriverId;
    char DriverDesc[512];
    char DriverDesc2[512];
    CKBOOL Hardware;
    CKDWORD DisplayModeCount;
    VxDisplayMode *DisplayModes;
    XSArray<VxImageDescEx> TextureFormats;
    Vx2DCapsDesc Caps2D;
    Vx3DCapsDesc Caps3D;
    CKRasterizer *Rasterizer;
    CKRasterizerDriver *RasterizerDriver;
};

class CKCallbacksContainer {
public:
    XClassArray<VxCallBack> m_PreCallBacks;
    void *m_Args;
    XClassArray<VxCallBack> m_PostCallBacks;
};

struct UserDrawPrimitiveDataClass : public VxDrawPrimitiveData {
    CKDWORD field_0[29];
};

struct CKRenderContextSettings
{
    CKRECT m_Rect;
    CKDWORD m_Bpp;
    CKDWORD m_Zbpp;
    CKDWORD m_StencilBpp;
};

struct CKObjectExtents {
    VxRect m_Rect;
    CKDWORD m_Extent;
    CK_ID m_Camera;
};

class CKSceneGraphNode {
public:
    CK3dEntity *m_Entity;
    CKDWORD m_TimeFpsCalc;
    CKDWORD m_Flag;
    CKDWORD m_ChildCount;
    VxBbox m_Bbox;
    CKWORD m_LastPriority;
    CKWORD m_Priority;
    CKDWORD m_RenderContextMask;
    CKDWORD m_EntityFlags;
    CKSceneGraphNode *m_Parent;
    XArray<CKSceneGraphNode> m_Children;
    CKDWORD m_ChildrenCount;
};

struct CKSceneGraphRootNode : public CKSceneGraphNode {
    XArray<CKSceneGraphNode> m_TransparentObjects;
};

class CKRenderedScene {
public:
    CKRenderContext *m_RenderContext;
    CKContext *m_Context;
    CKMaterial *m_BackgroundMaterial;
    CK3dEntity *m_3dEntity;
    CKCamera *m_AttachedCamera;
    XArray<int> field_14;
    XArray<CKCamera *> m_Cameras;
    XArray<CKLight *> m_Lights;
    CKDWORD m_FogMode;
    float m_FogStart;
    float m_FogEnd;
    float m_FogDensity;
    CKDWORD m_FogColor;
    CKDWORD m_AmbientLight;
    CKDWORD field_50;
    XArray<int> field_54;
};

struct RenderManagerHook : public CKRenderManager {
public:
    XClassArray<VxCallBack> m_TemporaryPreRenderCallbacks;
    XClassArray<VxCallBack> m_TemporaryPostRenderCallbacks;
    XSObjectArray m_RenderContexts;
    XArray<CKRasterizer *> m_Rasterizers;
    VxDriverDescEx *m_Drivers;
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
    XArray<VxOption*> m_Options;
    CK2dEntity *m_2DRootFore;
    CK2dEntity *m_2DRootBack;
    CKDWORD m_2DRootBackName;
    CKDWORD m_2DRootForeName;
    XClassArray<VxEffectDescription> m_Effects;

    static CP_CLASS_VTABLE_NAME(CKRenderManager)<CKRenderManager> s_VTable;

    CP_DECLARE_METHOD_HOOK(int, GetRenderDriverCount, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetRenderDriverCount);
    }

    CP_DECLARE_METHOD_HOOK(VxDriverDesc *, GetRenderDriverDescription, (int Driver)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetRenderDriverDescription, Driver);
    }

    CP_DECLARE_METHOD_HOOK(void, GetDesiredTexturesVideoFormat, (VxImageDescEx &VideoFormat)) {
        CP_CALL_METHOD_PTR(this, s_VTable.GetDesiredTexturesVideoFormat, VideoFormat);
    }

    CP_DECLARE_METHOD_HOOK(void, SetDesiredTexturesVideoFormat, (VxImageDescEx &VideoFormat)) {
        CP_CALL_METHOD_PTR(this, s_VTable.SetDesiredTexturesVideoFormat, VideoFormat);
    }

    CP_DECLARE_METHOD_HOOK(CKRenderContext *,GetRenderContext, (int pos)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetRenderContext, pos);
    }

    CP_DECLARE_METHOD_HOOK(CKRenderContext *,GetRenderContextFromPoint, (CKPOINT &pt)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetRenderContextFromPoint, pt);
    }

    CP_DECLARE_METHOD_HOOK(int, GetRenderContextCount, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetRenderContextCount);
    }

    CP_DECLARE_METHOD_HOOK(void, Process, ()) {
        CP_CALL_METHOD_PTR(this, s_VTable.Process);
    }

    CP_DECLARE_METHOD_HOOK(void, FlushTextures, ()) {
        CP_CALL_METHOD_PTR(this, s_VTable.FlushTextures);
    }

    CP_DECLARE_METHOD_HOOK(CKRenderContext *, CreateRenderContext, (void *Window, int Driver, CKRECT *rect, CKBOOL Fullscreen, int Bpp, int Zbpp, int StencilBpp, int RefreshRate)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.CreateRenderContext, Window, Driver, rect, Fullscreen, Bpp, Zbpp, StencilBpp, RefreshRate);
    }

    CP_DECLARE_METHOD_HOOK(CKERROR, DestroyRenderContext, (CKRenderContext *context)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.DestroyRenderContext, context);
    }

    CP_DECLARE_METHOD_HOOK(void, RemoveRenderContext, (CKRenderContext *context)) {
        CP_CALL_METHOD_PTR(this, s_VTable.RemoveRenderContext, context);
    }

    CP_DECLARE_METHOD_HOOK(CKVertexBuffer *,CreateVertexBuffer, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.CreateVertexBuffer);
    }

    CP_DECLARE_METHOD_HOOK(void, DestroyVertexBuffer, (CKVertexBuffer *VB)) {
        CP_CALL_METHOD_PTR(this, s_VTable.DestroyVertexBuffer, VB);
    }

    CP_DECLARE_METHOD_HOOK(void, SetRenderOptions, (CKSTRING RenderOptionString, CKDWORD Value)) {
        CP_CALL_METHOD_PTR(this, s_VTable.SetRenderOptions, RenderOptionString, Value);
    }

    CP_DECLARE_METHOD_HOOK(const VxEffectDescription &, GetEffectDescription, (int EffectIndex)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetEffectDescription, EffectIndex);
    }

    CP_DECLARE_METHOD_HOOK(int, GetEffectCount,()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetEffectCount);
    }

    CP_DECLARE_METHOD_HOOK(int, AddEffect, (const VxEffectDescription &NewEffect)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.AddEffect, NewEffect);
    }

    static void Hook(CKRenderManager *man) {
        if (!man)
            return;

        utils::LoadVTable<CP_CLASS_VTABLE_NAME(CKRenderManager)<CKRenderManager>>(man, s_VTable);

#define HOOK_RENDER_MANAGER_VIRTUAL_METHOD(Instance, Name) \
    utils::HookVirtualMethod(Instance, &RenderManagerHook::CP_FUNC_HOOK_NAME(Name), (offsetof(CP_CLASS_VTABLE_NAME(CKRenderManager)<CKRenderManager>, Name) / sizeof(void*)))

        HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, GetRenderDriverCount);
        HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, GetRenderDriverDescription);
        HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, GetDesiredTexturesVideoFormat);
        HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, SetDesiredTexturesVideoFormat);
        HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, GetRenderContext);
        HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, GetRenderContextFromPoint);
        HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, GetRenderContextCount);
        HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, Process);
        HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, FlushTextures);
        HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, CreateRenderContext);
        HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, DestroyRenderContext);
        HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, RemoveRenderContext);
        HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, CreateVertexBuffer);
        HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, DestroyVertexBuffer);
        HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, SetRenderOptions);
        HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, GetEffectDescription);
        HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, GetEffectCount);
        HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, AddEffect);

#undef HOOK_RENDER_MANAGER_VIRTUAL_METHOD
    }

    static void Unhook(CKRenderContext *rc) {
        if (rc)
            utils::SaveVTable<CP_CLASS_VTABLE_NAME(CKRenderManager)<CKRenderManager>>(rc, s_VTable);
    }
};

struct RenderContextHook : public CKRenderContext {
    CKDWORD m_WinHandle;
    CKDWORD m_AppHandle;
    CKRECT m_WinRect;
    CK_RENDER_FLAGS m_RenderFlags;
    CKRenderedScene *m_RenderedScene;
    CKBOOL m_Fullscreen;
    CKBOOL m_Active;
    CKBOOL m_PerspectiveOrOrthographic;
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
    CKDWORD m_Driver;
    CKDWORD m_Shading;
    CKDWORD m_TextureEnabled;
    CKDWORD m_DisplayWireframe;
    VxFrustum m_Frustum;
    float m_Fov;
    float m_Zoom;
    float m_NearPlane;
    float m_FarPlane;
    VxMatrix m_TransformMatrix;
    CKViewportData m_ViewportData;
    CKRECT m_WindowRect;
    int m_Bpp;
    int m_Zbpp;
    int m_StencilBpp;
    CKRenderContextSettings m_RenderContextSettings;
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
    CKTexture *m_Texture;
    CKDWORD m_CubeMapFace;
    float m_FocalLength;
    float m_EyeSeparation;
    CKDWORD m_Flags;
    CKDWORD m_FpsInterval;
    XString m_CurrentObjectDesc;
    XString m_StateString;
    CKDWORD field_33C;
    CKDWORD m_DrawSceneCalls;
    CKBOOL m_ObjectsRendering;
    XVoidArray m_Sprite3DBatches;
    XVoidArray m_TransparentObjects;
    int m_StencilFreeMask;
    UserDrawPrimitiveDataClass *m_UserDrawPrimitiveData;
    CKDWORD m_MaskFree;
    CKDWORD m_VertexBufferIndex;
    CKDWORD m_StartIndex;
    CKDWORD m_DpFlags;
    CKDWORD m_VertexBufferCount;
    XArray<CKObjectExtents> m_ObjectExtents;
    XArray<CKObjectExtents> m_Extents;
    XVoidArray m_RootObjects;
    CKCamera *m_Camera;
    CKTexture *m_NCUTex;
    VxTimeProfiler m_TimeProfiler_3A8;
    CKDWORD m_PVInformation;

    static bool s_DisableRender;
    static CP_CLASS_VTABLE_NAME(CKRenderContext)<CKRenderContext> s_VTable;

    CP_DECLARE_METHOD_HOOK(void, AddObject, (CKRenderObject *obj)) {
        CP_CALL_METHOD_PTR(this, s_VTable.AddObject, obj);
    }

    CP_DECLARE_METHOD_HOOK(void, AddObjectWithHierarchy, (CKRenderObject *obj)) {
        CP_CALL_METHOD_PTR(this, s_VTable.AddObjectWithHierarchy, obj);
    }

    CP_DECLARE_METHOD_HOOK(void, RemoveObject, (CKRenderObject *obj)) {
        CP_CALL_METHOD_PTR(this, s_VTable.RemoveObject, obj);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsObjectAttached, (CKRenderObject *obj)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.IsObjectAttached, obj);
    }

    CP_DECLARE_METHOD_HOOK(const XObjectArray &, Compute3dRootObjects, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.Compute3dRootObjects);
    }

    CP_DECLARE_METHOD_HOOK(const XObjectArray &, Compute2dRootObjects, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.Compute2dRootObjects);
    }

    CP_DECLARE_METHOD_HOOK(CK2dEntity *, Get2dRoot, (CKBOOL background)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.Get2dRoot, background);
    }

    CP_DECLARE_METHOD_HOOK(void, DetachAll, ()) {
        CP_CALL_METHOD_PTR(this, s_VTable.DetachAll);
    }

    CP_DECLARE_METHOD_HOOK(void, ForceCameraSettingsUpdate, ()) {
        CP_CALL_METHOD_PTR(this, s_VTable.ForceCameraSettingsUpdate);
    }

    CP_DECLARE_METHOD_HOOK(void, PrepareCameras, (CK_RENDER_FLAGS Flags)) {
        CP_CALL_METHOD_PTR(this, s_VTable.PrepareCameras, Flags);
    }

    CP_DECLARE_METHOD_HOOK(CKERROR, Clear, (CK_RENDER_FLAGS Flags, CKDWORD Stencil)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.Clear, Flags, Stencil);
    }

    CP_DECLARE_METHOD_HOOK(CKERROR, DrawScene, (CK_RENDER_FLAGS Flags)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.DrawScene, Flags);
    }

    CP_DECLARE_METHOD_HOOK(CKERROR, BackToFront, (CK_RENDER_FLAGS Flags)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.BackToFront, Flags);
    }

    CP_DECLARE_METHOD_HOOK(CKERROR, Render, (CK_RENDER_FLAGS Flags)) {
        if (s_DisableRender)
            return CK_OK;

        return CP_CALL_METHOD_PTR(this, s_VTable.Render, Flags);
    }

    CP_DECLARE_METHOD_HOOK(void, AddPreRenderCallBack, (CK_RENDERCALLBACK Function, void *Argument, CKBOOL Temporary)) {
        CP_CALL_METHOD_PTR(this, s_VTable.AddPreRenderCallBack, Function, Argument, Temporary);
    }

    CP_DECLARE_METHOD_HOOK(void, RemovePreRenderCallBack, (CK_RENDERCALLBACK Function, void *Argument)) {
        CP_CALL_METHOD_PTR(this, s_VTable.RemovePreRenderCallBack, Function, Argument);
    }

    CP_DECLARE_METHOD_HOOK(void, AddPostRenderCallBack, (CK_RENDERCALLBACK Function, void *Argument, CKBOOL Temporary, CKBOOL BeforeTransparent)) {
        CP_CALL_METHOD_PTR(this, s_VTable.AddPostRenderCallBack, Function, Argument, Temporary, BeforeTransparent);
    }

    CP_DECLARE_METHOD_HOOK(void, RemovePostRenderCallBack, (CK_RENDERCALLBACK Function, void *Argument)) {
        CP_CALL_METHOD_PTR(this, s_VTable.RemovePostRenderCallBack, Function, Argument);
    }

    CP_DECLARE_METHOD_HOOK(void, AddPostSpriteRenderCallBack, (CK_RENDERCALLBACK Function, void *Argument, CKBOOL Temporary)) {
        CP_CALL_METHOD_PTR(this, s_VTable.AddPostSpriteRenderCallBack, Function, Argument, Temporary);
    }

    CP_DECLARE_METHOD_HOOK(void, RemovePostSpriteRenderCallBack, (CK_RENDERCALLBACK Function, void *Argument)) {
        CP_CALL_METHOD_PTR(this, s_VTable.RemovePostSpriteRenderCallBack, Function, Argument);
    }

    CP_DECLARE_METHOD_HOOK(VxDrawPrimitiveData *, GetDrawPrimitiveStructure, (CKRST_DPFLAGS Flags, int VertexCount)) {
       return CP_CALL_METHOD_PTR(this, s_VTable.GetDrawPrimitiveStructure, Flags, VertexCount);
    }

    CP_DECLARE_METHOD_HOOK(CKWORD *, GetDrawPrimitiveIndices, (int IndicesCount)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetDrawPrimitiveIndices, IndicesCount);
    }

    CP_DECLARE_METHOD_HOOK(void, Transform, (VxVector *Dest, VxVector *Src, CK3dEntity *Ref)) {
        CP_CALL_METHOD_PTR(this, s_VTable.Transform, Dest, Src, Ref);
    }

    CP_DECLARE_METHOD_HOOK(void, TransformVertices, (int VertexCount, VxTransformData *data, CK3dEntity *Ref)) {
        CP_CALL_METHOD_PTR(this, s_VTable.TransformVertices, VertexCount, data, Ref);
    }

    CP_DECLARE_METHOD_HOOK(CKERROR, GoFullScreen, (int Width, int Height, int Bpp, int Driver, int RefreshRate)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GoFullScreen, Width, Height, Bpp, Driver, RefreshRate);
    }

    CP_DECLARE_METHOD_HOOK(CKERROR, StopFullScreen, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.StopFullScreen);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsFullScreen, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.IsFullScreen);
    }

    CP_DECLARE_METHOD_HOOK(int, GetDriverIndex, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetDriverIndex);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, ChangeDriver, (int NewDriver)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.ChangeDriver, NewDriver);
    }

    CP_DECLARE_METHOD_HOOK(WIN_HANDLE, GetWindowHandle, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetWindowHandle);
    }

    CP_DECLARE_METHOD_HOOK(void, ScreenToClient, (Vx2DVector *ioPoint)) {
        CP_CALL_METHOD_PTR(this, s_VTable.ScreenToClient, ioPoint);
    }

    CP_DECLARE_METHOD_HOOK(void, ClientToScreen, (Vx2DVector *ioPoint)) {
        CP_CALL_METHOD_PTR(this, s_VTable.ClientToScreen, ioPoint);

    }

    CP_DECLARE_METHOD_HOOK(CKERROR, SetWindowRect, (VxRect &rect, CKDWORD Flags)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.SetWindowRect, rect, Flags);
    }

    CP_DECLARE_METHOD_HOOK(void, GetWindowRect, (VxRect &rect, CKBOOL ScreenRelative)) {
        CP_CALL_METHOD_PTR(this, s_VTable.GetWindowRect, rect, ScreenRelative);
    }

    CP_DECLARE_METHOD_HOOK(int, GetHeight, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetHeight);
    }

    CP_DECLARE_METHOD_HOOK(int, GetWidth, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetWidth);
    }

    CP_DECLARE_METHOD_HOOK(CKERROR, Resize, (int PosX, int PosY, int SizeX, int SizeY, CKDWORD Flags)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.Resize, PosX, PosY, SizeX, SizeY, Flags);
    }

    CP_DECLARE_METHOD_HOOK(void, SetViewRect, (VxRect &rect)) {
        CP_CALL_METHOD_PTR(this, s_VTable.SetViewRect, rect);
    }

    CP_DECLARE_METHOD_HOOK(void, GetViewRect, (VxRect &rect)) {
        CP_CALL_METHOD_PTR(this, s_VTable.GetViewRect, rect);
    }

    CP_DECLARE_METHOD_HOOK(VX_PIXELFORMAT, GetPixelFormat, (int *Bpp, int *Zbpp, int *StencilBpp)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetPixelFormat, Bpp, Zbpp, StencilBpp);
    }

    CP_DECLARE_METHOD_HOOK(void, SetState, (VXRENDERSTATETYPE State, CKDWORD Value)) {
        CP_CALL_METHOD_PTR(this, s_VTable.SetState, State, Value);
    }

    CP_DECLARE_METHOD_HOOK(CKDWORD, GetState, (VXRENDERSTATETYPE State)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetState, State);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, SetTexture, (CKTexture *tex, CKBOOL Clamped, int Stage)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.SetTexture, tex, Clamped, Stage);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, SetTextureStageState, (CKRST_TEXTURESTAGESTATETYPE State, CKDWORD Value, int Stage)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.SetTextureStageState, State, Value, Stage);
    }

    CP_DECLARE_METHOD_HOOK(CKRasterizerContext *, GetRasterizerContext, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetRasterizerContext);
    }

    CP_DECLARE_METHOD_HOOK(void, SetClearBackground, (CKBOOL ClearBack)) {
        CP_CALL_METHOD_PTR(this, s_VTable.SetClearBackground, ClearBack);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, GetClearBackground, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetClearBackground);
    }

    CP_DECLARE_METHOD_HOOK(void, SetClearZBuffer, (CKBOOL ClearZ)) {
        CP_CALL_METHOD_PTR(this, s_VTable.SetClearZBuffer, ClearZ);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, GetClearZBuffer, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetClearZBuffer);
    }

    CP_DECLARE_METHOD_HOOK(void, GetGlobalRenderMode, (VxShadeType *Shading, CKBOOL *Texture, CKBOOL *Wireframe)) {
        CP_CALL_METHOD_PTR(this, s_VTable.GetGlobalRenderMode, Shading, Texture, Wireframe);
    }

    CP_DECLARE_METHOD_HOOK(void, SetGlobalRenderMode, (VxShadeType Shading, CKBOOL Texture, CKBOOL Wireframe)) {
        CP_CALL_METHOD_PTR(this, s_VTable.SetGlobalRenderMode, Shading, Texture, Wireframe);
    }

    CP_DECLARE_METHOD_HOOK(void, SetCurrentRenderOptions, (CKDWORD flags)) {
        CP_CALL_METHOD_PTR(this, s_VTable.SetCurrentRenderOptions, flags);
    }

    CP_DECLARE_METHOD_HOOK(CKDWORD, GetCurrentRenderOptions, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetCurrentRenderOptions);
    }

    CP_DECLARE_METHOD_HOOK(void, ChangeCurrentRenderOptions, (CKDWORD Add, CKDWORD Remove)) {
        CP_CALL_METHOD_PTR(this, s_VTable.ChangeCurrentRenderOptions, Add, Remove);
    }

    CP_DECLARE_METHOD_HOOK(void, SetCurrentExtents, (VxRect &extents)) {
        CP_CALL_METHOD_PTR(this, s_VTable.SetCurrentExtents, extents);
    }

    CP_DECLARE_METHOD_HOOK(void, GetCurrentExtents, (VxRect &extents)) {
        CP_CALL_METHOD_PTR(this, s_VTable.GetCurrentExtents, extents);
    }

    CP_DECLARE_METHOD_HOOK(void, SetAmbientLightRGB, (float R, float G, float B)) {
        CP_CALL_METHOD_PTR(this, s_VTable.SetAmbientLightRGB, R, G, B);
    }

    CP_DECLARE_METHOD_HOOK(void, SetAmbientLight, (CKDWORD Color)) {
        CP_CALL_METHOD_PTR(this, s_VTable.SetAmbientLight, Color);
    }

    CP_DECLARE_METHOD_HOOK(CKDWORD, GetAmbientLight, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetAmbientLight);
    }

    CP_DECLARE_METHOD_HOOK(void, SetFogMode, (VXFOG_MODE Mode)) {
        CP_CALL_METHOD_PTR(this, s_VTable.SetFogMode, Mode);
    }

    CP_DECLARE_METHOD_HOOK(void, SetFogStart, (float Start)) {
        CP_CALL_METHOD_PTR(this, s_VTable.SetFogStart, Start);
    }

    CP_DECLARE_METHOD_HOOK(void, SetFogEnd, (float End)) {
        CP_CALL_METHOD_PTR(this, s_VTable.SetFogEnd, End);
    }

    CP_DECLARE_METHOD_HOOK(void, SetFogDensity, (float Density)) {
        CP_CALL_METHOD_PTR(this, s_VTable.SetFogDensity, Density);
    }

    CP_DECLARE_METHOD_HOOK(void, SetFogColor, (CKDWORD Color)) {
        CP_CALL_METHOD_PTR(this, s_VTable.SetFogColor, Color);
    }

    CP_DECLARE_METHOD_HOOK(VXFOG_MODE, GetFogMode, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetFogMode);
    }

    CP_DECLARE_METHOD_HOOK(float, GetFogStart, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetFogStart);
    }

    CP_DECLARE_METHOD_HOOK(float, GetFogEnd, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetFogEnd);
    }

    CP_DECLARE_METHOD_HOOK(float, GetFogDensity, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetFogDensity);
    }

    CP_DECLARE_METHOD_HOOK(CKDWORD, GetFogColor, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetFogColor);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, DrawPrimitive, (VXPRIMITIVETYPE pType, CKWORD *indices, int indexcount, VxDrawPrimitiveData *data)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.DrawPrimitive, pType, indices, indexcount, data);
    }

    CP_DECLARE_METHOD_HOOK(void, SetWorldTransformationMatrix, (const VxMatrix &M)) {
        CP_CALL_METHOD_PTR(this, s_VTable.SetWorldTransformationMatrix, M);
    }

    CP_DECLARE_METHOD_HOOK(void, SetProjectionTransformationMatrix, (const VxMatrix &M)) {
        CP_CALL_METHOD_PTR(this, s_VTable.SetProjectionTransformationMatrix, M);
    }

    CP_DECLARE_METHOD_HOOK(void, SetViewTransformationMatrix, (const VxMatrix &M)) {
        CP_CALL_METHOD_PTR(this, s_VTable.SetViewTransformationMatrix, M);
    }

    CP_DECLARE_METHOD_HOOK(const VxMatrix &, GetWorldTransformationMatrix, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetWorldTransformationMatrix);
    }

    CP_DECLARE_METHOD_HOOK(const VxMatrix &, GetProjectionTransformationMatrix, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetProjectionTransformationMatrix);
    }

    CP_DECLARE_METHOD_HOOK(const VxMatrix &, GetViewTransformationMatrix, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetViewTransformationMatrix);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, SetUserClipPlane, (CKDWORD ClipPlaneIndex, const VxPlane &PlaneEquation)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.SetUserClipPlane, ClipPlaneIndex, PlaneEquation);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, GetUserClipPlane, (CKDWORD ClipPlaneIndex, VxPlane &PlaneEquation)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetUserClipPlane, ClipPlaneIndex, PlaneEquation);
    }

    CP_DECLARE_METHOD_HOOK(CKRenderObject *, Pick, (int x, int y, CKPICKRESULT *oRes, CKBOOL iIgnoreUnpickable)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.Pick, x, y, oRes, iIgnoreUnpickable);
    }

    CP_DECLARE_METHOD_HOOK(CKRenderObject *, PointPick, (CKPOINT pt, CKPICKRESULT *oRes, CKBOOL iIgnoreUnpickable)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.PointPick, pt, oRes, iIgnoreUnpickable);
    }

    CP_DECLARE_METHOD_HOOK(CKERROR,  RectPick, (const VxRect &r, XObjectPointerArray &oObjects, CKBOOL Intersect)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.RectPick, r, oObjects, Intersect);
    }

    CP_DECLARE_METHOD_HOOK(void, AttachViewpointToCamera, (CKCamera *cam)) {
        CP_CALL_METHOD_PTR(this, s_VTable.AttachViewpointToCamera, cam);
    }

    CP_DECLARE_METHOD_HOOK(void, DetachViewpointFromCamera, ()) {
        CP_CALL_METHOD_PTR(this, s_VTable.DetachViewpointFromCamera);
    }

    CP_DECLARE_METHOD_HOOK(CKCamera *, GetAttachedCamera, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetAttachedCamera);
    }

    CP_DECLARE_METHOD_HOOK(CK3dEntity *, GetViewpoint, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetViewpoint);
    }

    CP_DECLARE_METHOD_HOOK(CKMaterial *, GetBackgroundMaterial, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetBackgroundMaterial);
    }

    CP_DECLARE_METHOD_HOOK(void, GetBoundingBox, (VxBbox *BBox)) {
        CP_CALL_METHOD_PTR(this, s_VTable.GetBoundingBox, BBox);
    }

    CP_DECLARE_METHOD_HOOK(void, GetStats, (VxStats *stats)) {
        CP_CALL_METHOD_PTR(this, s_VTable.GetStats, stats);
    }

    CP_DECLARE_METHOD_HOOK(void, SetCurrentMaterial, (CKMaterial *mat, CKBOOL Lit)) {
        CP_CALL_METHOD_PTR(this, s_VTable.SetCurrentMaterial, mat, Lit);
    }

    CP_DECLARE_METHOD_HOOK(void, Activate, (CKBOOL active)) {
        CP_CALL_METHOD_PTR(this, s_VTable.Activate, active);
    }

    CP_DECLARE_METHOD_HOOK(int, DumpToMemory, (const VxRect *iRect, VXBUFFER_TYPE buffer, VxImageDescEx &desc)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.DumpToMemory, iRect, buffer, desc);
    }

    CP_DECLARE_METHOD_HOOK(int, CopyToVideo, (const VxRect *iRect, VXBUFFER_TYPE buffer, VxImageDescEx &desc)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.CopyToVideo, iRect, buffer, desc);
    }

    CP_DECLARE_METHOD_HOOK(CKERROR, DumpToFile, (CKSTRING filename, const VxRect *rect, VXBUFFER_TYPE buffer)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.DumpToFile, filename, rect, buffer);
    }

    CP_DECLARE_METHOD_HOOK(VxDirectXData *, GetDirectXInfo, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetDirectXInfo);
    }

    CP_DECLARE_METHOD_HOOK(void, WarnEnterThread, ()) {
        CP_CALL_METHOD_PTR(this, s_VTable.WarnEnterThread);
    }

    CP_DECLARE_METHOD_HOOK(void, WarnExitThread, ()) {
        CP_CALL_METHOD_PTR(this, s_VTable.WarnExitThread);
    }

    CP_DECLARE_METHOD_HOOK(CK2dEntity *, Pick2D, (const Vx2DVector &v)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.Pick2D, v);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, SetRenderTarget, (CKTexture *texture, int CubeMapFace)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.SetRenderTarget, texture, CubeMapFace);
    }

    CP_DECLARE_METHOD_HOOK(void, AddRemoveSequence, (CKBOOL Start)) {
        CP_CALL_METHOD_PTR(this, s_VTable.AddRemoveSequence, Start);
    }

    CP_DECLARE_METHOD_HOOK(void, SetTransparentMode, (CKBOOL Trans)) {
        CP_CALL_METHOD_PTR(this, s_VTable.SetTransparentMode, Trans);
    }

    CP_DECLARE_METHOD_HOOK(void, AddDirtyRect, (CKRECT *Rect)) {
        CP_CALL_METHOD_PTR(this, s_VTable.AddDirtyRect, Rect);
    }

    CP_DECLARE_METHOD_HOOK(void, RestoreScreenBackup, ()) {
        CP_CALL_METHOD_PTR(this, s_VTable.RestoreScreenBackup);
    }

    CP_DECLARE_METHOD_HOOK(CKDWORD, GetStencilFreeMask, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetStencilFreeMask);
    }

    CP_DECLARE_METHOD_HOOK(void, UsedStencilBits, (CKDWORD stencilBits)) {
        CP_CALL_METHOD_PTR(this, s_VTable.UsedStencilBits, stencilBits);
    }

    CP_DECLARE_METHOD_HOOK(int, GetFirstFreeStencilBits, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.GetFirstFreeStencilBits);
    }

    CP_DECLARE_METHOD_HOOK(VxDrawPrimitiveData *, LockCurrentVB, (CKDWORD VertexCount)) {
        return CP_CALL_METHOD_PTR(this, s_VTable.LockCurrentVB, VertexCount);
    }

    CP_DECLARE_METHOD_HOOK(CKBOOL, ReleaseCurrentVB, ()) {
        return CP_CALL_METHOD_PTR(this, s_VTable.ReleaseCurrentVB);
    }

    CP_DECLARE_METHOD_HOOK(void, SetTextureMatrix, (const VxMatrix &M, int Stage)) {
        CP_CALL_METHOD_PTR(this, s_VTable.SetTextureMatrix, M, Stage);
    }

    CP_DECLARE_METHOD_HOOK(void, SetStereoParameters, (float EyeSeparation, float FocalLength)) {
        CP_CALL_METHOD_PTR(this, s_VTable.SetStereoParameters, EyeSeparation, FocalLength);
    }

    CP_DECLARE_METHOD_HOOK(void, GetStereoParameters, (float &EyeSeparation, float &FocalLength)) {
        CP_CALL_METHOD_PTR(this, s_VTable.GetStereoParameters, EyeSeparation, FocalLength);
    }

    static void Hook(CKRenderContext *rc) {
        if (!rc)
            return;

        utils::LoadVTable<CP_CLASS_VTABLE_NAME(CKRenderContext)<CKRenderContext>>(rc, s_VTable);

#define HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(Instance, Name) \
    utils::HookVirtualMethod(Instance, &RenderContextHook::CP_FUNC_HOOK_NAME(Name), (offsetof(CP_CLASS_VTABLE_NAME(CKRenderContext)<CKRenderContext>, Name) / sizeof(void*)))

        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, AddObject);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, RemoveObject);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, IsObjectAttached);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, Compute3dRootObjects);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, Compute2dRootObjects);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, Get2dRoot);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, DetachAll);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, ForceCameraSettingsUpdate);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, PrepareCameras);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, Clear);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, DrawScene);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, BackToFront);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, Render);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, AddPreRenderCallBack);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, RemovePreRenderCallBack);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, AddPostRenderCallBack);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, RemovePostRenderCallBack);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, AddPostSpriteRenderCallBack);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, RemovePostSpriteRenderCallBack);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetDrawPrimitiveStructure);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetDrawPrimitiveIndices);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, Transform);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, TransformVertices);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GoFullScreen);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, StopFullScreen);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, IsFullScreen);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetDriverIndex);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, ChangeDriver);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetWindowHandle);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, ScreenToClient);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, ClientToScreen);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetWindowRect);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetWindowRect);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetHeight);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetWidth);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, Resize);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetViewRect);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetViewRect);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetPixelFormat);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetState);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetState);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetTexture);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetTextureStageState);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetRasterizerContext);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetClearBackground);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetClearBackground);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetClearZBuffer);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetClearZBuffer);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetGlobalRenderMode);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetGlobalRenderMode);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetCurrentRenderOptions);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetCurrentRenderOptions);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, ChangeCurrentRenderOptions);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetCurrentExtents);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetCurrentExtents);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetAmbientLightRGB);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetAmbientLight);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetAmbientLight);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetFogMode);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetFogStart);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetFogEnd);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetFogDensity);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetFogColor);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetFogMode);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetFogStart);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetFogEnd);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetFogDensity);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetFogColor);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, DrawPrimitive);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetWorldTransformationMatrix);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetProjectionTransformationMatrix);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetViewTransformationMatrix);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetWorldTransformationMatrix);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetProjectionTransformationMatrix);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetViewTransformationMatrix);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetUserClipPlane);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetUserClipPlane);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, Pick);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, PointPick);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, RectPick);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, AttachViewpointToCamera);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, DetachViewpointFromCamera);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetAttachedCamera);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetViewpoint);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetBackgroundMaterial);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetBoundingBox);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetStats);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetCurrentMaterial);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, Activate);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, DumpToMemory);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, CopyToVideo);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, DumpToFile);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetDirectXInfo);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, WarnEnterThread);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, WarnExitThread);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, Pick2D);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetRenderTarget);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, AddRemoveSequence);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetTransparentMode);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, AddDirtyRect);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, RestoreScreenBackup);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetStencilFreeMask);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, UsedStencilBits);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetFirstFreeStencilBits);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, LockCurrentVB);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, ReleaseCurrentVB);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetTextureMatrix);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetStereoParameters);
        HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetStereoParameters);

#undef HOOK_RENDER_CONTEXT_VIRTUAL_METHOD
    }

    static void Unhook(CKRenderContext *rc) {
        if (rc)
            utils::SaveVTable<CP_CLASS_VTABLE_NAME(CKRenderContext)<CKRenderContext>>(rc, s_VTable);
    }
};

bool RenderContextHook::s_DisableRender = false;
CP_CLASS_VTABLE_NAME(CKRenderContext)<CKRenderContext> RenderContextHook::s_VTable = {};

namespace RenderHook {
    void HookRenderContext(CKRenderContext *rc) {
        RenderContextHook::Hook(rc);
    }

    void UnhookRenderContext(CKRenderContext *rc) {
        RenderContextHook::Unhook(rc);
    }

    void DisableRender(bool disable) {
        RenderContextHook::s_DisableRender = disable;
    }
}
