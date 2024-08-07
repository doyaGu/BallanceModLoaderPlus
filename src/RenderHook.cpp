#include "RenderHook.h"

#include "CKLevel.h"
#include "CKTimeManager.h"
#include "CKAttributeManager.h"
#include "CKParameterOut.h"
#include "CKRasterizer.h"

#include <MinHook.h>

#include "HookUtils.h"
#include "VTables.h"

#define CP_ADD_METHOD_HOOK(Name, Base, Offset) \
        { CP_FUNC_TARGET_PTR_NAME(Name) = utils::ForceReinterpretCast<CP_FUNC_TYPE_NAME(Name)>(Base, Offset); } \
        if ((MH_CreateHook(*reinterpret_cast<LPVOID *>(&CP_FUNC_TARGET_PTR_NAME(Name)), \
                           *reinterpret_cast<LPVOID *>(&CP_FUNC_PTR_NAME(Name)), \
                            reinterpret_cast<LPVOID *>(&CP_FUNC_ORIG_PTR_NAME(Name))) != MH_OK || \
            MH_EnableHook(*reinterpret_cast<LPVOID *>(&CP_FUNC_TARGET_PTR_NAME(Name))) != MH_OK)) \
                return false;

#define CP_REMOVE_METHOD_HOOK(Name) \
    MH_DisableHook(*reinterpret_cast<void **>(&CP_FUNC_TARGET_PTR_NAME(Name))); \
    MH_RemoveHook(*reinterpret_cast<void **>(&CP_FUNC_TARGET_PTR_NAME(Name)));

static CKBOOL *g_UpdateTransparency = nullptr;
static CKDWORD *g_FogProjectionType = nullptr;

static void (*g_UpdateDriverDescCapsFunc)(VxDriverDesc2 *desc) = nullptr;

void UpdateDriverDescCaps(VxDriverDesc2 *desc) {
    g_UpdateDriverDescCapsFunc(desc);
}

CKRenderContextSettings::CKRenderContextSettings(CKRasterizerContext *rst) {
    if (rst) {
        *this = *(CKRenderContextSettings *)&rst->m_PosX;
    } else {
        m_Rect = {};
        m_Bpp = 0;
        m_Zbpp = 0;
        m_StencilBpp = 0;
    }
}

// CKRenderedScene

CP_DEFINE_METHOD_PTRS(CKRenderedScene, Draw);
CP_DEFINE_METHOD_PTRS(CKRenderedScene, SetDefaultRenderStates);
CP_DEFINE_METHOD_PTRS(CKRenderedScene, SetupLights);
CP_DEFINE_METHOD_PTRS(CKRenderedScene, PrepareCameras);
CP_DEFINE_METHOD_PTRS(CKRenderedScene, ForceCameraSettingsUpdate);
CP_DEFINE_METHOD_PTRS(CKRenderedScene, UpdateViewportSize);
CP_DEFINE_METHOD_PTRS(CKRenderedScene, ResizeViewport);
CP_DEFINE_METHOD_PTRS(CKRenderedScene, AddObject);
CP_DEFINE_METHOD_PTRS(CKRenderedScene, RemoveObject);
CP_DEFINE_METHOD_PTRS(CKRenderedScene, DetachAll);

CKRenderedScene::CKRenderedScene(CKRenderContext *rc) {
    m_RenderContext = rc;
    m_Context = rc->GetCKContext();
    m_AmbientLight = 0xFF0F0F0F;
    m_FogMode = 0;
    m_FogStart = 1.0f;
    m_FogEnd = 100.0f;
    m_FogColor = 0;
    m_FogDensity = 1.0f;
    m_LightCount = 0;
    m_BackgroundMaterial = nullptr;
    m_RootEntity = nullptr;
    m_AttachedCamera = nullptr;

    m_RootEntity = (CK3dEntity *) m_Context->CreateObject(CKCID_3DENTITY);
    m_RootEntity->ModifyObjectFlags(CK_OBJECT_NOTTOBELISTEDANDSAVED, 0);
    AddObject(m_RootEntity);

    m_BackgroundMaterial = (CKMaterial *) m_Context->CreateObject(CKCID_MATERIAL, (CKSTRING) "Background Material");
    m_BackgroundMaterial->ModifyObjectFlags(CK_OBJECT_NOTTOBELISTEDANDSAVED, 0);
    m_BackgroundMaterial->SetDiffuse(VxColor(0.0f, 0.0f, 0.0f));
    m_BackgroundMaterial->SetAmbient(VxColor(0.0f, 0.0f, 0.0f));
    m_BackgroundMaterial->SetSpecular(VxColor(0.0f, 0.0f, 0.0f));
    m_BackgroundMaterial->SetEmissive(VxColor(0.0f, 0.0f, 0.0f));
}

CKRenderedScene::~CKRenderedScene() {
    if (!m_Context->IsInClearAll()) {
        CKDestroyObject(m_BackgroundMaterial);
        CKDestroyObject(m_RootEntity);
    }
}

CKERROR CKRenderedScene::Draw(CK_RENDER_FLAGS Flags) {
//    return CP_CALL_METHOD_ORIG(Draw, Flags);

    auto *const dev = (CP_HOOK_CLASS_NAME(CKRenderContext) *) m_RenderContext;
    auto *const rm = (CP_HOOK_CLASS_NAME(CKRenderManager) *) dev->m_RenderManager;
    auto *const rst = dev->m_RasterizerContext;

    SetDefaultRenderStates(rst);

    dev->m_SpriteTimeProfiler.Reset();
    if ((Flags & CK_RENDER_BACKGROUNDSPRITES) != 0 && rm->m_2DRootBack->GetChildrenCount()) {
        VxRect rect;
        dev->GetViewRect(rect);
        dev->SetFullViewport(rst->m_ViewportData, dev->m_Settings.m_Rect.right, dev->m_Settings.m_Rect.bottom);
        rst->SetViewport(&rst->m_ViewportData);
        rm->m_2DRootBack->Render(m_RenderContext);
        ResizeViewport(rect);
    }
    dev->m_Stats.SpriteTime += dev->m_SpriteTimeProfiler.Current();

    auto *rootEntity = (CP_HOOK_CLASS_NAME(CK3dEntity) *) m_RootEntity;

    if ((Flags & CK_RENDER_SKIP3D) == 0) {
        if (dev->m_Camera) {
            auto *camera = (CP_HOOK_CLASS_NAME(CKCamera) *) dev->m_Camera;
            rootEntity->m_WorldMatrix = camera->m_WorldMatrix;
            rootEntity->WorldMatrixChanged(FALSE, TRUE);
            VxVector vec;
            camera->InverseTransform(&vec, &rootEntity->m_WorldMatrix[3]);
            auto z = dev->m_NearPlane / fabsf(vec.z);
            float right = (camera->m_LocalBoundingBox.Max.x - vec.x) * z;
            float left = (camera->m_LocalBoundingBox.Min.x - vec.x) * z;
            float top = (camera->m_LocalBoundingBox.Max.y - vec.y) * z;
            float bottom = (camera->m_LocalBoundingBox.Min.y - vec.y) * z;
            dev->m_ProjectionMatrix.PerspectiveRect(left, right, top, bottom, dev->m_NearPlane, dev->m_FarPlane);
            rst->SetTransformMatrix(VXMATRIX_PROJECTION, dev->m_ProjectionMatrix);
            rst->SetViewport(&dev->m_ViewportData);

            VxRect rect(0.0f, 0.0f, (float) dev->m_Settings.m_Rect.right, (float) dev->m_Settings.m_Rect.bottom);
            auto *background = dev->Get2dRoot(TRUE);
            background->SetRect(rect);
            auto *foreground = dev->Get2dRoot(FALSE);
            foreground->SetRect(rect);
        }

        const VxVector4 &origin = rootEntity->m_WorldMatrix[3];
        const VxVector4 &right = rootEntity->m_WorldMatrix[0];
        const VxVector4 &up = rootEntity->m_WorldMatrix[1];
        const VxVector4 &dir = rootEntity->m_WorldMatrix[2];
        const auto aspectRatio = (float) ((double) dev->m_ViewportData.ViewWidth / (double) dev->m_ViewportData.ViewHeight);
        dev->m_Frustum = VxFrustum(origin, right, up, dir, dev->m_NearPlane, dev->m_FarPlane, dev->m_Fov, aspectRatio);

        rst->SetTransformMatrix(VXMATRIX_WORLD, VxMatrix::Identity());
        rst->SetTransformMatrix(VXMATRIX_VIEW, rootEntity->GetInverseWorldMatrix());

        SetupLights(rst);

        if (!dev->m_Camera)
            dev->UpdateProjection(FALSE);

        dev->m_DevicePreCallbacksTimeProfiler.Reset();
        auto &devicePreCallbacks = dev->m_PreRenderCallBacks.m_PreCallBacks;
        for (auto it = devicePreCallbacks.Begin(); it != devicePreCallbacks.End(); ++it) {
            auto &cb = *it;
            ((CK_RENDERCALLBACK) cb.callback)(dev, cb.argument);
        }
        dev->m_Stats.DevicePreCallbacks += dev->m_DevicePreCallbacksTimeProfiler.Current();

        rst->SetVertexShader(0);
        m_Context->ExecuteManagersOnPreRender(dev);

        CK_RENDER_FLAGS flags = CK_RENDER_DEFAULTSETTINGS;
        if ((Flags & CK_RENDER_DONOTUPDATEEXTENTS) != 0)
            flags = CK_RENDER_CLEARVIEWPORT;

        dev->m_ObjectsRenderTimeProfiler.Reset();

        dev->m_TransparentObjects.Resize(0);
        dev->m_Stats.SceneTraversalTime = 0.0f;
        dev->m_SceneTraversalTimeProfiler.Reset();

        rm->m_SceneGraphRootNode.RenderTransparents(dev, flags);

        dev->m_Stats.SceneTraversalTime += dev->m_SceneTraversalTimeProfiler.Current();
        dev->CallSprite3DBatches();

        dev->m_DevicePostCallbacksTimeProfiler.Reset();
        auto &devicePostCallbacks = dev->m_PreRenderTempCallBacks.m_PostCallBacks;
        for (auto it = devicePostCallbacks.Begin(); it != devicePostCallbacks.End(); ++it) {
            auto &cb = *it;
            ((CK_RENDERCALLBACK) cb.callback)(dev, cb.argument);
        }
        dev->m_Stats.DevicePostCallbacks += dev->m_DevicePostCallbacksTimeProfiler.Current();

        dev->m_SortTransparentObjects = TRUE;
        rm->m_SceneGraphRootNode.SortTransparentObjects(dev, flags);
        dev->CallSprite3DBatches();
        dev->m_SortTransparentObjects = FALSE;

        dev->m_Stats.ObjectsRenderTime = dev->m_ObjectsRenderTimeProfiler.Current();

        dev->m_DevicePostCallbacksTimeProfiler.Reset();
        rst->SetVertexShader(0);
        auto &postCallbacks = dev->m_PreRenderCallBacks.m_PostCallBacks;
        for (auto it = postCallbacks.Begin(); it != postCallbacks.End(); ++it) {
            auto &cb = *it;
            ((CK_RENDERCALLBACK) cb.callback)(dev, cb.argument);
        }
        dev->m_Stats.DevicePostCallbacks += dev->m_DevicePostCallbacksTimeProfiler.Current();

        m_Context->ExecuteManagersOnPostRender(dev);
    }

    dev->m_SpriteTimeProfiler.Reset();
    if ((Flags & CK_RENDER_FOREGROUNDSPRITES) != 0 && rm->m_2DRootFore->GetChildrenCount() != 0) {
        VxRect rect;
        dev->GetViewRect(rect);
        dev->SetFullViewport(rst->m_ViewportData, dev->m_Settings.m_Rect.right, dev->m_Settings.m_Rect.bottom);
        rst->SetViewport(&rst->m_ViewportData);
        rm->m_2DRootFore->Render(m_RenderContext);
        ResizeViewport(rect);
    }
    dev->m_Stats.SpriteTime += dev->m_SpriteTimeProfiler.Current();

    rst->SetVertexShader(0);
    m_Context->ExecuteManagersOnPostSpriteRender(dev);

    dev->m_DevicePostCallbacksTimeProfiler.Reset();
    rst->SetVertexShader(0);
    auto &postCallbacks = dev->m_PostRenderCallBacks.m_PostCallBacks;
    for (auto it = postCallbacks.Begin(); it != postCallbacks.End(); ++it) {
        auto &cb = *it;
        ((CK_RENDERCALLBACK) cb.callback)(dev, cb.argument);
    }
    dev->m_Stats.DevicePostCallbacks += dev->m_DevicePostCallbacksTimeProfiler.Current();

    dev->m_Stats.ObjectsRenderTime = dev->m_Stats.ObjectsRenderTime -
                                     (dev->m_Stats.SceneTraversalTime +
                                      dev->m_Stats.TransparentObjectsSortTime +
                                      dev->m_Stats.ObjectsCallbacksTime + dev->m_Stats.SkinTime);
    dev->m_Stats.SpriteTime = dev->m_Stats.SpriteTime - dev->m_Stats.SpriteCallbacksTime;

    *g_UpdateTransparency = FALSE;
    return CK_OK;
}

void CKRenderedScene::SetDefaultRenderStates(CKRasterizerContext *rst) {
//    CP_CALL_METHOD_ORIG(SetDefaultRenderStates, rst);

    auto *const dev = (CP_HOOK_CLASS_NAME(CKRenderContext) *) m_RenderContext;
    auto *const rm = (CP_HOOK_CLASS_NAME(CKRenderManager) *) dev->m_RenderManager;

    rst->InvalidateStateCache(VXRENDERSTATE_FOGVERTEXMODE);
    rst->InvalidateStateCache(VXRENDERSTATE_FOGENABLE);
    rst->InvalidateStateCache(VXRENDERSTATE_FOGPIXELMODE);
    rst->SetRenderState(VXRENDERSTATE_FOGENABLE, m_FogMode != VXFOG_NONE);

    if (m_FogMode != VXFOG_NONE) {
        if (rm->m_ForceLinearFog.Value != 0)
            m_FogMode = VXFOG_LINEAR;

        if ((dev->m_RasterizerDriver->m_3DCaps.RasterCaps & (CKRST_RASTERCAPS_FOGRANGE | CKRST_RASTERCAPS_FOGPIXEL)) ==
            (CKRST_RASTERCAPS_FOGRANGE | CKRST_RASTERCAPS_FOGPIXEL)) {
            rst->SetRenderState(VXRENDERSTATE_FOGPIXELMODE, m_FogMode);
        } else {
            m_FogMode = VXFOG_LINEAR;
            rst->SetRenderState(VXRENDERSTATE_FOGVERTEXMODE, m_FogMode);
        }

        auto &proj = rst->m_ProjectionMatrix;
        float v17 = proj[2][2] * m_FogEnd + proj[3][2];
        float v15 = proj[2][3] * m_FogEnd + proj[3][3];
        float v21 = proj[2][3] * m_FogStart + proj[3][3];
        float v19 = proj[2][2] * m_FogStart + proj[3][2];
        float v16 = 1.0f / v15;
        float v18 = v17 * v16;
        float v22 = 1.0f / v21;
        float v20 = v19 * v22;

        if (*g_FogProjectionType == 0) {
            rst->SetRenderState(VXRENDERSTATE_FOGEND, *reinterpret_cast<CKDWORD *>(&m_FogEnd));
            rst->SetRenderState(VXRENDERSTATE_FOGSTART, *reinterpret_cast<CKDWORD *>(&m_FogStart));
        } else if (*g_FogProjectionType == 1) {
            rst->SetRenderState(VXRENDERSTATE_FOGEND, *reinterpret_cast<CKDWORD *>(&v18));
            rst->SetRenderState(VXRENDERSTATE_FOGSTART, *reinterpret_cast<CKDWORD *>(&v20));
        } else if (*g_FogProjectionType == 2) {
            rst->SetRenderState(VXRENDERSTATE_FOGEND, *reinterpret_cast<CKDWORD *>(&v20));
            rst->SetRenderState(VXRENDERSTATE_FOGSTART, *reinterpret_cast<CKDWORD *>(&v22));
        }

        rst->SetRenderState(VXRENDERSTATE_FOGDENSITY, *reinterpret_cast<CKDWORD *>(&m_FogDensity));
        rst->SetRenderState(VXRENDERSTATE_FOGCOLOR, m_FogColor);
    }

    if (rm->m_DisableSpecular.Value != 0) {
        rst->SetRenderStateFlags(VXRENDERSTATE_SPECULARENABLE, FALSE);
        rst->SetRenderState(VXRENDERSTATE_SPECULARENABLE, FALSE);
        rst->SetRenderStateFlags(VXRENDERSTATE_SPECULARENABLE, TRUE);
    } else {
        rst->SetRenderStateFlags(VXRENDERSTATE_SPECULARENABLE, FALSE);
        rst->SetRenderState(VXRENDERSTATE_SPECULARENABLE, TRUE);
    }

    if (rm->m_DisableDithering.Value != 0) {
        rst->SetRenderStateFlags(VXRENDERSTATE_DITHERENABLE, FALSE);
        rst->SetRenderState(VXRENDERSTATE_DITHERENABLE, FALSE);
        rst->SetRenderStateFlags(VXRENDERSTATE_DITHERENABLE, TRUE);
    } else {
        rst->SetRenderStateFlags(VXRENDERSTATE_DITHERENABLE, FALSE);
        rst->InvalidateStateCache(VXRENDERSTATE_DITHERENABLE);
        rst->SetRenderState(VXRENDERSTATE_DITHERENABLE, TRUE);
    }

    if (rm->m_DisablePerspectiveCorrection.Value != 0) {
        rst->SetRenderStateFlags(VXRENDERSTATE_TEXTUREPERSPECTIVE, FALSE);
        rst->SetRenderState(VXRENDERSTATE_TEXTUREPERSPECTIVE, FALSE);
        rst->SetRenderStateFlags(VXRENDERSTATE_TEXTUREPERSPECTIVE, TRUE);
    } else {
        rst->SetRenderStateFlags(VXRENDERSTATE_TEXTUREPERSPECTIVE, FALSE);
        rst->SetRenderState(VXRENDERSTATE_TEXTUREPERSPECTIVE, TRUE);
    }

    rst->m_PresentInterval = rm->m_DisableFilter.Value;
    rst->m_CurrentPresentInterval = rm->m_DisableMipmap.Value;
    rst->SetRenderState(VXRENDERSTATE_NORMALIZENORMALS, TRUE);
    rst->SetRenderState(VXRENDERSTATE_ZENABLE, TRUE);
    rst->SetRenderState(VXRENDERSTATE_CULLMODE, VXCULL_CCW);
    rst->SetRenderState(VXRENDERSTATE_ZFUNC, VXCMP_LESSEQUAL);

    if (dev->m_Shading == 0) {
        rst->SetRenderStateFlags(VXRENDERSTATE_SHADEMODE, FALSE);
        rst->SetRenderStateFlags(VXRENDERSTATE_FILLMODE, FALSE);
        rst->SetRenderState(VXRENDERSTATE_FILLMODE, VXFILL_WIREFRAME);
        rst->SetRenderStateFlags(VXRENDERSTATE_FILLMODE, TRUE);
    } else if (dev->m_Shading == 1) {
        rst->SetRenderStateFlags(VXRENDERSTATE_FILLMODE, FALSE);
        rst->SetRenderStateFlags(VXRENDERSTATE_SHADEMODE, FALSE);
        rst->SetRenderState(VXRENDERSTATE_SHADEMODE, dev->m_Shading);
        rst->SetRenderStateFlags(VXRENDERSTATE_SHADEMODE, TRUE);
    } else if (dev->m_Shading == 2) {
        rst->SetRenderStateFlags(VXRENDERSTATE_SHADEMODE, FALSE);
        rst->SetRenderStateFlags(VXRENDERSTATE_FILLMODE, FALSE);
    }
}

