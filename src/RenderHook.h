#ifndef BML_RENDERHOOK_H
#define BML_RENDERHOOK_H

#include "CKRenderContext.h"
#include "CKRasterizer.h"

#include "VTables.h"

class CKRenderedScene;
class CKSceneGraphNode;

struct VxCallBack {
    void *callback;
    void *argument;
    CKBOOL temp;
};

struct VxOption {
    CKDWORD Value;
    XString Key;
};

class CKCallbacksContainer {
public:
    XClassArray<VxCallBack> m_PreCallBacks;
    VxCallBack *m_OnCallBack;
    XClassArray<VxCallBack> m_PostCallBacks;
};

struct UserDrawPrimitiveDataClass : VxDrawPrimitiveData {
    CKDWORD UserData[29];
};

struct CKRenderContextSettings {
    CKRECT m_Rect;
    int m_Bpp;
    int m_Zbpp;
    int m_StencilBpp;
};

struct CKObjectExtents {
    VxRect m_Rect;
    CK_RENDER_FLAGS m_Flags = CK_RENDER_USECURRENTSETTINGS;
    CK_ID m_Camera = 0;
};

struct CKTransparentObject {
    CKSceneGraphNode *m_Node = nullptr;
    float m_ZhMin = 0.0f;
    float m_ZhMax = 0.0f;
};

class CP_HOOK_CLASS_NAME(CKRenderContext) : public CKRenderContext {
public:
    CP_DECLARE_METHOD_HOOK(CKERROR, Render, (CK_RENDER_FLAGS Flags));

    CKBOOL UpdateProjection(CKBOOL force);

    CP_DECLARE_METHOD_PTRS(CKRenderContext, CKBOOL, UpdateProjection, (CKBOOL force));

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
    CKTexture *m_TargetTexture;
    CKRST_CUBEFACE m_CubeMapFace;
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

namespace RenderHook {
    bool HookRenderEngine();
    bool UnhookRenderEngine();

    void DisableRender(bool disable);
    void EnableWidescreenFix(bool enable);
}

#endif // BML_RENDERHOOK_H