void CKRenderedScene::SetupLights(CKRasterizerContext *rst) {
//    CP_CALL_METHOD_ORIG(SetupLights, rst);

    for (int i = 0; i < m_LightCount; ++i)
        rst->EnableLight(i, FALSE);

    m_LightCount = 0;
    for (auto it = m_Lights.Begin(); it != m_Lights.End(); ++it) {
        auto *light = (CP_HOOK_CLASS_NAME(CKLight) *) (*it);
        if (light->Setup(rst, m_LightCount))
            ++m_LightCount;
    }

    rst->SetRenderState(VXRENDERSTATE_AMBIENT, m_AmbientLight);
}

void CKRenderedScene::PrepareCameras(CK_RENDER_FLAGS Flags) {
//    CP_CALL_METHOD_ORIG(PrepareCameras, Flags);

    for (auto it = m_Cameras.Begin(); it != m_Cameras.End(); ++it) {
        CKCamera *camera = *it;
        if (camera && camera->GetClassID() == CKCID_TARGETCAMERA) {
            auto *target = camera->GetTarget();
            if (target) {
                VxVector o(0.0f);
                camera->LookAt(&o, target);
            }
        }
    }

    for (auto it = m_Lights.Begin(); it != m_Lights.End(); ++it) {
        CKLight *light = *it;
        if (light && light->GetClassID() == CKCID_TARGETLIGHT) {
            auto *target = light->GetTarget();
            if (target) {
                VxVector o(0.0f);
                light->LookAt(&o, target);
            }
        }
    }

    auto *const dev = (CP_HOOK_CLASS_NAME(CKRenderContext) *) m_RenderContext;
    auto *const attachedCamera = (CP_HOOK_CLASS_NAME(CKCamera) *) m_AttachedCamera;
    auto *const rootEntity = (CP_HOOK_CLASS_NAME(CK3dEntity) *) m_RootEntity;

    if (attachedCamera && rootEntity) {
        rootEntity->m_WorldMatrix = attachedCamera->m_WorldMatrix;
        rootEntity->WorldMatrixChanged(TRUE, FALSE);
        if (!attachedCamera->IsUpToDate()) {
            dev->m_Perspective = attachedCamera->GetProjectionType() == CK_PERSPECTIVEPROJECTION;
            dev->m_Zoom = attachedCamera->GetOrthographicZoom();
            dev->m_Fov = attachedCamera->GetFov();
            dev->m_NearPlane = attachedCamera->GetFrontPlane();
            dev->m_FarPlane = attachedCamera->GetBackPlane();
            if ((Flags & CK_RENDER_USECAMERARATIO) != 0)
                UpdateViewportSize(TRUE, Flags);
            else
                dev->UpdateProjection(TRUE);

            attachedCamera->ModifyObjectFlags(CK_OBJECT_UPTODATE, 0);
            return;
        }
    }

    dev->UpdateProjection(FALSE);
}

void CKRenderedScene::ForceCameraSettingsUpdate() {
//    CP_CALL_METHOD_ORIG(ForceCameraSettingsUpdate);

    auto *const dev = (CP_HOOK_CLASS_NAME(CKRenderContext) *) m_RenderContext;
    auto *const attachedCamera = (CP_HOOK_CLASS_NAME(CKCamera) *) m_AttachedCamera;
    auto *const rootEntity = (CP_HOOK_CLASS_NAME(CK3dEntity) *) m_RootEntity;

    if (attachedCamera && rootEntity) {
        rootEntity->SetWorldMatrix(attachedCamera->m_WorldMatrix);
        dev->m_Perspective = attachedCamera->GetProjectionType() == CK_PERSPECTIVEPROJECTION;
        dev->m_Zoom = attachedCamera->GetOrthographicZoom();
        dev->m_Fov = attachedCamera->GetFov();
        dev->m_NearPlane = attachedCamera->GetFrontPlane();
        dev->m_FarPlane = attachedCamera->GetBackPlane();
        UpdateViewportSize(TRUE, CK_RENDER_USECURRENTSETTINGS);
    }
}

void CKRenderedScene::UpdateViewportSize(CKBOOL updateProjection, CK_RENDER_FLAGS Flags) {
//    CP_CALL_METHOD_ORIG(UpdateViewportSize, force, Flags);

    auto *const dev = (CP_HOOK_CLASS_NAME(CKRenderContext) *) m_RenderContext;

    VxRect rect;
    dev->GetWindowRect(rect);

    if (Flags == CK_RENDER_USECURRENTSETTINGS)
        Flags = static_cast<CK_RENDER_FLAGS>(dev->GetCurrentRenderOptions());

    int width = 4;
    int height = 3;
    if (m_AttachedCamera)
        m_AttachedCamera->GetAspectRatio(width, height);

    int windowWidth = dev->m_Settings.m_Rect.right;
    int windowHeight = dev->m_Settings.m_Rect.bottom;

    int viewWidth;
    int viewHeight;
    int viewX = 0;
    int viewY = 0;

    auto ar1 = (float) ((double) windowWidth / (double) windowHeight);
    auto ar2 = (float) ((double) width / (double) height);

    if ((Flags & CK_RENDER_USECAMERARATIO) != 0) {
        if (ar1 <= ar2) {
            viewWidth = windowWidth;
            if (ar1 == ar2)
                viewHeight = windowHeight;
            else
                viewHeight = height * windowWidth / width;
        } else {
            viewWidth = width * windowHeight / height;
            viewHeight = windowHeight;
        }
        viewX += (int)((double)(windowWidth - viewWidth) * 0.5);
        viewY += (int)((double)(windowHeight - viewHeight) * 0.5);
    } else {
        viewWidth = windowWidth;
        viewHeight = windowHeight;
    }

    auto &data = dev->m_ViewportData;
    if ((dev->m_RenderFlags & CK_RENDER_USECAMERARATIO) != 0) {
        if (m_AttachedCamera) {
            if ((m_AttachedCamera->GetFlags() & CK_OBJECT_NOTTOBEDELETED) != 0) {
                if (updateProjection)
                    dev->UpdateProjection(TRUE);
            } else if (data.ViewWidth == viewWidth && data.ViewHeight == viewHeight &&
                       data.ViewX == viewX && data.ViewY == viewY) {
                if (updateProjection)
                    dev->UpdateProjection(TRUE);
            } else {
                data.ViewWidth = viewWidth;
                data.ViewHeight = viewHeight;
                data.ViewX = viewX;
                data.ViewY = viewY;
                dev->UpdateProjection(TRUE);
            }
        } else if (updateProjection) {
            dev->UpdateProjection(TRUE);
        }
    } else if (updateProjection) {
        dev->UpdateProjection(TRUE);
    }
}

void CKRenderedScene::ResizeViewport(const VxRect &rect) {
//    CP_CALL_METHOD_ORIG(ResizeViewport, rect);

    auto *rst = m_RenderContext->GetRasterizerContext();
    rst->m_ViewportData.ViewX = (int) rect.left;
    rst->m_ViewportData.ViewY = (int) rect.top;
    rst->m_ViewportData.ViewWidth = (int) rect.GetWidth();
    rst->m_ViewportData.ViewHeight = (int) rect.GetHeight();
    rst->SetViewport(&rst->m_ViewportData);
}

void CKRenderedScene::AddObject(CKRenderObject *obj) {
//    CP_CALL_METHOD_ORIG(AddObject, obj);

    CK_CLASSID cid = obj->GetClassID();
    if (CKIsChildClassOf(cid, CKCID_3DENTITY)) {
        m_3DEntities.PushBack((CK3dEntity *) obj);
        if (CKIsChildClassOf(cid, CKCID_CAMERA)) {
            m_Cameras.PushBack((CKCamera *) obj);
        } else if (CKIsChildClassOf(cid, CKCID_LIGHT)) {
            m_Lights.PushBack((CKLight *) obj);
        }
    } else if (CKIsChildClassOf(cid, CKCID_2DENTITY)) {
        auto *ent = (CK2dEntity *) obj;
        m_2DEntities.PushBack((CK2dEntity *) obj);
        if (!ent->GetParent()) {
            auto *rm = (CP_HOOK_CLASS_NAME(CKRenderManager) *) m_Context->GetRenderManager();
            if (ent->IsBackground()) {
                ent->SetParent(rm->m_2DRootBack);
            } else {
                ent->SetParent(rm->m_2DRootFore);
            }
        }
    }
}

void CKRenderedScene::RemoveObject(CKRenderObject *obj) {
//    CP_CALL_METHOD_ORIG(RemoveObject, obj);

    CK_CLASSID cid = obj->GetClassID();
    if (CKIsChildClassOf(cid, CKCID_3DENTITY)) {
        m_3DEntities.Remove((CK3dEntity *) obj);
        if (CKIsChildClassOf(cid, CKCID_CAMERA)) {
            m_Cameras.Remove((CKCamera *) obj);
            if (m_AttachedCamera == obj)
                m_AttachedCamera = nullptr;
        } else if (CKIsChildClassOf(cid, CKCID_LIGHT)) {
            m_Lights.Remove((CKLight *) obj);
        }
    } else if (CKIsChildClassOf(cid, CKCID_2DENTITY)) {
        auto *ent = (CK2dEntity *) obj;
        m_2DEntities.Remove((CK2dEntity *) obj);
        if (!ent->GetParent()) {
            auto *root = (CP_HOOK_CLASS_NAME(CK2dEntity) *) m_RenderContext->Get2dRoot(ent->IsBackground());
            root->m_Children.Remove(ent);
        }
    }
}

void CKRenderedScene::DetachAll() {
//    CP_CALL_METHOD_ORIG(DetachAll);

    auto *rm = (CP_HOOK_CLASS_NAME(CKRenderManager) *) m_Context->GetRenderManager();

    if (m_Context->IsInClearAll() || !rm->m_RenderContexts.IsHere(m_RenderContext->GetID())) {
        if (rm->m_2DRootBack) {
            auto *back = (CP_HOOK_CLASS_NAME(CK2dEntity) *) rm->m_2DRootBack;
            back->m_Children.Clear();
        }
        if (rm->m_2DRootFore) {
            auto *fore = (CP_HOOK_CLASS_NAME(CK2dEntity) *) rm->m_2DRootFore;
            fore->m_Children.Clear();
        }
    }

    if (!m_Context->IsInClearAll()) {
        for (XArray<CK2dEntity *>::Iterator it = m_2DEntities.Begin(); it != m_2DEntities.End(); ++it) {
            auto *ent = (CP_HOOK_CLASS_NAME(CK2dEntity) *) (*it);
            if (ent)
                ent->RemoveFromRenderContext(m_RenderContext);
        }
        for (XArray<CK3dEntity *>::Iterator it = m_3DEntities.Begin(); it != m_3DEntities.End(); ++it) {
            auto *ent = (CP_HOOK_CLASS_NAME(CK3dEntity) *) (*it);
            if (ent)
                ent->RemoveFromRenderContext(m_RenderContext);
        }
    }

    m_2DEntities.Clear();
    m_3DEntities.Clear();
    m_Cameras.Clear();
    m_Lights.Clear();
    m_AttachedCamera = nullptr;
}

bool CKRenderedScene::Hook(void *base) {
    if (!base)
        return false;

    CP_ADD_METHOD_HOOK(Draw, base, 0x704AE);
    CP_ADD_METHOD_HOOK(SetDefaultRenderStates, base, 0x6FF59);
    CP_ADD_METHOD_HOOK(SetupLights, base, 0x6FEA0);
    CP_ADD_METHOD_HOOK(PrepareCameras, base, 0x710F4);
    CP_ADD_METHOD_HOOK(ForceCameraSettingsUpdate, base, 0x70FFD);
    CP_ADD_METHOD_HOOK(UpdateViewportSize, base, 0x71381);
    CP_ADD_METHOD_HOOK(ResizeViewport, base, 0x70429);
    CP_ADD_METHOD_HOOK(AddObject, base, 0x6FAE5);
    CP_ADD_METHOD_HOOK(RemoveObject, base, 0x6FC21);
    CP_ADD_METHOD_HOOK(DetachAll, base, 0x6FD4F);

    return true;
}

bool CKRenderedScene::Unhook(void *base) {
    if (!base)
        return false;

    CP_REMOVE_METHOD_HOOK(Draw);
    CP_REMOVE_METHOD_HOOK(SetDefaultRenderStates);
    CP_REMOVE_METHOD_HOOK(SetupLights);
    CP_REMOVE_METHOD_HOOK(PrepareCameras);
    CP_REMOVE_METHOD_HOOK(ForceCameraSettingsUpdate);
    CP_REMOVE_METHOD_HOOK(UpdateViewportSize);
    CP_REMOVE_METHOD_HOOK(ResizeViewport);
    CP_REMOVE_METHOD_HOOK(AddObject);
    CP_REMOVE_METHOD_HOOK(RemoveObject);
    CP_REMOVE_METHOD_HOOK(DetachAll);

    return true;
}

// CKSceneGraphNode

CP_DEFINE_METHOD_PTRS(CKSceneGraphNode, NoTestsTraversal);
CP_DEFINE_METHOD_PTRS(CKSceneGraphNode, AddNode);
CP_DEFINE_METHOD_PTRS(CKSceneGraphNode, RemoveNode);
CP_DEFINE_METHOD_PTRS(CKSceneGraphNode, SortNodes);
CP_DEFINE_METHOD_PTRS(CKSceneGraphNode, Rebuild);
CP_DEFINE_METHOD_PTRS(CKSceneGraphNode, SetAsPotentiallyVisible);
CP_DEFINE_METHOD_PTRS(CKSceneGraphNode, SetAsInsideFrustum);
CP_DEFINE_METHOD_PTRS(CKSceneGraphNode, SetRenderContextMask);
CP_DEFINE_METHOD_PTRS(CKSceneGraphNode, SetPriority);
CP_DEFINE_METHOD_PTRS(CKSceneGraphNode, PrioritiesChanged);
CP_DEFINE_METHOD_PTRS(CKSceneGraphNode, EntityFlagsChanged);
CP_DEFINE_METHOD_PTRS(CKSceneGraphNode, IsToBeParsed);
CP_DEFINE_METHOD_PTRS(CKSceneGraphNode, ComputeHierarchicalBox);
CP_DEFINE_METHOD_PTRS(CKSceneGraphNode, InvalidateBox);

void CKSceneGraphNode::NoTestsTraversal(CKRenderContext *Dev, CK_RENDER_FLAGS Flags) {
//    CP_CALL_METHOD_ORIG(NoTestsTraversal, Dev, Flags);

    auto *const dev = (CP_HOOK_CLASS_NAME(CKRenderContext) *) Dev;
    auto *const rm = (CP_HOOK_CLASS_NAME(CKRenderManager) *) dev->m_RenderManager;
    auto *const rst = dev->m_RasterizerContext;

    CKBOOL needClip = FALSE;
    ++dev->m_SceneTraversalCalls;

    SetAsPotentiallyVisible();
    SetAsInsideFrustum();

    if ((m_Flags & 0x10) != 0) {
        SortNodes();
    }

    if (m_Entity->GetClassID() == CKCID_PLACE) {
        auto *place = (CKPlace *) m_Entity;
        VxRect &clip = place->ViewportClip();
        if (!clip.IsNull()) {
            if (clip != VxRect(0.0f, 0.0f, 1.0f, 1.0f)) {
                needClip = TRUE;
                VxRect rect(0.0f, 0.0f, 0.0f, 0.0f);
                const CKViewportData &data = dev->m_ViewportData;
                VxRect screen((float) data.ViewX, (float) data.ViewY, (float) (data.ViewWidth + data.ViewX), (float) (data.ViewHeight + data.ViewY));
                rect.TransformFromHomogeneous(screen);
                dev->SetClipRect(rect);
            }
        }
    }

    if (m_Entity) {
        if (m_Entity->GetClassID() == CKCID_CHARACTER) {
            m_Entity->ModifyMoveableFlags(VX_MOVEABLE_CHARACTERRENDERED, 0);
        }

        if ((dev->m_Mask & m_RenderContextMask) != 0 && m_Entity->IsToBeRendered()) {
            m_Entity->ModifyMoveableFlags(0, VX_MOVEABLE_EXTENTSUPTODATE);
            if (!m_Entity->IsToBeRenderedLast()) {
                dev->m_Stats.SceneTraversalTime += dev->m_SceneTraversalTimeProfiler.Current();
                m_Entity->Render(dev, Flags);
                dev->m_SceneTraversalTimeProfiler.Reset();
            } else {
                m_TimeFpsCalc = dev->m_TimeFpsCalc;
                rm->m_SceneGraphRootNode.AddTransparentObject(this);
            }
        }
    }

    for (int i = 0; i < m_ChildrenCount; ++i) {
        auto *child = m_Children[i];
        if ((dev->m_Mask & child->m_EntityMask) != 0) {
            child->NoTestsTraversal(dev, Flags);
        }
    }

    if (needClip) {
        rst->SetViewport(&dev->m_ViewportData);
        rst->SetTransformMatrix(VXMATRIX_PROJECTION, dev->m_ProjectionMatrix);
    }
}

void CKSceneGraphNode::AddNode(CKSceneGraphNode *node) {
//    CP_CALL_METHOD_ORIG(AddNode, node);

    node->m_Index = m_Children.Size();
    node->m_Parent = this;
    m_Children.PushBack(node);

    node->InvalidateBox(TRUE);

    auto *entity = (CP_HOOK_CLASS_NAME(CK3dEntity) *) m_Entity;
    SetRenderContextMask(entity ? entity->m_Mask : 0, TRUE);
    node->EntityFlagsChanged(TRUE);

    if (node->m_MaxPriority > m_Priority || node->m_Priority > m_Priority)
        PrioritiesChanged();
}

void CKSceneGraphNode::RemoveNode(CKSceneGraphNode *node) {
//    CP_CALL_METHOD_ORIG(RemoveNode, node);

    node->m_Parent = nullptr;
    if (node->m_Index < m_ChildrenCount)
        --m_ChildrenCount;
    m_Children.Remove(node);
    for (int i = node->m_Index; i < m_Children.Size(); ++i)
        m_Children[i]->m_Index = i;

    PrioritiesChanged();
    InvalidateBox(TRUE);
    auto *entity = (CP_HOOK_CLASS_NAME(CK3dEntity) *) m_Entity;
    SetRenderContextMask(entity ? entity->m_Mask : 0, TRUE);
}

void CKSceneGraphNode::SortNodes() {
    CP_CALL_METHOD_ORIG(SortNodes);
}

CKDWORD CKSceneGraphNode::Rebuild() {
    return CP_CALL_METHOD_ORIG(Rebuild);
}

void CKSceneGraphNode::SetAsPotentiallyVisible() {
//    CP_CALL_METHOD_ORIG(SetAsPotentiallyVisible);

    m_Flags &= ~3;
}

void CKSceneGraphNode::SetAsInsideFrustum() {
//    CP_CALL_METHOD_ORIG(SetAsInsideFrustum);

    m_Flags |= 1;
}

void CKSceneGraphNode::SetRenderContextMask(CKDWORD mask, CKBOOL force) {
//    CP_CALL_METHOD_ORIG(SetRenderContextMask, mask, force);

    if (mask != m_RenderContextMask || force) {
        m_RenderContextMask = mask;
        for (auto *node = this; node != nullptr; node = node->m_Parent) {
            CKDWORD flags = node->m_EntityMask;
            node->m_EntityMask = node->m_RenderContextMask;
            for (auto it = node->m_Children.Begin(); it != node->m_Children.End(); ++it)
                node->m_EntityMask |= (*it)->m_EntityMask;
            if (node->m_EntityMask == flags || !node->m_Parent)
                break;
            node->EntityFlagsChanged(TRUE);
        }
    }
}

void CKSceneGraphNode::SetPriority(int priority, int unused) {
//    CP_CALL_METHOD_ORIG(SetPriority, priority, unused);

    if (priority < -10000) {
        priority = -10000;
    } else if (priority > 10000) {
        priority = 10000;
    }

    if (m_MaxPriority == priority + 10000 || !m_Parent) {
        m_MaxPriority = priority + 10000;
    } else {
        m_Parent->m_Flags |= 0x10;
        m_MaxPriority = priority + 10000;
        m_Parent->PrioritiesChanged();
    }
}

void CKSceneGraphNode::PrioritiesChanged() {
//    CP_CALL_METHOD_ORIG(PrioritiesChanged);

    for (auto *node = this; node != nullptr; node = node->m_Parent) {
        CKWORD priority = 0;

        for (auto it = node->m_Children.Begin(); it != node->m_Children.End(); ++it) {
            if ((*it)->m_Priority > priority)
                priority = (*it)->m_Priority;
            if ((*it)->m_MaxPriority > priority)
                priority = (*it)->m_MaxPriority;
        }

        if (m_Priority == priority)
            break;

        m_Flags |= 0x10;
        m_Priority = priority;
    }
}

void CKSceneGraphNode::EntityFlagsChanged(CKBOOL hierarchically) {
//    CP_CALL_METHOD_ORIG(EntityFlagsChanged, hierarchically);

    if (!m_Parent || m_Parent->m_Children.IsEmpty())
        return;

    if (IsToBeParsed()) {
        if (m_Index >= m_Parent->m_ChildrenCount) {
            if (m_Parent->m_Children.Size() == 1) {
                m_Parent->m_ChildrenCount = 1;
                if (hierarchically && m_Parent->m_Parent)
                    m_Parent->EntityFlagsChanged(TRUE);
            } else {
                if (m_Index > m_Parent->m_ChildrenCount) {
                    auto *node = m_Parent->m_Children[m_Parent->m_ChildrenCount];
                    m_Parent->m_Children[m_Index] = node;
                    m_Parent->m_Children[m_Parent->m_ChildrenCount] = this;
                    node->m_Index = m_Index;
                    m_Index = m_Parent->m_ChildrenCount;
                }
                ++m_Parent->m_ChildrenCount;
                if (hierarchically && m_Parent->m_Parent)
                    m_Parent->EntityFlagsChanged(TRUE);
            }
            m_Parent->m_Flags |= 0x10;
        }
    } else if (m_Index < m_Parent->m_ChildrenCount) {
        if (m_Index < m_Parent->m_ChildrenCount - 1) {
            auto *node = m_Parent->m_Children[m_Parent->m_ChildrenCount - 1];
            m_Parent->m_Children[m_Index] = node;
            m_Parent->m_Children[m_Parent->m_ChildrenCount - 1] = this;
            node->m_Index = m_Index;
            m_Index = m_Parent->m_ChildrenCount - 1;
        }
        --m_Parent->m_ChildrenCount;
        if (hierarchically && m_Parent->m_Parent)
            m_Parent->EntityFlagsChanged(TRUE);
        m_Parent->m_Flags |= 0x10;
    }
}

CKBOOL CKSceneGraphNode::IsToBeParsed() {
//    return CP_CALL_METHOD_ORIG(IsToBeParsed);

    if (m_EntityMask == 0)
        return FALSE;
    if ((m_Entity->GetMoveableFlags() & VX_MOVEABLE_VISIBLE) != 0)
        return TRUE;
    if ((m_Entity->GetMoveableFlags() & VX_MOVEABLE_HIERARCHICALHIDE) != 0)
        return FALSE;
    return m_ChildrenCount != 0;
}

CKBOOL CKSceneGraphNode::ComputeHierarchicalBox() {
//    return CP_CALL_METHOD_ORIG(ComputeHierarchicalBox);

    if ((m_Flags & 8) != 0)
        return (m_Flags & 4) != 0;

    m_Flags |= 8;

    auto *entity = (CP_HOOK_CLASS_NAME(CK3dEntity) *) m_Entity;
    entity->UpdateBox();
    if ((entity->m_MoveableFlags & VX_MOVEABLE_BOXVALID) != 0) {
        m_Bbox = entity->m_WorldBoundingBox;
        for (auto it = m_Children.Begin(); it != m_Children.End(); ++it) {
            if ((*it)->ComputeHierarchicalBox())
                m_Bbox.Merge((*it)->m_Bbox);
        }
        m_Flags |= 4;
    } else {
        auto it = m_Children.Begin();
        while (it != m_Children.End()) {
            if ((*it)->ComputeHierarchicalBox()) {
                m_Bbox = (*it)->m_Bbox;
                m_Flags |= 4;
                ++it;
                break;
            }
            ++it;
        }

        while (it != m_Children.End()) {
            if ((*it)->ComputeHierarchicalBox())
                m_Bbox.Merge((*it)->m_Bbox);
            ++it;
        }
    }

    return (m_Flags & 4) != 0;
}

void CKSceneGraphNode::InvalidateBox(CKBOOL hierarchically) {
//    CP_CALL_METHOD_ORIG(InvalidateBox, b);

    m_Flags &= ~0xC;
    if (hierarchically) {
        for (auto *node = m_Parent; node && (node->m_Flags & 8) != 0; node = node->m_Parent)
            node->m_Flags &= ~0xC;
    }
}

void CKSceneGraphNode::sub_100789A0() {
    m_Flags &= ~1;
    m_Flags |= 2;

    for (int i = 0; i < m_ChildrenCount; ++i) {
        m_Children[i]->sub_100789A0();
    }
}

bool CKSceneGraphNode::Hook(void *base) {
    if (!base)
        return false;

    CP_ADD_METHOD_HOOK(NoTestsTraversal, base, 0x77E4D);
    CP_ADD_METHOD_HOOK(AddNode, base, 0x770B5);
    CP_ADD_METHOD_HOOK(RemoveNode, base, 0x7715F);
    CP_ADD_METHOD_HOOK(SortNodes, base, 0x76E60);
    CP_ADD_METHOD_HOOK(Rebuild, base, 0x77242);
    CP_ADD_METHOD_HOOK(SetAsPotentiallyVisible, base, 0xD250);
    CP_ADD_METHOD_HOOK(SetAsInsideFrustum, base, 0xD270);
    CP_ADD_METHOD_HOOK(SetRenderContextMask, base, 0x7784F);
    CP_ADD_METHOD_HOOK(SetPriority, base, 0x76F50);
    CP_ADD_METHOD_HOOK(PrioritiesChanged, base, 0x76FDD);
    CP_ADD_METHOD_HOOK(EntityFlagsChanged, base, 0x77426);
    CP_ADD_METHOD_HOOK(IsToBeParsed, base, 0x7766D);
    CP_ADD_METHOD_HOOK(ComputeHierarchicalBox, base, 0x776BC);
    CP_ADD_METHOD_HOOK(InvalidateBox, base, 0xD1F0);

    return true;
}

bool CKSceneGraphNode::Unhook(void *base) {
    if (!base)
        return false;

    CP_REMOVE_METHOD_HOOK(NoTestsTraversal);
    CP_REMOVE_METHOD_HOOK(AddNode);
    CP_REMOVE_METHOD_HOOK(RemoveNode);
    CP_REMOVE_METHOD_HOOK(SortNodes);
    CP_REMOVE_METHOD_HOOK(Rebuild);
    CP_REMOVE_METHOD_HOOK(SetAsPotentiallyVisible);
    CP_REMOVE_METHOD_HOOK(SetAsInsideFrustum);
    CP_REMOVE_METHOD_HOOK(SetRenderContextMask);
    CP_REMOVE_METHOD_HOOK(SetPriority);
    CP_REMOVE_METHOD_HOOK(PrioritiesChanged);
    CP_REMOVE_METHOD_HOOK(EntityFlagsChanged);
    CP_REMOVE_METHOD_HOOK(IsToBeParsed);
    CP_REMOVE_METHOD_HOOK(ComputeHierarchicalBox);
    CP_REMOVE_METHOD_HOOK(InvalidateBox);

    return true;
}

// CKSceneGraphRootNode

CP_DEFINE_METHOD_PTRS(CKSceneGraphRootNode, RenderTransparents);
CP_DEFINE_METHOD_PTRS(CKSceneGraphRootNode, SortTransparentObjects);
CP_DEFINE_METHOD_PTRS(CKSceneGraphRootNode, Check);
CP_DEFINE_METHOD_PTRS(CKSceneGraphRootNode, Clear);

void CKSceneGraphRootNode::RenderTransparents(CKRenderContext *Dev, CK_RENDER_FLAGS Flags) {
//    CP_CALL_METHOD_ORIG(RenderTransparents, Dev, Flags);

    auto *const dev = (CP_HOOK_CLASS_NAME(CKRenderContext) *) Dev;
    auto *const rm = (CP_HOOK_CLASS_NAME(CKRenderManager) *) dev->m_RenderManager;

    ++dev->m_SceneTraversalCalls;

    SetAsPotentiallyVisible();

    if (m_ChildrenCount == 0) {
        if (m_Entity && m_Entity->IsToBeRendered() && (dev->m_Mask & m_RenderContextMask) != 0 && m_Entity->IsInViewFrustrum(dev, Flags)) {
            if (!m_Entity->IsToBeRenderedLast()) {
                dev->m_Stats.SceneTraversalTime += dev->m_SceneTraversalTimeProfiler.Current();
                m_Entity->Render(dev, static_cast<CK_RENDER_FLAGS>(Flags | CK_RENDER_CLEARVIEWPORT));
                dev->m_SceneTraversalTimeProfiler.Reset();
            } else {
                m_TimeFpsCalc = dev->m_TimeFpsCalc;
                rm->m_SceneGraphRootNode.AddTransparentObject(this);
            }
        }
        return;
    }

    if ((m_Flags & 0x10) != 0) {
        SortNodes();
    }

    CKBOOL needClip = FALSE;

    if (m_Entity) {
        m_Entity->ModifyMoveableFlags(0, VX_MOVEABLE_EXTENTSUPTODATE);
        if (!m_Entity->IsInViewFrustrumHierarchic(dev)) {
            if (m_Entity->GetClassID() == CKCID_CHARACTER) {
                VxBbox box = m_Bbox;
                box.Max *= 2.0f;
                box.Min *= 2.0f;
                if (dev->m_RasterizerContext->ComputeBoxVisibility(box, TRUE)) {
                    m_Entity->ModifyMoveableFlags(VX_MOVEABLE_CHARACTERRENDERED, 0);
                }
            }
            sub_100789A0();
            return;
        }

        if (m_Entity->GetClassID() == CKCID_PLACE) {
            auto *place = (CKPlace *) m_Entity;
            VxRect &clip = place->ViewportClip();
            if (!clip.IsNull()) {
                if (clip != VxRect(0.0f, 0.0f, 1.0f, 1.0f)) {
                    needClip = TRUE;
                    VxRect rect = clip;
                    const CKViewportData &data = dev->m_ViewportData;
                    VxRect screen((float) data.ViewX, (float) data.ViewY, (float) (data.ViewWidth + data.ViewX), (float) (data.ViewHeight + data.ViewY));
                    rect.TransformFromHomogeneous(screen);
                    dev->SetClipRect(rect);
                }
            }
        }

        if (m_Entity->GetClassID() == CKCID_CHARACTER) {
            m_Entity->ModifyMoveableFlags(VX_MOVEABLE_CHARACTERRENDERED, 0);
        }

        if ((dev->m_Mask & m_RenderContextMask) != 0 && m_Entity->IsToBeRendered()) {
            if (!m_Entity->IsToBeRenderedLast()) {
                dev->m_Stats.SceneTraversalTime += dev->m_SceneTraversalTimeProfiler.Current();
                m_Entity->Render(dev, Flags);
                dev->m_SceneTraversalTimeProfiler.Reset();
            } else {
                if (IsInsideFrustum() || m_Entity->IsInViewFrustrum(dev, Flags)) {
                    m_TimeFpsCalc = dev->m_TimeFpsCalc;
                    rm->m_SceneGraphRootNode.AddTransparentObject(this);
                }
            }
        }
    }

    if (IsInsideFrustum()) {
        for (int i = 0; i < m_ChildrenCount; ++i) {
            auto *child = m_Children[i];
            if ((dev->m_Mask & child->m_EntityMask) != 0) {
                child->NoTestsTraversal(dev, Flags);
            }
        }
    } else {
        for (int i = 0; i < m_ChildrenCount; ++i) {
            auto *child = m_Children[i];
            if ((dev->m_Mask & child->m_EntityMask) != 0) {
                auto *root = (CKSceneGraphRootNode *) child;
                root->RenderTransparents(dev, Flags);
            }
        }
    }

    if (needClip) {
        dev->m_RasterizerContext->SetViewport(&dev->m_ViewportData);
        dev->m_RasterizerContext->SetTransformMatrix(VXMATRIX_PROJECTION, dev->m_ProjectionMatrix);
    }
}

void CKSceneGraphRootNode::SortTransparentObjects(CKRenderContext *Dev, CK_RENDER_FLAGS Flags) {
    CP_CALL_METHOD_ORIG(SortTransparentObjects, Dev, Flags);
}

void CKSceneGraphRootNode::Check() {
    CP_CALL_METHOD_ORIG(Check);
}

void CKSceneGraphRootNode::Clear() {
    CP_CALL_METHOD_ORIG(Clear);
}

void CKSceneGraphRootNode::AddTransparentObject(CKSceneGraphNode *node) {
    if ((node->m_Flags & 0x20) == 0) {
        m_TransparentObjects.PushBack(CKTransparentObject(node));
        node->m_Flags |= 0x20u;
    }
}

bool CKSceneGraphRootNode::Hook(void *base) {
    if (!base)
        return false;

    CP_ADD_METHOD_HOOK(RenderTransparents, base, 0x77912);
    CP_ADD_METHOD_HOOK(SortTransparentObjects, base, 0x78183);
    CP_ADD_METHOD_HOOK(Check, base, 0x78129);
    CP_ADD_METHOD_HOOK(Clear, base, 0x765C0);

    return true;
}

bool CKSceneGraphRootNode::Unhook(void *base) {
    if (!base)
        return false;

    CP_REMOVE_METHOD_HOOK(RenderTransparents);
    CP_REMOVE_METHOD_HOOK(SortTransparentObjects);
    CP_REMOVE_METHOD_HOOK(Check);
    CP_REMOVE_METHOD_HOOK(Clear);

    return true;
}

// CKRenderManager

CP_CLASS_VTABLE_NAME(CKRenderManager)<CKRenderManager> CP_HOOK_CLASS_NAME(CKRenderManager)::s_VTable = {};
CP_DEFINE_METHOD_PTRS(CP_HOOK_CLASS_NAME(CKRenderManager), GetFullscreenContext);

#define CP_RENDER_MANAGER_METHOD_NAME(Name) CP_HOOK_CLASS_NAME(CKRenderManager)::CP_FUNC_HOOK_NAME(Name)

int CP_RENDER_MANAGER_METHOD_NAME(GetRenderDriverCount)() {
//    return CP_CALL_METHOD_PTR(this, s_VTable.GetRenderDriverCount);

    return m_DriverCount;
}

VxDriverDesc *CP_RENDER_MANAGER_METHOD_NAME(GetRenderDriverDescription)(int Driver) {
//    return CP_CALL_METHOD_PTR(this, s_VTable.GetRenderDriverDescription, Driver);

    if (Driver < 0 || Driver >= m_DriverCount)
        return nullptr;

    VxDriverDesc2 *drv = &m_Drivers[Driver];
    if (!drv->CapsUpToDate)
        UpdateDriverDescCaps(drv);
    return &drv->Desc;
}

void CP_RENDER_MANAGER_METHOD_NAME(GetDesiredTexturesVideoFormat)(VxImageDescEx &VideoFormat) {
//    CP_CALL_METHOD_PTR(this, s_VTable.GetDesiredTexturesVideoFormat, VideoFormat);

    VxPixelFormat2ImageDesc((VX_PIXELFORMAT)m_TextureVideoFormat.Value, VideoFormat);
}

void CP_RENDER_MANAGER_METHOD_NAME(SetDesiredTexturesVideoFormat)(VxImageDescEx &VideoFormat) {
//    CP_CALL_METHOD_PTR(this, s_VTable.SetDesiredTexturesVideoFormat, VideoFormat);

    m_TextureVideoFormat.Value = VxImageDesc2PixelFormat(VideoFormat);
}

CKRenderContext *CP_RENDER_MANAGER_METHOD_NAME(GetRenderContext)(int pos) {
//    return CP_CALL_METHOD_PTR(this, s_VTable.GetRenderContext, pos);

    return (CKRenderContext *) m_RenderContexts.GetObject(m_Context, pos);
}

CKRenderContext *CP_RENDER_MANAGER_METHOD_NAME(GetRenderContextFromPoint)(CKPOINT &pt) {
//    return CP_CALL_METHOD_PTR(this, s_VTable.GetRenderContextFromPoint, pt);

    for (auto it = m_RenderContexts.Begin(); it != m_RenderContexts.End(); ++it) {
        auto *dev = (CKRenderContext *) m_Context->GetObject(*it);
        if (dev) {
            WIN_HANDLE handle = dev->GetWindowHandle();
            if (handle) {
                CKRECT rect;
                VxGetWindowRect(handle, &rect);
                if (VxPtInRect(&rect, &pt))
                    return dev;
            }
        }
    }

    return nullptr;
}

int CP_RENDER_MANAGER_METHOD_NAME(GetRenderContextCount)() {
//    return CP_CALL_METHOD_PTR(this, s_VTable.GetRenderContextCount);

    return m_RenderContexts.Size();
}

void CP_RENDER_MANAGER_METHOD_NAME(Process)() {
//    CP_CALL_METHOD_PTR(this, s_VTable.Process);

    for (auto it = m_RenderContexts.Begin(); it != m_RenderContexts.End(); ++it) {
        auto *dev = (CKRenderContext *) m_Context->GetObject(*it);
        if (dev)
            dev->Render();
    }
}

void CP_RENDER_MANAGER_METHOD_NAME(FlushTextures)() {
//    CP_CALL_METHOD_PTR(this, s_VTable.FlushTextures);

    int textureCount = m_Context->GetObjectsCountByClassID(CKCID_TEXTURE);
    CK_ID *textureIds = m_Context->GetObjectsListByClassID(CKCID_TEXTURE);
    for (int i = 0; i < textureCount; ++i) {
        auto *texture = (CKTexture *) m_Context->GetObject(textureIds[i]);
        if (texture)
            texture->FreeVideoMemory();
    }

    int spriteCount = m_Context->GetObjectsCountByClassID(CKCID_SPRITE);
    CK_ID *spriteIds = m_Context->GetObjectsListByClassID(CKCID_SPRITE);
    for (int i = 0; i < spriteCount; ++i) {
        auto *sprite = (CKSprite *) m_Context->GetObject(spriteIds[i]);
        if (sprite)
            sprite->FreeVideoMemory();
    }

    int spriteTextCount = m_Context->GetObjectsCountByClassID(CKCID_SPRITETEXT);
    CK_ID *spriteTextIds = m_Context->GetObjectsListByClassID(CKCID_SPRITETEXT);
    for (int i = 0; i < spriteTextCount; ++i) {
        auto *spriteText = (CKSpriteText *) m_Context->GetObject(spriteTextIds[i]);
        if (spriteText)
            spriteText->FreeVideoMemory();
    }
}

CKRenderContext *CP_RENDER_MANAGER_METHOD_NAME(CreateRenderContext)(void *Window, int Driver, CKRECT *Rect, CKBOOL Fullscreen, int Bpp, int Zbpp, int StencilBpp, int RefreshRate) {
    auto *const dev = (CP_HOOK_CLASS_NAME(CKRenderContext) *) m_Context->CreateObject(CKCID_RENDERCONTEXT);
    if (!dev)
        return nullptr;

    if (dev->Create(Window, Driver, Rect, Fullscreen, Bpp, Zbpp, StencilBpp, RefreshRate) != CK_OK) {
        m_Context->DestroyObject(dev);
        return nullptr;
    }

    m_RenderContexts.PushBack(dev->GetID());

    return dev;
}

CKERROR CP_RENDER_MANAGER_METHOD_NAME(DestroyRenderContext)(CKRenderContext *context) {
    if (!context)
        return CKERR_INVALIDPARAMETER;

    CKLevel *level = m_Context->GetCurrentLevel();
    if (level)
        level->RemoveRenderContext(context);

    if (!m_RenderContexts.RemoveObject(context))
        return CKERR_INVALIDPARAMETER;

    m_Context->DestroyObject(context);
    return CK_OK;
}

void CP_RENDER_MANAGER_METHOD_NAME(RemoveRenderContext)(CKRenderContext *context) {
//    CP_CALL_METHOD_PTR(this, s_VTable.RemoveRenderContext, context);

    if (context)
        m_RenderContexts.RemoveObject(context);
}

CKVertexBuffer *CP_RENDER_MANAGER_METHOD_NAME(CreateVertexBuffer)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.CreateVertexBuffer);
}

void CP_RENDER_MANAGER_METHOD_NAME(DestroyVertexBuffer)(CKVertexBuffer *VB) {
    CP_CALL_METHOD_PTR(this, s_VTable.DestroyVertexBuffer, VB);
}

void CP_RENDER_MANAGER_METHOD_NAME(SetRenderOptions)(CKSTRING RenderOptionString, CKDWORD Value) {
    CP_CALL_METHOD_PTR(this, s_VTable.SetRenderOptions, RenderOptionString, Value);
}

const VxEffectDescription &CP_RENDER_MANAGER_METHOD_NAME(GetEffectDescription)(int EffectIndex) {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetEffectDescription, EffectIndex);
}

int CP_RENDER_MANAGER_METHOD_NAME(GetEffectCount)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetEffectCount);
}

int CP_RENDER_MANAGER_METHOD_NAME(AddEffect)(const VxEffectDescription &NewEffect) {
    return CP_CALL_METHOD_PTR(this, s_VTable.AddEffect, NewEffect);
}

CKRasterizerContext *CP_HOOK_CLASS_NAME(CKRenderManager)::GetFullscreenContext() {
//    return CP_CALL_METHOD_ORIG(GetFullscreenContext);

    for (auto it = m_Rasterizers.Begin(); it != m_Rasterizers.End(); ++it) {
        CKRasterizerContext *rst = (*it)->m_FullscreenContext;
        if (rst)
            return rst;
    }

    return nullptr;
}

bool CP_HOOK_CLASS_NAME(CKRenderManager)::Hook(void *base) {
    if (!base)
        return false;

    auto *table = utils::ForceReinterpretCast<CP_CLASS_VTABLE_NAME(CKRenderManager)<CKRenderManager> *>(base, 0x86D08);
    utils::LoadVTable<CP_CLASS_VTABLE_NAME(CKRenderManager)<CKRenderManager>>(&table, s_VTable);

#define HOOK_RENDER_MANAGER_VIRTUAL_METHOD(Instance, Name) \
    utils::HookVirtualMethod(Instance, &CP_HOOK_CLASS_NAME(CKRenderManager)::CP_FUNC_HOOK_NAME(Name), \
                             (offsetof(CP_CLASS_VTABLE_NAME(CKRenderManager)<CKRenderManager>, Name) / sizeof(void*)))

    HOOK_RENDER_MANAGER_VIRTUAL_METHOD(&table, GetRenderDriverCount);
    HOOK_RENDER_MANAGER_VIRTUAL_METHOD(&table, GetRenderDriverDescription);
    HOOK_RENDER_MANAGER_VIRTUAL_METHOD(&table, GetDesiredTexturesVideoFormat);
    HOOK_RENDER_MANAGER_VIRTUAL_METHOD(&table, SetDesiredTexturesVideoFormat);
    HOOK_RENDER_MANAGER_VIRTUAL_METHOD(&table, GetRenderContext);
    HOOK_RENDER_MANAGER_VIRTUAL_METHOD(&table, GetRenderContextFromPoint);
    HOOK_RENDER_MANAGER_VIRTUAL_METHOD(&table, GetRenderContextCount);
    HOOK_RENDER_MANAGER_VIRTUAL_METHOD(&table, Process);
    HOOK_RENDER_MANAGER_VIRTUAL_METHOD(&table, FlushTextures);
    HOOK_RENDER_MANAGER_VIRTUAL_METHOD(&table, CreateRenderContext);
    HOOK_RENDER_MANAGER_VIRTUAL_METHOD(&table, DestroyRenderContext);
    HOOK_RENDER_MANAGER_VIRTUAL_METHOD(&table, RemoveRenderContext);
    // HOOK_RENDER_MANAGER_VIRTUAL_METHOD(&table, CreateVertexBuffer);
    // HOOK_RENDER_MANAGER_VIRTUAL_METHOD(&table, DestroyVertexBuffer);
    // HOOK_RENDER_MANAGER_VIRTUAL_METHOD(&table, SetRenderOptions);
    // HOOK_RENDER_MANAGER_VIRTUAL_METHOD(&table, GetEffectDescription);
    // HOOK_RENDER_MANAGER_VIRTUAL_METHOD(&table, GetEffectCount);
    // HOOK_RENDER_MANAGER_VIRTUAL_METHOD(&table, AddEffect);

#undef HOOK_RENDER_MANAGER_VIRTUAL_METHOD

    CP_ADD_METHOD_HOOK(GetFullscreenContext, base, 0x74F8A);

    g_UpdateTransparency = utils::ForceReinterpretCast<CKBOOL *>(base, 0x90CCC);
    g_FogProjectionType = utils::ForceReinterpretCast<CKDWORD *>(base, 0x90CD0);

    g_UpdateDriverDescCapsFunc = utils::ForceReinterpretCast<decltype(g_UpdateDriverDescCapsFunc)>(base, 0x734A0);

    return true;
}

bool CP_HOOK_CLASS_NAME(CKRenderManager)::Unhook(void *base) {
    if (!base)
        return false;

    auto *table = utils::ForceReinterpretCast<CP_CLASS_VTABLE_NAME(CKRenderManager)<CKRenderManager> *>(base, 0x86D08);
    utils::SaveVTable<CP_CLASS_VTABLE_NAME(CKRenderManager)<CKRenderManager>>(&table, s_VTable);

    CP_REMOVE_METHOD_HOOK(GetFullscreenContext);

    return true;
}

// CKRenderContext

bool CP_HOOK_CLASS_NAME(CKRenderContext)::s_DisableRender = false;
bool CP_HOOK_CLASS_NAME(CKRenderContext)::s_EnableWidescreenFix = false;
CP_CLASS_VTABLE_NAME(CKRenderContext)<CKRenderContext> CP_HOOK_CLASS_NAME(CKRenderContext)::s_VTable = {};
CP_DEFINE_METHOD_PTRS(CP_HOOK_CLASS_NAME(CKRenderContext), Create);
CP_DEFINE_METHOD_PTRS(CP_HOOK_CLASS_NAME(CKRenderContext), DestroyDevice);
CP_DEFINE_METHOD_PTRS(CP_HOOK_CLASS_NAME(CKRenderContext), CallSprite3DBatches);
CP_DEFINE_METHOD_PTRS(CP_HOOK_CLASS_NAME(CKRenderContext), UpdateProjection);
CP_DEFINE_METHOD_PTRS(CP_HOOK_CLASS_NAME(CKRenderContext), SetClipRect);
CP_DEFINE_METHOD_PTRS(CP_HOOK_CLASS_NAME(CKRenderContext), AddExtents2D);

#define CP_RENDER_CONTEXT_METHOD_NAME(Name) CP_HOOK_CLASS_NAME(CKRenderContext)::CP_FUNC_HOOK_NAME(Name)

void CP_RENDER_CONTEXT_METHOD_NAME(AddObject)(CKRenderObject *obj) {
    CP_CALL_METHOD_PTR(this, s_VTable.AddObject, obj);
}

void CP_RENDER_CONTEXT_METHOD_NAME(AddObjectWithHierarchy)(CKRenderObject *obj) {
    CP_CALL_METHOD_PTR(this, s_VTable.AddObjectWithHierarchy, obj);
}

void CP_RENDER_CONTEXT_METHOD_NAME(RemoveObject)(CKRenderObject *obj) {
    CP_CALL_METHOD_PTR(this, s_VTable.RemoveObject, obj);
}

CKBOOL CP_RENDER_CONTEXT_METHOD_NAME(IsObjectAttached)(CKRenderObject *obj) {
    return CP_CALL_METHOD_PTR(this, s_VTable.IsObjectAttached, obj);
}

const XObjectArray &CP_RENDER_CONTEXT_METHOD_NAME(Compute3dRootObjects)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.Compute3dRootObjects);
}

const XObjectArray &CP_RENDER_CONTEXT_METHOD_NAME(Compute2dRootObjects)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.Compute2dRootObjects);
}

CK2dEntity *CP_RENDER_CONTEXT_METHOD_NAME(Get2dRoot)(CKBOOL background) {
    return CP_CALL_METHOD_PTR(this, s_VTable.Get2dRoot, background);
}

void CP_RENDER_CONTEXT_METHOD_NAME(DetachAll)() {
//    CP_CALL_METHOD_PTR(this, s_VTable.DetachAll);

    m_ObjectExtents.Resize(0);
    if (m_RasterizerContext)
        m_RasterizerContext->FlushRenderStateCache();
    m_RenderedScene->DetachAll();
}

void CP_RENDER_CONTEXT_METHOD_NAME(ForceCameraSettingsUpdate)() {
//    CP_CALL_METHOD_PTR(this, s_VTable.ForceCameraSettingsUpdate);

    m_RenderedScene->ForceCameraSettingsUpdate();
}

void CP_RENDER_CONTEXT_METHOD_NAME(PrepareCameras)(CK_RENDER_FLAGS Flags) {
//    CP_CALL_METHOD_PTR(this, s_VTable.PrepareCameras, Flags);

    if (Flags == CK_RENDER_USECURRENTSETTINGS)
        Flags = m_RenderFlags;
    m_RenderedScene->PrepareCameras(Flags);
}

CKERROR CP_RENDER_CONTEXT_METHOD_NAME(Clear)(CK_RENDER_FLAGS Flags, CKDWORD Stencil) {
    return CP_CALL_METHOD_PTR(this, s_VTable.Clear, Flags, Stencil);
}

CKERROR CP_RENDER_CONTEXT_METHOD_NAME(DrawScene)(CK_RENDER_FLAGS Flags) {
//    return CP_CALL_METHOD_PTR(this, s_VTable.DrawScene, Flags);

    if (!m_RasterizerContext)
        return CKERR_INVALIDRENDERCONTEXT;

    if (Flags == CK_RENDER_USECURRENTSETTINGS)
        Flags = m_RenderFlags;

    if ((Flags & CK_RENDER_SKIPDRAWSCENE) != 0)
        return CK_OK;

    ++m_DrawSceneCalls;
    memset(&m_Stats, 0, sizeof(VxStats));
    m_Stats.SmoothedFps = m_SmoothedFps;
    m_RasterizerContext->m_RenderStateCacheHit = 0;
    m_RasterizerContext->m_RenderStateCacheMiss = 0;

    if ((Flags & CK_RENDER_DONOTUPDATEEXTENTS) == 0)
        m_ObjectExtents.Resize(0);

    m_RasterizerContext->BeginScene();
    CKERROR err = m_RenderedScene->Draw(Flags);
    m_RasterizerContext->EndScene();

    m_Stats.RenderStateCacheHit = m_RasterizerContext->m_RenderStateCacheHit;
    m_Stats.RenderStateCacheMiss = m_RasterizerContext->m_RenderStateCacheMiss;
    --m_DrawSceneCalls;

    return err;
}

CKERROR CP_RENDER_CONTEXT_METHOD_NAME(BackToFront)(CK_RENDER_FLAGS Flags) {
    return CP_CALL_METHOD_PTR(this, s_VTable.BackToFront, Flags);
}

CKERROR CP_RENDER_CONTEXT_METHOD_NAME(Render)(CK_RENDER_FLAGS Flags) {
    if (s_DisableRender)
        return CK_OK;

    VxTimeProfiler renderProfiler;

    if (!m_Active)
        return CKERR_RENDERCONTEXTINACTIVE;

    if (!m_RasterizerContext)
        return CKERR_INVALIDRENDERCONTEXT;

    if (Flags == CK_RENDER_USECURRENTSETTINGS)
        Flags = m_RenderFlags;

    CKTimeManager *tm = m_Context->GetTimeManager();
    if ((tm->GetLimitOptions() & CK_FRAMERATE_SYNC) != 0) {
        Flags = static_cast<CK_RENDER_FLAGS>(Flags & CK_RENDER_WAITVBL);
    }

    PrepareCameras(Flags);
    m_Camera = nullptr;

    if (m_RenderedScene->m_AttachedCamera) {
        CKAttributeManager *am = m_Context->GetAttributeManager();
        CKAttributeType attrType = am->GetAttributeTypeByName((CKSTRING) "1CamPl8ne4SterCube2Rend");
        CKParameterOut *param = m_RenderedScene->m_AttachedCamera->GetAttributeParameter(attrType);
        if (param) {
            auto *cameraId = static_cast<CK_ID *>(param->GetReadDataPtr(0));
            m_Camera = (CKCamera *) m_Context->GetObject(*cameraId);
        }
    }

    CKERROR err = Clear(Flags);
    if (err != CK_OK)
        return err;

    err = DrawScene(Flags);
    if (err != CK_OK)
        return err;

    // Skip Stereo code

    ++m_TimeFpsCalc;

    float renderTime = m_RenderTimeProfiler.Current();
    if (renderTime > 1000.0f) {
        float fps = (float) m_TimeFpsCalc * 1000.0f / renderTime;
        m_RenderTimeProfiler.Reset();
        m_TimeFpsCalc = 0;
        fps = fps * 0.9f + m_SmoothedFps * 0.1f;
        m_SmoothedFps = fps;
        m_Stats.SmoothedFps = fps;
    }

    err = BackToFront(Flags);
    if (err != CK_OK)
        return err;

    if ((Flags & CK_RENDER_DONOTUPDATEEXTENTS) != 0) {
        CKObjectExtents extent;
        GetViewRect(extent.m_Rect);
        extent.m_Flags = Flags;
        CKCamera *camera = GetAttachedCamera();
        extent.m_Camera = (camera) ? camera->GetID() : 0;
        m_Extents.PushBack(extent);
    }

    renderTime = renderProfiler.Current();
    m_Context->AddProfileTime(CK_PROFILE_RENDERTIME, renderTime);

    return err;
}

void CP_RENDER_CONTEXT_METHOD_NAME(AddPreRenderCallBack)(CK_RENDERCALLBACK Function, void *Argument, CKBOOL Temporary) {
    CP_CALL_METHOD_PTR(this, s_VTable.AddPreRenderCallBack, Function, Argument, Temporary);
}

void CP_RENDER_CONTEXT_METHOD_NAME(RemovePreRenderCallBack)(CK_RENDERCALLBACK Function, void *Argument) {
    CP_CALL_METHOD_PTR(this, s_VTable.RemovePreRenderCallBack, Function, Argument);
}

void CP_RENDER_CONTEXT_METHOD_NAME(AddPostRenderCallBack)(CK_RENDERCALLBACK Function, void *Argument, CKBOOL Temporary, CKBOOL BeforeTransparent) {
    CP_CALL_METHOD_PTR(this, s_VTable.AddPostRenderCallBack, Function, Argument, Temporary, BeforeTransparent);
}

void CP_RENDER_CONTEXT_METHOD_NAME(RemovePostRenderCallBack)(CK_RENDERCALLBACK Function, void *Argument) {
    CP_CALL_METHOD_PTR(this, s_VTable.RemovePostRenderCallBack, Function, Argument);
}

void CP_RENDER_CONTEXT_METHOD_NAME(AddPostSpriteRenderCallBack)(CK_RENDERCALLBACK Function, void *Argument, CKBOOL Temporary) {
    CP_CALL_METHOD_PTR(this, s_VTable.AddPostSpriteRenderCallBack, Function, Argument, Temporary);
}

void CP_RENDER_CONTEXT_METHOD_NAME(RemovePostSpriteRenderCallBack)(CK_RENDERCALLBACK Function, void *Argument) {
    CP_CALL_METHOD_PTR(this, s_VTable.RemovePostSpriteRenderCallBack, Function, Argument);
}

VxDrawPrimitiveData *CP_RENDER_CONTEXT_METHOD_NAME(GetDrawPrimitiveStructure)(CKRST_DPFLAGS Flags, int VertexCount) {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetDrawPrimitiveStructure, Flags, VertexCount);
}

CKWORD *CP_RENDER_CONTEXT_METHOD_NAME(GetDrawPrimitiveIndices)(int IndicesCount) {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetDrawPrimitiveIndices, IndicesCount);
}

void CP_RENDER_CONTEXT_METHOD_NAME(Transform)(VxVector *Dest, VxVector *Src, CK3dEntity *Ref) {
    CP_CALL_METHOD_PTR(this, s_VTable.Transform, Dest, Src, Ref);
}

void CP_RENDER_CONTEXT_METHOD_NAME(TransformVertices)(int VertexCount, VxTransformData *data, CK3dEntity *Ref) {
    CP_CALL_METHOD_PTR(this, s_VTable.TransformVertices, VertexCount, data, Ref);
}

CKERROR CP_RENDER_CONTEXT_METHOD_NAME(GoFullScreen)(int Width, int Height, int Bpp, int Driver, int RefreshRate) {
//    return CP_CALL_METHOD_PTR(this, s_VTable.GoFullScreen, Width, Height, Bpp, Driver, RefreshRate);

    if (m_Fullscreen)
        return CKERR_ALREADYFULLSCREEN;

    auto *rm = (CP_HOOK_CLASS_NAME(CKRenderManager) *) m_RenderManager;
    if (rm->GetFullscreenContext())
        return CKERR_ALREADYFULLSCREEN;

    m_FullscreenSettings = CKRenderContextSettings(m_RasterizerContext);
    m_AppHandle = VxGetParent(m_WinHandle);
    VxGetWindowRect(m_WinHandle, &m_WinRect);
    VxScreenToClient(m_AppHandle, (CKPOINT *) &m_WinRect);
    VxScreenToClient(m_AppHandle, (CKPOINT *) &m_WinRect.right);
    DestroyDevice();

    CKRECT rect = {0, 0, Width, Height};
    CKERROR err = Create(m_WinHandle, Driver, &rect, TRUE, Bpp, 0, 0, RefreshRate);
    if (err == CK_OK) {
        m_RenderedScene->UpdateViewportSize(TRUE, CK_RENDER_USECURRENTSETTINGS);
        Clear(static_cast<CK_RENDER_FLAGS>(CK_RENDER_CLEARSTENCIL | CK_RENDER_CLEARBACK | CK_RENDER_CLEARZ));
        BackToFront(CK_RENDER_USECURRENTSETTINGS);
        Clear(static_cast<CK_RENDER_FLAGS>(CK_RENDER_CLEARSTENCIL | CK_RENDER_CLEARBACK | CK_RENDER_CLEARZ));
    } else {
        VxSetParent(m_WinHandle, m_AppHandle);
        VxMoveWindow(m_WinHandle, m_WinRect.left, m_WinRect.top, m_WinRect.right - m_WinRect.left, m_WinRect.bottom - m_WinRect.top, FALSE);
        rect.left = m_FullscreenSettings.m_Rect.left;
        rect.top = m_FullscreenSettings.m_Rect.top;
        rect.right = rect.left + m_FullscreenSettings.m_Rect.right;
        rect.bottom = rect.top + m_FullscreenSettings.m_Rect.bottom;
        Create(m_WinHandle, m_DriverIndex, &rect, FALSE,
               m_FullscreenSettings.m_Bpp, m_FullscreenSettings.m_Zbpp, m_FullscreenSettings.m_StencilBpp, 0);
    }

    return err;
}

CKERROR CP_RENDER_CONTEXT_METHOD_NAME(StopFullScreen)() {
//    return CP_CALL_METHOD_PTR(this, s_VTable.StopFullScreen);

    auto *rm = (CP_HOOK_CLASS_NAME(CKRenderManager) *) m_RenderManager;
    if (rm->GetFullscreenContext() != m_RasterizerContext)
        return CKERR_INVALIDRENDERCONTEXT;

    if (!m_Fullscreen)
        return CK_OK;
    m_Fullscreen = FALSE;

    DestroyDevice();
    VxSetParent(m_WinHandle, m_AppHandle);
    VxMoveWindow(m_WinHandle, m_WinRect.left, m_WinRect.top, m_WinRect.right - m_WinRect.left, m_WinRect.bottom - m_WinRect.top, FALSE);

    CKRECT rect;
    rect.left = m_FullscreenSettings.m_Rect.left;
    rect.top = m_FullscreenSettings.m_Rect.top;
    rect.right = rect.left + m_FullscreenSettings.m_Rect.right;
    rect.bottom = rect.top + m_FullscreenSettings.m_Rect.bottom;

    CKERROR err = Create(m_WinHandle, m_DriverIndex, &rect, FALSE,
                         m_FullscreenSettings.m_Bpp, m_FullscreenSettings.m_Zbpp, m_FullscreenSettings.m_StencilBpp, 0);
    m_RenderedScene->UpdateViewportSize(FALSE, CK_RENDER_USECURRENTSETTINGS);

    return err;
}

CKBOOL CP_RENDER_CONTEXT_METHOD_NAME(IsFullScreen)() {
//    return CP_CALL_METHOD_PTR(this, s_VTable.IsFullScreen);

    return m_Fullscreen;
}

int CP_RENDER_CONTEXT_METHOD_NAME(GetDriverIndex)() {
//    return CP_CALL_METHOD_PTR(this, s_VTable.GetDriverIndex);

    return m_DriverIndex;
}

CKBOOL CP_RENDER_CONTEXT_METHOD_NAME(ChangeDriver)(int NewDriver) {
    return CP_CALL_METHOD_PTR(this, s_VTable.ChangeDriver, NewDriver);
}

WIN_HANDLE CP_RENDER_CONTEXT_METHOD_NAME(GetWindowHandle)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetWindowHandle);
}

void CP_RENDER_CONTEXT_METHOD_NAME(ScreenToClient)(Vx2DVector *ioPoint) {
    CP_CALL_METHOD_PTR(this, s_VTable.ScreenToClient, ioPoint);
}

void CP_RENDER_CONTEXT_METHOD_NAME(ClientToScreen)(Vx2DVector *ioPoint) {
    CP_CALL_METHOD_PTR(this, s_VTable.ClientToScreen, ioPoint);
}

CKERROR CP_RENDER_CONTEXT_METHOD_NAME(SetWindowRect)(VxRect &rect, CKDWORD Flags) {
    return CP_CALL_METHOD_PTR(this, s_VTable.SetWindowRect, rect, Flags);
}

void CP_RENDER_CONTEXT_METHOD_NAME(GetWindowRect)(VxRect &rect, CKBOOL ScreenRelative) {
    CP_CALL_METHOD_PTR(this, s_VTable.GetWindowRect, rect, ScreenRelative);
}

int CP_RENDER_CONTEXT_METHOD_NAME(GetHeight)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetHeight);
}

int CP_RENDER_CONTEXT_METHOD_NAME(GetWidth)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetWidth);
}

CKERROR CP_RENDER_CONTEXT_METHOD_NAME(Resize)(int PosX, int PosY, int SizeX, int SizeY, CKDWORD Flags) {
//    return CP_CALL_METHOD_PTR(this, s_VTable.Resize, PosX, PosY, SizeX, SizeY, Flags);

    if (m_DeviceValid)
        return CKERR_INVALIDRENDERCONTEXT;

    if (!m_RasterizerContext) {
        if (SizeX != 0 && SizeY != 0) {
            CKRECT rect = {PosX, PosY, SizeX + PosX, SizeY + PosY};
            Create(m_WinHandle, m_DriverIndex, &rect, FALSE, -1, -1, -1, 0);
        } else {
            Create(m_WinHandle, m_DriverIndex, nullptr, FALSE, -1, -1, -1, 0);
        }

        if (!m_RasterizerContext)
            return CKERR_INVALIDRENDERCONTEXT;
    }

    if (m_Fullscreen)
        return CKERR_ALREADYFULLSCREEN;

    if ((Flags & VX_RESIZE_NOMOVE) == 0) {
        m_Settings.m_Rect.left = PosX;
        m_Settings.m_Rect.top = PosY;
    }

    if ((Flags & VX_RESIZE_NOSIZE) == 0) {
        if (SizeX == 0 || SizeY == 0) {
            CKRECT rect;
            VxGetClientRect(m_WinHandle, &rect);
            SizeX = rect.right;
            SizeY = rect.bottom;
        }
        m_Settings.m_Rect.right = SizeX;
        m_Settings.m_Rect.bottom = SizeY;
        m_ViewportData.ViewX = 0;
        m_ViewportData.ViewY = 0;
        m_ViewportData.ViewWidth = SizeX;
        m_ViewportData.ViewHeight = SizeY;
        m_ProjectionUpdated = 0;
    }

    if (m_RasterizerContext->Resize(PosX, PosY, SizeX, SizeY, Flags)) {
        m_RenderedScene->UpdateViewportSize(FALSE, CK_RENDER_USECURRENTSETTINGS);
        return CK_OK;
    } else {
        m_RenderedScene->UpdateViewportSize(FALSE, CK_RENDER_USECURRENTSETTINGS);
        return CKERR_OUTOFMEMORY;
    }
}

void CP_RENDER_CONTEXT_METHOD_NAME(SetViewRect)(VxRect &rect) {
    CP_CALL_METHOD_PTR(this, s_VTable.SetViewRect, rect);
}

void CP_RENDER_CONTEXT_METHOD_NAME(GetViewRect)(VxRect &rect) {
    CP_CALL_METHOD_PTR(this, s_VTable.GetViewRect, rect);
}

VX_PIXELFORMAT CP_RENDER_CONTEXT_METHOD_NAME(GetPixelFormat)(int *Bpp, int *Zbpp, int *StencilBpp) {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetPixelFormat, Bpp, Zbpp, StencilBpp);
}

void CP_RENDER_CONTEXT_METHOD_NAME(SetState)(VXRENDERSTATETYPE State, CKDWORD Value) {
    CP_CALL_METHOD_PTR(this, s_VTable.SetState, State, Value);
}

CKDWORD CP_RENDER_CONTEXT_METHOD_NAME(GetState)(VXRENDERSTATETYPE State) {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetState, State);
}

CKBOOL CP_RENDER_CONTEXT_METHOD_NAME(SetTexture)(CKTexture *tex, CKBOOL Clamped, int Stage) {
    return CP_CALL_METHOD_PTR(this, s_VTable.SetTexture, tex, Clamped, Stage);
}

CKBOOL CP_RENDER_CONTEXT_METHOD_NAME(SetTextureStageState)(CKRST_TEXTURESTAGESTATETYPE State, CKDWORD Value, int Stage) {
    return CP_CALL_METHOD_PTR(this, s_VTable.SetTextureStageState, State, Value, Stage);
}

CKRasterizerContext *CP_RENDER_CONTEXT_METHOD_NAME(GetRasterizerContext)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetRasterizerContext);
}

void CP_RENDER_CONTEXT_METHOD_NAME(SetClearBackground)(CKBOOL ClearBack) {
    CP_CALL_METHOD_PTR(this, s_VTable.SetClearBackground, ClearBack);
}

CKBOOL CP_RENDER_CONTEXT_METHOD_NAME(GetClearBackground)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetClearBackground);
}

void CP_RENDER_CONTEXT_METHOD_NAME(SetClearZBuffer)(CKBOOL ClearZ) {
    CP_CALL_METHOD_PTR(this, s_VTable.SetClearZBuffer, ClearZ);
}

CKBOOL CP_RENDER_CONTEXT_METHOD_NAME(GetClearZBuffer)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetClearZBuffer);
}

void CP_RENDER_CONTEXT_METHOD_NAME(GetGlobalRenderMode)(VxShadeType *Shading, CKBOOL *Texture, CKBOOL *Wireframe) {
    CP_CALL_METHOD_PTR(this, s_VTable.GetGlobalRenderMode, Shading, Texture, Wireframe);
}

void CP_RENDER_CONTEXT_METHOD_NAME(SetGlobalRenderMode)(VxShadeType Shading, CKBOOL Texture, CKBOOL Wireframe) {
    CP_CALL_METHOD_PTR(this, s_VTable.SetGlobalRenderMode, Shading, Texture, Wireframe);
}

void CP_RENDER_CONTEXT_METHOD_NAME(SetCurrentRenderOptions)(CKDWORD flags) {
    CP_CALL_METHOD_PTR(this, s_VTable.SetCurrentRenderOptions, flags);
}

CKDWORD CP_RENDER_CONTEXT_METHOD_NAME(GetCurrentRenderOptions)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetCurrentRenderOptions);
}

void CP_RENDER_CONTEXT_METHOD_NAME(ChangeCurrentRenderOptions)(CKDWORD Add, CKDWORD Remove) {
    CP_CALL_METHOD_PTR(this, s_VTable.ChangeCurrentRenderOptions, Add, Remove);
}

void CP_RENDER_CONTEXT_METHOD_NAME(SetCurrentExtents)(VxRect &extents) {
    CP_CALL_METHOD_PTR(this, s_VTable.SetCurrentExtents, extents);
}

void CP_RENDER_CONTEXT_METHOD_NAME(GetCurrentExtents)(VxRect &extents) {
    CP_CALL_METHOD_PTR(this, s_VTable.GetCurrentExtents, extents);
}

void CP_RENDER_CONTEXT_METHOD_NAME(SetAmbientLightRGB)(float R, float G, float B) {
    CP_CALL_METHOD_PTR(this, s_VTable.SetAmbientLightRGB, R, G, B);
}

void CP_RENDER_CONTEXT_METHOD_NAME(SetAmbientLight)(CKDWORD Color) {
    CP_CALL_METHOD_PTR(this, s_VTable.SetAmbientLight, Color);
}

CKDWORD CP_RENDER_CONTEXT_METHOD_NAME(GetAmbientLight)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetAmbientLight);
}

void CP_RENDER_CONTEXT_METHOD_NAME(SetFogMode)(VXFOG_MODE Mode) {
    CP_CALL_METHOD_PTR(this, s_VTable.SetFogMode, Mode);
}

void CP_RENDER_CONTEXT_METHOD_NAME(SetFogStart)(float Start) {
    CP_CALL_METHOD_PTR(this, s_VTable.SetFogStart, Start);
}

void CP_RENDER_CONTEXT_METHOD_NAME(SetFogEnd)(float End) {
    CP_CALL_METHOD_PTR(this, s_VTable.SetFogEnd, End);
}

void CP_RENDER_CONTEXT_METHOD_NAME(SetFogDensity)(float Density) {
    CP_CALL_METHOD_PTR(this, s_VTable.SetFogDensity, Density);
}

void CP_RENDER_CONTEXT_METHOD_NAME(SetFogColor)(CKDWORD Color) {
    CP_CALL_METHOD_PTR(this, s_VTable.SetFogColor, Color);
}

VXFOG_MODE CP_RENDER_CONTEXT_METHOD_NAME(GetFogMode)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetFogMode);
}

float CP_RENDER_CONTEXT_METHOD_NAME(GetFogStart)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetFogStart);
}

float CP_RENDER_CONTEXT_METHOD_NAME(GetFogEnd)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetFogEnd);
}

float CP_RENDER_CONTEXT_METHOD_NAME(GetFogDensity)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetFogDensity);
}

CKDWORD CP_RENDER_CONTEXT_METHOD_NAME(GetFogColor)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetFogColor);
}

CKBOOL CP_RENDER_CONTEXT_METHOD_NAME(DrawPrimitive)(VXPRIMITIVETYPE pType, CKWORD *indices, int indexcount, VxDrawPrimitiveData *data) {
    return CP_CALL_METHOD_PTR(this, s_VTable.DrawPrimitive, pType, indices, indexcount, data);
}

void CP_RENDER_CONTEXT_METHOD_NAME(SetWorldTransformationMatrix)(const VxMatrix &M) {
    CP_CALL_METHOD_PTR(this, s_VTable.SetWorldTransformationMatrix, M);
}

void CP_RENDER_CONTEXT_METHOD_NAME(SetProjectionTransformationMatrix)(const VxMatrix &M) {
    CP_CALL_METHOD_PTR(this, s_VTable.SetProjectionTransformationMatrix, M);
}

void CP_RENDER_CONTEXT_METHOD_NAME(SetViewTransformationMatrix)(const VxMatrix &M) {
    CP_CALL_METHOD_PTR(this, s_VTable.SetViewTransformationMatrix, M);
}

const VxMatrix &CP_RENDER_CONTEXT_METHOD_NAME(GetWorldTransformationMatrix)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetWorldTransformationMatrix);
}

const VxMatrix &CP_RENDER_CONTEXT_METHOD_NAME(GetProjectionTransformationMatrix)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetProjectionTransformationMatrix);
}

const VxMatrix &CP_RENDER_CONTEXT_METHOD_NAME(GetViewTransformationMatrix)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetViewTransformationMatrix);
}

CKBOOL CP_RENDER_CONTEXT_METHOD_NAME(SetUserClipPlane)(CKDWORD ClipPlaneIndex, const VxPlane &PlaneEquation) {
    return CP_CALL_METHOD_PTR(this, s_VTable.SetUserClipPlane, ClipPlaneIndex, PlaneEquation);
}

CKBOOL CP_RENDER_CONTEXT_METHOD_NAME(GetUserClipPlane)(CKDWORD ClipPlaneIndex, VxPlane &PlaneEquation) {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetUserClipPlane, ClipPlaneIndex, PlaneEquation);
}

CKRenderObject *CP_RENDER_CONTEXT_METHOD_NAME(Pick)(int x, int y, CKPICKRESULT *oRes, CKBOOL iIgnoreUnpickable) {
    return CP_CALL_METHOD_PTR(this, s_VTable.Pick, x, y, oRes, iIgnoreUnpickable);
}

CKRenderObject *CP_RENDER_CONTEXT_METHOD_NAME(PointPick)(CKPOINT pt, CKPICKRESULT *oRes, CKBOOL iIgnoreUnpickable) {
    return CP_CALL_METHOD_PTR(this, s_VTable.PointPick, pt, oRes, iIgnoreUnpickable);
}

CKERROR CP_RENDER_CONTEXT_METHOD_NAME(RectPick)(const VxRect &r, XObjectPointerArray &oObjects, CKBOOL Intersect) {
    return CP_CALL_METHOD_PTR(this, s_VTable.RectPick, r, oObjects, Intersect);
}

void CP_RENDER_CONTEXT_METHOD_NAME(AttachViewpointToCamera)(CKCamera *cam) {
    CP_CALL_METHOD_PTR(this, s_VTable.AttachViewpointToCamera, cam);
}

void CP_RENDER_CONTEXT_METHOD_NAME(DetachViewpointFromCamera)() {
    CP_CALL_METHOD_PTR(this, s_VTable.DetachViewpointFromCamera);
}

CKCamera *CP_RENDER_CONTEXT_METHOD_NAME(GetAttachedCamera)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetAttachedCamera);
}

CK3dEntity *CP_RENDER_CONTEXT_METHOD_NAME(GetViewpoint)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetViewpoint);
}

CKMaterial *CP_RENDER_CONTEXT_METHOD_NAME(GetBackgroundMaterial)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetBackgroundMaterial);
}

void CP_RENDER_CONTEXT_METHOD_NAME(GetBoundingBox)(VxBbox *BBox) {
    CP_CALL_METHOD_PTR(this, s_VTable.GetBoundingBox, BBox);
}

void CP_RENDER_CONTEXT_METHOD_NAME(GetStats)(VxStats *stats) {
    CP_CALL_METHOD_PTR(this, s_VTable.GetStats, stats);
}

void CP_RENDER_CONTEXT_METHOD_NAME(SetCurrentMaterial)(CKMaterial *mat, CKBOOL Lit) {
    CP_CALL_METHOD_PTR(this, s_VTable.SetCurrentMaterial, mat, Lit);
}

void CP_RENDER_CONTEXT_METHOD_NAME(Activate)(CKBOOL active) {
    CP_CALL_METHOD_PTR(this, s_VTable.Activate, active);
}

int CP_RENDER_CONTEXT_METHOD_NAME(DumpToMemory)(const VxRect *iRect, VXBUFFER_TYPE buffer, VxImageDescEx &desc) {
    return CP_CALL_METHOD_PTR(this, s_VTable.DumpToMemory, iRect, buffer, desc);
}

int CP_RENDER_CONTEXT_METHOD_NAME(CopyToVideo)(const VxRect *iRect, VXBUFFER_TYPE buffer, VxImageDescEx &desc) {
    return CP_CALL_METHOD_PTR(this, s_VTable.CopyToVideo, iRect, buffer, desc);
}

CKERROR CP_RENDER_CONTEXT_METHOD_NAME(DumpToFile)(CKSTRING filename, const VxRect *rect, VXBUFFER_TYPE buffer) {
    return CP_CALL_METHOD_PTR(this, s_VTable.DumpToFile, filename, rect, buffer);
}

VxDirectXData *CP_RENDER_CONTEXT_METHOD_NAME(GetDirectXInfo)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetDirectXInfo);
}

void CP_RENDER_CONTEXT_METHOD_NAME(WarnEnterThread)() {
    CP_CALL_METHOD_PTR(this, s_VTable.WarnEnterThread);
}

void CP_RENDER_CONTEXT_METHOD_NAME(WarnExitThread)() {
    CP_CALL_METHOD_PTR(this, s_VTable.WarnExitThread);
}

CK2dEntity *CP_RENDER_CONTEXT_METHOD_NAME(Pick2D)(const Vx2DVector &v) {
    return CP_CALL_METHOD_PTR(this, s_VTable.Pick2D, v);
}

CKBOOL CP_RENDER_CONTEXT_METHOD_NAME(SetRenderTarget)(CKTexture *texture, int CubeMapFace) {
    return CP_CALL_METHOD_PTR(this, s_VTable.SetRenderTarget, texture, CubeMapFace);
}

void CP_RENDER_CONTEXT_METHOD_NAME(AddRemoveSequence)(CKBOOL Start) {
    CP_CALL_METHOD_PTR(this, s_VTable.AddRemoveSequence, Start);
}

void CP_RENDER_CONTEXT_METHOD_NAME(SetTransparentMode)(CKBOOL Trans) {
    CP_CALL_METHOD_PTR(this, s_VTable.SetTransparentMode, Trans);
}

void CP_RENDER_CONTEXT_METHOD_NAME(AddDirtyRect)(CKRECT *Rect) {
    CP_CALL_METHOD_PTR(this, s_VTable.AddDirtyRect, Rect);
}

void CP_RENDER_CONTEXT_METHOD_NAME(RestoreScreenBackup)() {
    CP_CALL_METHOD_PTR(this, s_VTable.RestoreScreenBackup);
}

CKDWORD CP_RENDER_CONTEXT_METHOD_NAME(GetStencilFreeMask)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetStencilFreeMask);
}

void CP_RENDER_CONTEXT_METHOD_NAME(UsedStencilBits)(CKDWORD stencilBits) {
    CP_CALL_METHOD_PTR(this, s_VTable.UsedStencilBits, stencilBits);
}

int CP_RENDER_CONTEXT_METHOD_NAME(GetFirstFreeStencilBits)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetFirstFreeStencilBits);
}

VxDrawPrimitiveData *CP_RENDER_CONTEXT_METHOD_NAME(LockCurrentVB)(CKDWORD VertexCount) {
    return CP_CALL_METHOD_PTR(this, s_VTable.LockCurrentVB, VertexCount);
}

CKBOOL CP_RENDER_CONTEXT_METHOD_NAME(ReleaseCurrentVB)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.ReleaseCurrentVB);
}

void CP_RENDER_CONTEXT_METHOD_NAME(SetTextureMatrix)(const VxMatrix &M, int Stage) {
    CP_CALL_METHOD_PTR(this, s_VTable.SetTextureMatrix, M, Stage);
}

void CP_RENDER_CONTEXT_METHOD_NAME(SetStereoParameters)(float EyeSeparation, float FocalLength) {
    CP_CALL_METHOD_PTR(this, s_VTable.SetStereoParameters, EyeSeparation, FocalLength);
}

void CP_RENDER_CONTEXT_METHOD_NAME(GetStereoParameters)(float &EyeSeparation, float &FocalLength) {
    CP_CALL_METHOD_PTR(this, s_VTable.GetStereoParameters, EyeSeparation, FocalLength);
}

CKERROR CP_HOOK_CLASS_NAME(CKRenderContext)::Create(WIN_HANDLE Window, int Driver, CKRECT *Rect, CKBOOL Fullscreen, int Bpp, int Zbpp, int StencilBpp, int RefreshRate) {
//    return CP_CALL_METHOD_ORIG(Create, Window, Driver, Rect, Fullscreen, Bpp, Zbpp, StencilBpp, RefreshRate);

    m_SmoothedFps = 0.0f;
    m_TimeFpsCalc = 0;

    m_RenderTimeProfiler.Reset();

    m_FocalLength = 0.4f;
    m_EyeSeparation = 100.0f;

    auto *rm = (CP_HOOK_CLASS_NAME(CKRenderManager) *) m_RenderManager;

    if (rm->GetFullscreenContext())
        return CKERR_ALREADYFULLSCREEN;

    if (m_RasterizerContext)
        return CKERR_INVALIDRENDERCONTEXT;

    // Skip Option ForceSoftware

   if (Driver > rm->m_DriverCount)
       return CK_OK;

    m_RasterizerDriver = rm->m_Drivers[Driver].RasterizerDriver;
    if (!m_RasterizerDriver)
        return CK_OK;

    m_WinHandle = Window;

    CKRECT rect;
    if (Rect)
        rect = *Rect;
    else
        VxGetClientRect(m_WinHandle, &rect);

    if (!Fullscreen)
        m_DriverIndex = Driver;
    m_DeviceValid = TRUE;

    if (Fullscreen) {
        m_AppHandle = VxGetParent(m_WinHandle);
        VxSetParent(m_WinHandle, nullptr);
        if (!VxMoveWindow(m_WinHandle, 0, 0, rect.right, rect.bottom, FALSE)) {
            m_DeviceValid = FALSE;
            return CKERR_INVALIDOPERATION;
        }
    }

    SetFullViewport(m_ViewportData, rect.right - rect.left, rect.bottom - rect.top);
    m_RasterizerContext = m_RasterizerDriver->CreateContext();
    if (!m_RasterizerContext)
        return CKERR_CANCREATERENDERCONTEXT;
    m_RasterizerContext->m_Antialias = rm->m_Antialias.Value == TRUE;
    m_RasterizerContext->m_EnableScreenDump = rm->m_EnableScreenDump.Value == TRUE;
    m_RasterizerContext->m_EnsureVertexShader = rm->m_EnsureVertexShader.Value == TRUE;
    CKBOOL result = m_RasterizerContext->Create(m_WinHandle,
                                                rect.left,
                                                rect.top,
                                                rect.right - rect.left,
                                                rect.bottom - rect.top,
                                                Bpp,
                                                Fullscreen,
                                                RefreshRate,
                                                Zbpp,
                                                StencilBpp);
    if (!result) {
        m_RasterizerDriver->DestroyContext(m_RasterizerContext);
        m_RasterizerContext = nullptr;
        m_DeviceValid = FALSE;
        return CKERR_CANCREATERENDERCONTEXT;
    }

    if (m_RasterizerContext)
        m_RasterizerContext->SetTransparentMode(m_TransparentMode);
    m_Fullscreen = Fullscreen;

    m_Settings = CKRenderContextSettings(m_RasterizerContext);
    if (!Fullscreen)
        m_FullscreenSettings = m_Settings;

    m_DeviceValid = FALSE;
    m_ProjectionUpdated = FALSE;
    m_Active = TRUE;
    m_VertexBufferCount = 0;
    m_DpFlags = 0;
    m_VertexBufferIndex = 0;
    m_StartIndex = -1;

    if (Fullscreen && m_WinRect.left == -1 && m_WinRect.right == -1)
    {
        m_FullscreenSettings = CKRenderContextSettings(m_RasterizerContext);;
        m_WinRect.left = 0;
        m_WinRect.top = 0;
        m_WinRect.right = m_FullscreenSettings.m_Rect.right;
        m_WinRect.right = m_FullscreenSettings.m_Rect.bottom;
    }

    return CK_OK;
}

CKBOOL CP_HOOK_CLASS_NAME(CKRenderContext)::DestroyDevice() {
    return CP_CALL_METHOD_ORIG(DestroyDevice);
}

void CP_HOOK_CLASS_NAME(CKRenderContext)::CallSprite3DBatches() {
    CP_CALL_METHOD_ORIG(CallSprite3DBatches);
}

CKBOOL CP_HOOK_CLASS_NAME(CKRenderContext)::UpdateProjection(CKBOOL force) {
    if (!force && m_ProjectionUpdated)
        return TRUE;
    if (!m_RasterizerContext)
        return FALSE;

    const auto aspect = (float) ((double) m_ViewportData.ViewWidth / (double) m_ViewportData.ViewHeight);
    if (m_Perspective) {
        const float fov = s_EnableWidescreenFix ? atan2f(tanf(m_Fov * 0.5f) * 0.75f * aspect, 1.0f) * 2.0f : m_Fov;
        m_ProjectionMatrix.Perspective(fov, aspect, m_NearPlane, m_FarPlane);
    } else {
        m_ProjectionMatrix.Orthographic(m_Zoom, aspect, m_NearPlane, m_FarPlane);
    }

    m_RasterizerContext->SetTransformMatrix(VXMATRIX_PROJECTION, m_ProjectionMatrix);
    m_RasterizerContext->SetViewport(&m_ViewportData);
    m_ProjectionUpdated = TRUE;

    VxRect rect(0.0f, 0.0f, (float) m_Settings.m_Rect.right, (float) m_Settings.m_Rect.bottom);
    auto *background = Get2dRoot(TRUE);
    background->SetRect(rect);
    auto *foreground = Get2dRoot(FALSE);
    foreground->SetRect(rect);

    return TRUE;
}

void CP_HOOK_CLASS_NAME(CKRenderContext)::SetClipRect(VxRect &rect) {
    CP_CALL_METHOD_ORIG(SetClipRect, rect);
}

void CP_HOOK_CLASS_NAME(CKRenderContext)::AddExtents2D(const VxRect &rect, CKObject *obj) {
    CP_CALL_METHOD_ORIG(AddExtents2D, rect, obj);
}

void CP_HOOK_CLASS_NAME(CKRenderContext)::SetFullViewport(CKViewportData &data, int width, int height) {
    data.ViewY = 0;
    data.ViewX = 0;
    data.ViewWidth = width;
    data.ViewHeight = height;
}

bool CP_HOOK_CLASS_NAME(CKRenderContext)::Hook(void *base) {
    if (!base)
        return false;

    auto *table = utils::ForceReinterpretCast<CP_CLASS_VTABLE_NAME(CKRenderContext)<CKRenderContext> *>(base, 0x86AF8);
    utils::LoadVTable<CP_CLASS_VTABLE_NAME(CKRenderContext)<CKRenderContext>>(&table, s_VTable);

#define HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(Instance, Name) \
    utils::HookVirtualMethod(Instance, &CP_HOOK_CLASS_NAME(CKRenderContext)::CP_FUNC_HOOK_NAME(Name), \
                             (offsetof(CP_CLASS_VTABLE_NAME(CKRenderContext)<CKRenderContext>, Name) / sizeof(void*)))

    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, AddObject);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, RemoveObject);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, IsObjectAttached);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, Compute3dRootObjects);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, Compute2dRootObjects);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, Get2dRoot);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, DetachAll);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, ForceCameraSettingsUpdate);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, PrepareCameras);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, Clear);
    HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, DrawScene);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, BackToFront);
    HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, Render);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, AddPreRenderCallBack);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, RemovePreRenderCallBack);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, AddPostRenderCallBack);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, RemovePostRenderCallBack);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, AddPostSpriteRenderCallBack);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, RemovePostSpriteRenderCallBack);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetDrawPrimitiveStructure);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetDrawPrimitiveIndices);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, Transform);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, TransformVertices);
    HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GoFullScreen);
    HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, StopFullScreen);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, IsFullScreen);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetDriverIndex);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, ChangeDriver);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetWindowHandle);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, ScreenToClient);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, ClientToScreen);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, SetWindowRect);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetWindowRect);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetHeight);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetWidth);
    HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, Resize);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, SetViewRect);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetViewRect);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetPixelFormat);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, SetState);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetState);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, SetTexture);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, SetTextureStageState);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetRasterizerContext);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, SetClearBackground);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetClearBackground);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, SetClearZBuffer);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetClearZBuffer);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetGlobalRenderMode);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, SetGlobalRenderMode);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, SetCurrentRenderOptions);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetCurrentRenderOptions);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, ChangeCurrentRenderOptions);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, SetCurrentExtents);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetCurrentExtents);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, SetAmbientLightRGB);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, SetAmbientLight);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetAmbientLight);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, SetFogMode);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, SetFogStart);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, SetFogEnd);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, SetFogDensity);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, SetFogColor);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetFogMode);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetFogStart);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetFogEnd);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetFogDensity);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetFogColor);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, DrawPrimitive);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, SetWorldTransformationMatrix);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, SetProjectionTransformationMatrix);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, SetViewTransformationMatrix);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetWorldTransformationMatrix);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetProjectionTransformationMatrix);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetViewTransformationMatrix);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, SetUserClipPlane);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetUserClipPlane);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, Pick);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, PointPick);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, RectPick);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, AttachViewpointToCamera);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, DetachViewpointFromCamera);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetAttachedCamera);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetViewpoint);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetBackgroundMaterial);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetBoundingBox);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetStats);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, SetCurrentMaterial);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, Activate);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, DumpToMemory);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, CopyToVideo);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, DumpToFile);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetDirectXInfo);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, WarnEnterThread);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, WarnExitThread);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, Pick2D);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, SetRenderTarget);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, AddRemoveSequence);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, SetTransparentMode);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, AddDirtyRect);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, RestoreScreenBackup);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetStencilFreeMask);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, UsedStencilBits);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetFirstFreeStencilBits);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, LockCurrentVB);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, ReleaseCurrentVB);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, SetTextureMatrix);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, SetStereoParameters);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(&table, GetStereoParameters);

#undef HOOK_RENDER_CONTEXT_VIRTUAL_METHOD

    CP_ADD_METHOD_HOOK(Create, base, 0x6711B);
    CP_ADD_METHOD_HOOK(DestroyDevice, base, 0x67558);
    CP_ADD_METHOD_HOOK(CallSprite3DBatches, base, 0x6DC61);
    CP_ADD_METHOD_HOOK(UpdateProjection, base, 0x6C68D);
    CP_ADD_METHOD_HOOK(SetClipRect, base, 0x6C808);
    CP_ADD_METHOD_HOOK(AddExtents2D, base, 0xD330);

    return true;
}

bool CP_HOOK_CLASS_NAME(CKRenderContext)::Unhook(void *base) {
    if (!base)
        return false;

    auto *table = utils::ForceReinterpretCast<CP_CLASS_VTABLE_NAME(CKRenderContext)<CKRenderContext> *>(base, 0x86AF8);
    utils::SaveVTable<CP_CLASS_VTABLE_NAME(CKRenderContext)<CKRenderContext>>(&table, s_VTable);

    CP_REMOVE_METHOD_HOOK(Create);
    CP_REMOVE_METHOD_HOOK(DestroyDevice);
    CP_REMOVE_METHOD_HOOK(CallSprite3DBatches);
    CP_REMOVE_METHOD_HOOK(UpdateProjection);
    CP_REMOVE_METHOD_HOOK(SetClipRect);
    CP_REMOVE_METHOD_HOOK(AddExtents2D);

    return true;
}

void CP_HOOK_CLASS_NAME(CKRenderObject)::RemoveFromRenderContext(CKRenderContext *rc) {
    auto *const dev = (CP_HOOK_CLASS_NAME(CKRenderContext) *) rc;

    m_Mask &= ~dev->m_Mask;
    if (CKIsChildClassOf(this, CKCID_3DENTITY) && !m_Context->IsInClearAll()) {
        auto *ent = (CP_HOOK_CLASS_NAME(CK3dEntity) *) this;
        if (dev->m_Start)
            ent->m_SceneGraphNode->m_RenderContextMask = m_Mask;
        else
            ent->m_SceneGraphNode->SetRenderContextMask(m_Mask, 0);
    }
}

// CK2dEntity

CP_CLASS_VTABLE_NAME(CK2dEntity)<CK2dEntity> CP_HOOK_CLASS_NAME(CK2dEntity)::s_VTable = {};
CP_DEFINE_METHOD_PTRS(CP_HOOK_CLASS_NAME(CK2dEntity), UpdateExtents);

#define CP_2D_ENTITY_METHOD_NAME(Name) CP_HOOK_CLASS_NAME(CK2dEntity)::CP_FUNC_HOOK_NAME(Name)

CKERROR CP_2D_ENTITY_METHOD_NAME(Render)(CKRenderContext *Dev) {
    if ((m_ObjectFlags & CK_OBJECT_HIERACHICALHIDE) != 0)
        return CK_OK;

    CKBOOL visible = FALSE;
    if (IsVisible() && (IsInScene(m_Context->GetCurrentScene()) || (m_ObjectFlags & CK_OBJECT_INTERFACEOBJ) != 0))
        visible = TRUE;

    if (!visible && m_Children.IsEmpty())
        return CK_OK;

    auto *const dev = (CP_HOOK_CLASS_NAME(CKRenderContext) *) Dev;

    CKBOOL updated = UpdateExtents(dev);

    if (visible) {
        if (m_Callbacks && m_Callbacks->m_PreCallBacks.Size() != 0) {
            dev->m_SpriteCallbacksTimeProfiler.Reset();
            dev->m_RasterizerContext->SetVertexShader(0);
            for (auto it = m_Callbacks->m_PreCallBacks.Begin(); it != m_Callbacks->m_PreCallBacks.End(); ++it) {
                ((CK_RENDEROBJECT_CALLBACK) it->callback)(dev, this, it->argument);
            }
            dev->m_Stats.SpriteCallbacksTime += dev->m_SpriteCallbacksTimeProfiler.Current();
        }

        if (updated)
            Draw(dev);
    }

    for (auto it = m_Children.Begin(); it != m_Children.End(); ++it) {
        auto *child = (CP_HOOK_CLASS_NAME(CK2dEntity) *) *it;
        if (updated || (child->m_Flags & CK_2DENTITY_CLIPTOPARENT) == 0)
            child->Render(dev);
    }

    if (visible && m_Callbacks && m_Callbacks->m_PostCallBacks.Size() != 0) {
        dev->m_SpriteCallbacksTimeProfiler.Reset();
        dev->m_RasterizerContext->SetVertexShader(0);
        for (auto it = m_Callbacks->m_PostCallBacks.Begin(); it != m_Callbacks->m_PostCallBacks.End(); ++it) {
            ((CK_RENDEROBJECT_CALLBACK) it->callback)(dev, this, it->argument);
        }
        dev->m_Stats.SpriteCallbacksTime += dev->m_SpriteCallbacksTimeProfiler.Current();
    }

    return CK_OK;
}

CKERROR CP_2D_ENTITY_METHOD_NAME(Draw)(CKRenderContext *Dev) {
    auto *const dev = (CP_HOOK_CLASS_NAME(CKRenderContext) *) Dev;
    auto *const rst = dev->m_RasterizerContext;

    if (m_Material)
    {
        VxRect viewRect;
        if ((m_Flags & CK_2DENTITY_CLIPTOCAMERAVIEW) == 0) {
            dev->GetViewRect(viewRect);
            VxRect windowRect;
            dev->GetWindowRect(windowRect);
            dev->SetFullViewport(rst->m_ViewportData, (int) windowRect.GetWidth(), (int) windowRect.GetHeight());
            rst->SetViewport(&rst->m_ViewportData);
        }

        m_Material->SetAsCurrent(dev);
        dev->SetState(VXRENDERSTATE_CULLMODE, VXCULL_NONE);
        dev->SetState(VXRENDERSTATE_FOGENABLE, FALSE);
        if (IsBackground())
        {
            dev->SetState(VXRENDERSTATE_ZFUNC, VXCMP_ALWAYS);
            dev->SetState(VXRENDERSTATE_ZWRITEENABLE, FALSE);
        }
        VxDrawPrimitiveData *data = dev->GetDrawPrimitiveStructure(CKRST_DP_CL_VCT, 4);
        VxColor diffuseColor = m_Material->GetDiffuse();
        CKDWORD diffuse = RGBAFTOCOLOR(&diffuseColor);
        VxFillStructure(4, data->ColorPtr, data->ColorStride, sizeof(CKDWORD), &diffuse);

        if (m_Material->GetTexture()) {
            XPtrStrided<VxUV> uvs(data->TexCoordPtr, data->TexCoordStride);
            uvs->u = m_SrcRect.left;
            uvs->v = m_SrcRect.top;
            ++uvs;
            uvs->u = m_SrcRect.right;
            uvs->v = m_SrcRect.top;
            ++uvs;
            uvs->u = m_SrcRect.right;
            uvs->v = m_SrcRect.bottom;
            ++uvs;
            uvs->u = m_SrcRect.left;
            uvs->v = m_SrcRect.bottom;
        }

        XPtrStrided<VxVector4> positions(data->PositionPtr, data->PositionStride);
        positions->Set(m_VtxPos.left + 0.5f, m_VtxPos.top + 0.5f, 0.0f, 1.0f);
        ++positions;
        positions->Set(m_VtxPos.right + 0.5f, m_VtxPos.top + 0.5f, 0.0f, 1.0f);
        ++positions;
        positions->Set(m_VtxPos.right + 0.5f, m_VtxPos.bottom + 0.5f, 0.0f, 1.0f);
        ++positions;
        positions->Set(m_VtxPos.left + 0.5f, m_VtxPos.bottom + 0.5f, 0.0f, 1.0f);

        dev->DrawPrimitive(VX_TRIANGLEFAN, nullptr, 4, data);
        dev->SetState(VXRENDERSTATE_FOGENABLE, dev->m_RenderedScene->m_FogMode != 0);
        if ((m_Flags & CK_2DENTITY_CLIPTOCAMERAVIEW) == 0)
        {
            rst->m_ViewportData.ViewX = (int) viewRect.left;
            rst->m_ViewportData.ViewY = (int) viewRect.top;
            rst->m_ViewportData.ViewWidth = (int) viewRect.GetWidth();
            rst->m_ViewportData.ViewHeight = (int) viewRect.GetHeight();
            rst->SetViewport(&rst->m_ViewportData);
        }
    } else if (!m_Context->IsPlaying()) {
        dev->SetState(VXRENDERSTATE_ALPHABLENDENABLE, TRUE);
        dev->SetState(VXRENDERSTATE_ALPHATESTENABLE, FALSE);
        dev->SetState(VXRENDERSTATE_SRCBLEND, VXBLEND_SRCALPHA);
        dev->SetState(VXRENDERSTATE_DESTBLEND, VXBLEND_INVSRCALPHA);
        dev->SetState(VXRENDERSTATE_CULLMODE, VXCULL_NONE);
        dev->SetState(VXRENDERSTATE_ZWRITEENABLE, FALSE);
        dev->SetState(VXRENDERSTATE_ZFUNC, VXCMP_ALWAYS);
        dev->SetState(VXRENDERSTATE_FOGENABLE, FALSE);
        dev->SetTexture(nullptr);

        VxDrawPrimitiveData *data = dev->GetDrawPrimitiveStructure(CKRST_DP_CL_VCT, 4);
        VxColor diffuseColor(0.0f, 0.0f, 0.0f);
        CKDWORD diffuse = RGBAFTOCOLOR(&diffuseColor);
        VxFillStructure(4, data->ColorPtr, data->ColorStride, sizeof(CKDWORD), &diffuse);

        {
            XPtrStrided<VxVector4> positions(data->PositionPtr, data->PositionStride);
            positions->Set(m_VtxPos.left, m_VtxPos.top, 0.0f, 1.0f);
            ++positions;
            positions->Set(m_VtxPos.right, m_VtxPos.top, 0.0f, 1.0f);
            ++positions;
            positions->Set(m_VtxPos.right, m_VtxPos.bottom, 0.0f, 1.0f);
            ++positions;
            positions->Set(m_VtxPos.left, m_VtxPos.bottom, 0.0f, 1.0f);
        }

        dev->DrawPrimitive(VX_TRIANGLEFAN, nullptr, 4, data);
        CKWORD *indices = dev->GetDrawPrimitiveIndices(5);
        indices[0] = 0;
        indices[1] = 1;
        indices[2] = 2;
        indices[3] = 3;
        indices[4] = 0;

        diffuse = A_MASK | R_MASK | G_MASK | B_MASK;

        XPtrStrided<VxVector4> positions(data->PositionPtr, data->PositionStride);
        positions->x -= 1.0f;
        ++positions;
        positions->x -= 1.0f;
        ++positions;
        positions->x -= 1.0f;
        ++positions;
        positions->x -= 1.0f;
        VxFillStructure(4, data->ColorPtr, data->ColorStride, sizeof(CKDWORD), &diffuse);
        dev->DrawPrimitive(VX_LINESTRIP, indices, 5, data);
        dev->SetState(VXRENDERSTATE_FOGENABLE, dev->m_RenderedScene->m_FogMode != 0);
    }

    return CK_OK;
}

CKBOOL CP_HOOK_CLASS_NAME(CK2dEntity)::UpdateExtents(CKRenderContext *Dev) {
    return CP_CALL_METHOD_ORIG_FORCE(CK2dEntity, UpdateExtents, Dev);
}

bool CP_HOOK_CLASS_NAME(CK2dEntity)::Hook(void *base) {
    if (!base)
        return false;

    auto *table = utils::ForceReinterpretCast<CP_CLASS_VTABLE_NAME(CK2dEntity)<CK2dEntity> *>(base, 0x86378);

#define HOOK_2D_ENTITY_VIRTUAL_METHOD(Instance, Name) \
    utils::HookVirtualMethod(Instance, &CP_HOOK_CLASS_NAME(CK2dEntity)::CP_FUNC_HOOK_NAME(Name), \
                             (offsetof(CP_CLASS_VTABLE_NAME(CK2dEntity)<CK2dEntity>, Name) / sizeof(void*)))

    HOOK_2D_ENTITY_VIRTUAL_METHOD(&table, Render);
    HOOK_2D_ENTITY_VIRTUAL_METHOD(&table, Draw);

#undef HOOK_2D_ENTITY_VIRTUAL_METHOD

    CP_ADD_METHOD_HOOK(UpdateExtents, base, 0x5CCE5);

    return true;
}

bool CP_HOOK_CLASS_NAME(CK2dEntity)::Unhook(void *base) {
    if (!base)
        return false;

    auto *table = utils::ForceReinterpretCast<CP_CLASS_VTABLE_NAME(CK2dEntity)<CK2dEntity> *>(base, 0x86378);
    utils::SaveVTable<CP_CLASS_VTABLE_NAME(CK2dEntity)<CK2dEntity>>(&table, s_VTable);

    CP_REMOVE_METHOD_HOOK(UpdateExtents);

    return true;
}

// CK3dEntity

CP_CLASS_VTABLE_NAME(CK3dEntity)<CK3dEntity> CP_HOOK_CLASS_NAME(CK3dEntity)::s_VTable = {};
CP_DEFINE_METHOD_PTRS(CP_HOOK_CLASS_NAME(CK3dEntity), WorldMatrixChanged);

#define CP_3D_ENTITY_METHOD_NAME(Name) CP_HOOK_CLASS_NAME(CK3dEntity)::CP_FUNC_HOOK_NAME(Name)

CKBOOL CP_3D_ENTITY_METHOD_NAME(Render)(CKRenderContext *Dev, CKDWORD Flags) {
    auto *const dev = (CP_HOOK_CLASS_NAME(CKRenderContext) *) Dev;

    if (!m_CurrentMesh && !m_Callbacks)
        return FALSE;

    if (dev->m_SortTransparentObjects && !dev->m_Sprite3DBatches.IsEmpty()) {
        dev->CallSprite3DBatches();
    }

    if ((m_MoveableFlags & VX_MOVEABLE_EXTENTSUPTODATE) != 0) {
        if ((Flags & CK_RENDER_CLEARVIEWPORT) == 0)
            dev->SetWorldTransformationMatrix(m_WorldMatrix);
    } else if (!IsInViewFrustrum(Dev, Flags)) {
        if ((dev->m_Flags & 1) != 0) {
            dev->m_CurrentObjectDesc << m_Name;
            if (IsToBeRenderedLast()) {
                dev->m_CurrentObjectDesc << " (as transparent Object)";
            }
            dev->m_CurrentObjectDesc << " : ";
            dev->m_CurrentObjectDesc << this;
            dev->m_CurrentObjectDesc << "\n";

            if (--dev->m_FpsInterval <= 0) {
                dev->BackToFront();
            }
        }

        return TRUE;
    }

    if ((m_MoveableFlags & VX_MOVEABLE_INDIRECTMATRIX) != 0)
    {
        CKDWORD inverseWinding = FALSE;
        dev->m_RasterizerContext->GetRenderState(VXRENDERSTATE_INVERSEWINDING, &inverseWinding);
        dev->m_RasterizerContext->SetRenderState(VXRENDERSTATE_INVERSEWINDING, !inverseWinding);
    }

    CKBOOL isPM = FALSE;

    if (m_Skin && m_CurrentMesh) {
        if (m_CurrentMesh->IsPM()) {
            isPM = TRUE;
        } else {
            dev->m_SkinTimeProfiler.Reset();
            UpdateSkin();
            dev->m_Stats.SkinTime += dev->m_SkinTimeProfiler.Current();
        }
    }

    if (m_Callbacks) {
        if (m_Callbacks->m_PreCallBacks.Size() != 0) {
            dev->m_ObjectsCallbacksTimeProfiler.Reset();
            for (auto it = m_Callbacks->m_PreCallBacks.Begin(); it != m_Callbacks->m_PreCallBacks.End(); ++it) {
                ((CK_RENDEROBJECT_CALLBACK) it->callback)(dev, this, it->argument);
            }
            dev->m_Stats.ObjectsCallbacksTime += dev->m_ObjectsCallbacksTimeProfiler.Current();
        }

        if (isPM) {
            dev->m_SkinTimeProfiler.Reset();
            UpdateSkin();
            dev->m_Stats.SkinTime += dev->m_SkinTimeProfiler.Current();
        }

        if (m_Callbacks->m_OnCallBack) {
            const VxCallBack *const cb = m_Callbacks->m_OnCallBack;
            ((CK_RENDEROBJECT_CALLBACK) cb->callback)(dev, this, cb->argument);
        } else if (m_CurrentMesh && (m_CurrentMesh->GetFlags() & VXMESH_VISIBLE) != 0) {
            dev->m_Current3dEntity = (CK3dEntity *) this;
            m_CurrentMesh->Render(dev, (CK3dEntity *) this);
            dev->m_Current3dEntity = nullptr;
        }

        if (m_Callbacks->m_PostCallBacks.Size() != 0) {
            dev->m_ObjectsCallbacksTimeProfiler.Reset();
            dev->m_RasterizerContext->SetVertexShader(0);
            for (auto it = m_Callbacks->m_PostCallBacks.Begin(); it != m_Callbacks->m_PostCallBacks.End(); ++it) {
                ((CK_RENDEROBJECT_CALLBACK) it->callback)(dev, this, it->argument);
            }
            dev->m_Stats.ObjectsCallbacksTime += dev->m_ObjectsCallbacksTimeProfiler.Current();
        }
    } else if (m_CurrentMesh && (m_CurrentMesh->GetFlags() & VXMESH_VISIBLE) != 0) {
        dev->m_Current3dEntity = (CK3dEntity *) this;
        m_CurrentMesh->Render(dev, (CK3dEntity *) this);
        dev->m_Current3dEntity = nullptr;
    }

    if ((Flags & CK_RENDER_DEFAULTSETTINGS) != 0) {
        dev->AddExtents2D(m_RenderExtents, this);
    }

    if ((dev->m_Flags & 1) != 0) {
        dev->m_CurrentObjectDesc << m_Name;
        if (IsToBeRenderedLast()) {
            dev->m_CurrentObjectDesc << " (as transparent Object)";
        }
        dev->m_CurrentObjectDesc << " : ";
        dev->m_CurrentObjectDesc << this;
        dev->m_CurrentObjectDesc << "\n";

        if (--dev->m_FpsInterval <= 0) {
            dev->BackToFront();
        }
    }

    return TRUE;
}

void CP_HOOK_CLASS_NAME(CK3dEntity)::WorldMatrixChanged(CKBOOL invalidateBox, CKBOOL inverse) {
    CP_CALL_METHOD_ORIG_FORCE(CK3dEntity, WorldMatrixChanged, invalidateBox, inverse);
}

bool CP_HOOK_CLASS_NAME(CK3dEntity)::Hook(void *base) {
    if (!base)
        return false;

    auto *table = utils::ForceReinterpretCast<CP_CLASS_VTABLE_NAME(CK3dEntity)<CK3dEntity> *>(base, 0x83650);

#define HOOK_3D_ENTITY_VIRTUAL_METHOD(Instance, Name) \
    utils::HookVirtualMethod(Instance, &CP_HOOK_CLASS_NAME(CK3dEntity)::CP_FUNC_HOOK_NAME(Name), \
                             (offsetof(CP_CLASS_VTABLE_NAME(CK3dEntity)<CK3dEntity>, Name) / sizeof(void*)))

    HOOK_3D_ENTITY_VIRTUAL_METHOD(&table, Render);

#undef HOOK_3D_ENTITY_VIRTUAL_METHOD

    CP_ADD_METHOD_HOOK(WorldMatrixChanged, base, 0x62D6);

    return true;
}

bool CP_HOOK_CLASS_NAME(CK3dEntity)::Unhook(void *base) {
    if (!base)
        return false;

    auto *table = utils::ForceReinterpretCast<CP_CLASS_VTABLE_NAME(CK3dEntity)<CK3dEntity> *>(base, 0x83650);
    utils::SaveVTable<CP_CLASS_VTABLE_NAME(CK3dEntity)<CK3dEntity>>(&table, s_VTable);

    CP_REMOVE_METHOD_HOOK(WorldMatrixChanged);

    return true;
}

CP_DEFINE_METHOD_PTRS(CP_HOOK_CLASS_NAME(CKLight), Setup);

CKBOOL CP_HOOK_CLASS_NAME(CKLight)::Setup(CKRasterizerContext *rst, int index) {
    return CP_CALL_METHOD_ORIG_FORCE(CKLight, Setup, rst, index);
}

bool CP_HOOK_CLASS_NAME(CKLight)::Hook(void *base) {
    if (!base)
        return false;

    CP_ADD_METHOD_HOOK(Setup, base, 0x1B0C2);

    return true;
}

bool CP_HOOK_CLASS_NAME(CKLight)::Unhook(void *base) {
    if (!base)
        return false;

    CP_REMOVE_METHOD_HOOK(Setup);

    return true;
}

namespace RenderHook {
    bool HookRenderEngine() {
        void *base = utils::GetModuleBaseAddress("CK2_3D.dll");
        if (!base)
            return false;

        CKRenderedScene::Hook(base);
        CKSceneGraphNode::Hook(base);
        CKSceneGraphRootNode::Hook(base);

        CP_HOOK_CLASS_NAME(CKRenderManager)::Hook(base);
        CP_HOOK_CLASS_NAME(CKRenderContext)::Hook(base);

        CP_HOOK_CLASS_NAME(CK2dEntity)::Hook(base);
        CP_HOOK_CLASS_NAME(CK3dEntity)::Hook(base);
        CP_HOOK_CLASS_NAME(CKLight)::Hook(base);

        return true;
    }

    bool UnhookRenderEngine()
    {
        void *base = utils::GetModuleBaseAddress("CK2_3D.dll");
        if (!base)
            return false;

        CKRenderedScene::Unhook(base);
        CKSceneGraphNode::Unhook(base);
        CKSceneGraphRootNode::Unhook(base);

        CP_HOOK_CLASS_NAME(CKRenderManager)::Unhook(base);
        CP_HOOK_CLASS_NAME(CKRenderContext)::Unhook(base);

        CP_HOOK_CLASS_NAME(CK2dEntity)::Unhook(base);
        CP_HOOK_CLASS_NAME(CK3dEntity)::Unhook(base);
        CP_HOOK_CLASS_NAME(CKLight)::Unhook(base);

        return true;
    }

    void DisableRender(bool disable) {
        CP_HOOK_CLASS_NAME(CKRenderContext)::s_DisableRender = disable;
    }

    void EnableWidescreenFix(bool enable) {
        CP_HOOK_CLASS_NAME(CKRenderContext)::s_EnableWidescreenFix = enable;
    }
}