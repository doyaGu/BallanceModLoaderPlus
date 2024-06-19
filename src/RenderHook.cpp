#include "RenderHook.h"

#include "CKTimeManager.h"
#include "CKAttributeManager.h"
#include "CKParameterOut.h"
#include "CKRasterizer.h"

#include <MinHook.h>

#include "HookUtils.h"
#include "VTables.h"

#include "Overlay.h"

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

static CKBOOL *g_UpdateTransparency = nullptr;

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

    auto *dev = (CP_HOOK_CLASS_NAME(CKRenderContext) *) m_RenderContext;
    auto *rm = (CP_HOOK_CLASS_NAME(CKRenderManager) *) dev->m_RenderManager;
    auto *rst = dev->m_RasterizerContext;

    SetDefaultRenderStates(rst);

    dev->m_SpriteTimeProfiler.Reset();
    if ((Flags & CK_RENDER_BACKGROUNDSPRITES) != 0 && rm->m_2DRootBack->GetChildrenCount()) {
        VxRect rect;
        dev->GetViewRect(rect);
        dev->SetFullViewport(rst->m_ViewportData, dev->m_WindowRect.right, dev->m_WindowRect.bottom);
        rst->SetViewport(&rst->m_ViewportData);
        rm->m_2DRootBack->Render(m_RenderContext);
        ResizeViewport(rect);
    }
    dev->m_Stats.SpriteTime = dev->m_SpriteTimeProfiler.Current() + dev->m_Stats.SpriteTime;

    auto *rootEntity = (CP_HOOK_CLASS_NAME(CK3dEntity) *) m_RootEntity;

    if ((Flags & CK_RENDER_SKIP3D) == 0) {
        if (dev->m_Camera) {
            auto *camera = (CP_HOOK_CLASS_NAME(CKCamera) *) dev->m_Camera;
            rootEntity->m_WorldMatrix = camera->m_WorldMatrix;
            rootEntity->WorldMatrixChanged(FALSE, TRUE);
            VxVector vec;
            camera->InverseTransform(&vec, rootEntity->m_WorldMatrix[3]);
            auto z = dev->m_NearPlane / fabsf(vec.z);
            float right = (camera->m_LocalBoundingBox.Max.x - vec.x) * z;
            float left = (camera->m_LocalBoundingBox.Min.x - vec.x) * z;
            float top = (camera->m_LocalBoundingBox.Max.y - vec.y) * z;
            float bottom = (camera->m_LocalBoundingBox.Min.y - vec.y) * z;
            dev->m_ProjectionMatrix.PerspectiveRect(left, right, top, bottom, dev->m_NearPlane, dev->m_FarPlane);
            rst->SetTransformMatrix(VXMATRIX_PROJECTION, dev->m_ProjectionMatrix);
            rst->SetViewport(&dev->m_ViewportData);

            VxRect rect(0.0f, 0.0f, (float) dev->m_WindowRect.right, (float) dev->m_WindowRect.bottom);
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
        dev->m_Stats.DevicePreCallbacks = dev->m_DevicePreCallbacksTimeProfiler.Current() + dev->m_Stats.DevicePreCallbacks;

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
        dev->m_Stats.SceneTraversalTime = dev->m_SceneTraversalTimeProfiler.Current() + dev->m_Stats.SceneTraversalTime;
        dev->CallSprite3DBatches();

        dev->m_DevicePostCallbacksTimeProfiler.Reset();
        auto &devicePostCallbacks = dev->m_PreRenderTempCallBacks.m_PostCallBacks;
        for (auto it = devicePostCallbacks.Begin(); it != devicePostCallbacks.End(); ++it) {
            auto &cb = *it;
            ((CK_RENDERCALLBACK) cb.callback)(dev, cb.argument);
        }
        dev->m_Stats.DevicePostCallbacks = dev->m_DevicePostCallbacksTimeProfiler.Current() + dev->m_Stats.DevicePostCallbacks;

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
        dev->m_Stats.DevicePostCallbacks = dev->m_DevicePostCallbacksTimeProfiler.Current() + dev->m_Stats.DevicePostCallbacks;

        m_Context->ExecuteManagersOnPostRender(dev);
    }

    dev->m_SpriteTimeProfiler.Reset();
    if ((Flags & CK_RENDER_FOREGROUNDSPRITES) != 0 && rm->m_2DRootFore->GetChildrenCount() != 0) {
        VxRect rect;
        dev->GetViewRect(rect);
        dev->SetFullViewport(rst->m_ViewportData, dev->m_WindowRect.right, dev->m_WindowRect.bottom);
        rst->SetViewport(&rst->m_ViewportData);
        rm->m_2DRootFore->Render(m_RenderContext);
        ResizeViewport(rect);
    }
    dev->m_Stats.SpriteTime = dev->m_SpriteTimeProfiler.Current() + dev->m_Stats.SpriteTime;

    rst->SetVertexShader(0);
    m_Context->ExecuteManagersOnPostSpriteRender(dev);

    dev->m_DevicePostCallbacksTimeProfiler.Reset();
    rst->SetVertexShader(0);
    auto &postCallbacks = dev->m_PostRenderCallBacks.m_PostCallBacks;
    for (auto it = postCallbacks.Begin(); it != postCallbacks.End(); ++it) {
        auto &cb = *it;
        ((CK_RENDERCALLBACK) cb.callback)(dev, cb.argument);
    }
    dev->m_Stats.DevicePostCallbacks = dev->m_DevicePostCallbacksTimeProfiler.Current() + dev->m_Stats.DevicePostCallbacks;

    dev->m_Stats.ObjectsRenderTime = dev->m_Stats.ObjectsRenderTime -
                                     (dev->m_Stats.SceneTraversalTime +
                                      dev->m_Stats.TransparentObjectsSortTime +
                                      dev->m_Stats.ObjectsCallbacksTime + dev->m_Stats.SkinTime);
    dev->m_Stats.SpriteTime = dev->m_Stats.SpriteTime - dev->m_Stats.SpriteCallbacksTime;

    *g_UpdateTransparency = FALSE;
    return CK_OK;
}

void CKRenderedScene::SetDefaultRenderStates(CKRasterizerContext *rst) {
    CP_CALL_METHOD_ORIG(SetDefaultRenderStates, rst);
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
            if (target)
                camera->LookAt(VxVector(0.0f, 0.0f, 0.0f), target);
        }
    }

    for (auto it = m_Lights.Begin(); it != m_Lights.End(); ++it) {
        CKLight *light = *it;
        if (light && light->GetClassID() == CKCID_TARGETLIGHT) {
            auto *target = light->GetTarget();
            if (target)
                light->LookAt(VxVector(0.0f, 0.0f, 0.0f), target);
        }
    }

    auto *dev = (CP_HOOK_CLASS_NAME(CKRenderContext) *) m_RenderContext;
    auto *attachedCamera = (CP_HOOK_CLASS_NAME(CKCamera) *) m_AttachedCamera;
    auto *rootEntity = (CP_HOOK_CLASS_NAME(CK3dEntity) *) m_RootEntity;

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

    auto *dev = (CP_HOOK_CLASS_NAME(CKRenderContext) *) m_RenderContext;
    auto *attachedCamera = (CP_HOOK_CLASS_NAME(CKCamera) *) m_AttachedCamera;
    auto *rootEntity = (CP_HOOK_CLASS_NAME(CK3dEntity) *) m_RootEntity;

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

void CKRenderedScene::UpdateViewportSize(CKBOOL force, CK_RENDER_FLAGS Flags) {
    CP_CALL_METHOD_ORIG(UpdateViewportSize, force, Flags);
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
    assert(base != nullptr);

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

void CKRenderedScene::Unhook() {
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
}

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
    CP_CALL_METHOD_ORIG(NoTestsTraversal, Dev, Flags);
}

void CKSceneGraphNode::AddNode(CKSceneGraphNode *node) {
    CP_CALL_METHOD_ORIG(AddNode, node);
}

void CKSceneGraphNode::RemoveNode(CKSceneGraphNode *node) {
    CP_CALL_METHOD_ORIG(RemoveNode, node);
}

void CKSceneGraphNode::SortNodes() {
    CP_CALL_METHOD_ORIG(SortNodes);
}

CKDWORD CKSceneGraphNode::Rebuild() {
    return CP_CALL_METHOD_ORIG(Rebuild);
}

void CKSceneGraphNode::SetAsPotentiallyVisible() {
    CP_CALL_METHOD_ORIG(SetAsPotentiallyVisible);
}

void CKSceneGraphNode::SetAsInsideFrustum() {
    CP_CALL_METHOD_ORIG(SetAsInsideFrustum);
}

void CKSceneGraphNode::SetRenderContextMask(CKDWORD mask, CKBOOL b) {
    CP_CALL_METHOD_ORIG(SetRenderContextMask, mask, b);
}

void CKSceneGraphNode::SetPriority(int priority, int unused) {
    CP_CALL_METHOD_ORIG(SetPriority, priority, unused);
}

void CKSceneGraphNode::PrioritiesChanged() {
    CP_CALL_METHOD_ORIG(PrioritiesChanged);
}

void CKSceneGraphNode::EntityFlagsChanged(CKBOOL b) {
    CP_CALL_METHOD_ORIG(EntityFlagsChanged, b);
}

CKBOOL CKSceneGraphNode::IsToBeParsed() {
    return CP_CALL_METHOD_ORIG(IsToBeParsed);
}

CKBOOL CKSceneGraphNode::ComputeHierarchicalBox() {
    return CP_CALL_METHOD_ORIG(ComputeHierarchicalBox);
}

void CKSceneGraphNode::InvalidateBox(CKBOOL b) {
    CP_CALL_METHOD_ORIG(InvalidateBox, b);
}

void CKSceneGraphNode::sub_100789A0() {
    m_Flag &= ~1;
    m_Flag |= 2;

    for (int i = 0; i < m_ChildrenCount; ++i) {
        m_Children[i]->sub_100789A0();
    }
}

bool CKSceneGraphNode::Hook(void *base) {
    assert(base != nullptr);

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

void CKSceneGraphNode::Unhook() {
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
}

CP_DEFINE_METHOD_PTRS(CKSceneGraphRootNode, RenderTransparents);
CP_DEFINE_METHOD_PTRS(CKSceneGraphRootNode, SortTransparentObjects);
CP_DEFINE_METHOD_PTRS(CKSceneGraphRootNode, Check);
CP_DEFINE_METHOD_PTRS(CKSceneGraphRootNode, Clear);

void CKSceneGraphRootNode::RenderTransparents(CKRenderContext *Dev, CK_RENDER_FLAGS Flags) {
//    CP_CALL_METHOD_ORIG(RenderTransparents, Dev, Flags);

    auto *dev = (CP_HOOK_CLASS_NAME(CKRenderContext) *) Dev;
    auto *rm = (CP_HOOK_CLASS_NAME(CKRenderManager) *) dev->m_RenderManager;
    auto *rst = dev->m_RasterizerContext;

    ++dev->m_RenderTransparentCount;

    SetAsPotentiallyVisible();
    if (m_ChildrenCount != 0) {
        if ((m_Flag & 0x10) != 0)
            SortNodes();

        CKBOOL needClip = FALSE;

        if (m_Entity) {
            m_Entity->ModifyMoveableFlags(0, VX_MOVEABLE_EXTENTSUPTODATE);
            if (!m_Entity->IsInViewFrustrumHierarchic(dev)) {
                if (m_Entity->GetClassID() == CKCID_CHARACTER) {
                    VxBbox box = m_Bbox;
                    box.Max *= 2.0f;
                    box.Min *= 2.0f;
                    if (rst->ComputeBoxVisibility(box, TRUE))
                        m_Entity->ModifyMoveableFlags(VX_MOVEABLE_CHARACTERRENDERED, 0);
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

            if (m_Entity->GetClassID() == CKCID_CHARACTER)
                m_Entity->ModifyMoveableFlags(VX_MOVEABLE_CHARACTERRENDERED, 0);
            if ((dev->m_MaskFree & m_RenderContextMask) != 0 && m_Entity->IsToBeRendered()) {
                if (m_Entity->IsToBeRenderedLast()) {
                    if (sub_1000D290() || m_Entity->IsInViewFrustrum(dev, Flags)) {
                        m_TimeFpsCalc = dev->m_TimeFpsCalc;
                        rm->m_SceneGraphRootNode.AddTransparentObject(this);
                    }
                } else {
                    dev->m_Stats.SceneTraversalTime = dev->m_SceneTraversalTimeProfiler.Current() + dev->m_Stats.SceneTraversalTime;
                    m_Entity->Render(dev, Flags);
                    dev->m_SceneTraversalTimeProfiler.Reset();
                }
            }
        }

        if (sub_1000D290()) {
            for (int i = 0; i < m_ChildrenCount; ++i) {
                auto *child = m_Children[i];
                if ((dev->m_MaskFree & child->m_EntityFlags) != 0)
                    child->NoTestsTraversal(dev, Flags);
            }
        } else {
            for (int i = 0; i < m_ChildrenCount; ++i) {
                auto *child = m_Children[i];
                if ((dev->m_MaskFree & child->m_EntityFlags) != 0) {
                    auto *root = (CKSceneGraphRootNode *) child;
                    root->RenderTransparents(dev, Flags);
                }
            }
        }

        if (needClip) {
            rst->SetViewport(&dev->m_ViewportData);
            rst->SetTransformMatrix(VXMATRIX_PROJECTION, dev->m_ProjectionMatrix);
        }
    } else if (m_Entity && m_Entity->IsToBeRendered() &&
               (dev->m_MaskFree & m_RenderContextMask) != 0 &&
               m_Entity->IsInViewFrustrum(dev, Flags)) {
        if (m_Entity->IsToBeRenderedLast()) {
            m_TimeFpsCalc = dev->m_TimeFpsCalc;
            rm->m_SceneGraphRootNode.AddTransparentObject(this);
        } else {
            dev->m_Stats.SceneTraversalTime = dev->m_SceneTraversalTimeProfiler.Current() + dev->m_Stats.SceneTraversalTime;
            m_Entity->Render(dev, static_cast<CK_RENDER_FLAGS>(Flags | CK_RENDER_CLEARVIEWPORT));
            dev->m_SceneTraversalTimeProfiler.Reset();
        }
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
    if ((node->m_Flag & 0x20) == 0) {
        m_TransparentObjects.PushBack(CKTransparentObject(node));
        node->m_Flag |= 0x20u;
    }
}

bool CKSceneGraphRootNode::Hook(void *base) {
    assert(base != nullptr);

    CP_ADD_METHOD_HOOK(RenderTransparents, base, 0x77912);
    CP_ADD_METHOD_HOOK(SortTransparentObjects, base, 0x78183);
    CP_ADD_METHOD_HOOK(Check, base, 0x78129);
    CP_ADD_METHOD_HOOK(Clear, base, 0x765C0);

    return true;
}

void CKSceneGraphRootNode::Unhook() {
    CP_REMOVE_METHOD_HOOK(RenderTransparents);
    CP_REMOVE_METHOD_HOOK(SortTransparentObjects);
    CP_REMOVE_METHOD_HOOK(Check);
    CP_REMOVE_METHOD_HOOK(Clear);
}

CP_CLASS_VTABLE_NAME(CKRenderManager)<CKRenderManager> CP_HOOK_CLASS_NAME(CKRenderManager)::s_VTable = {};

#define CP_RENDER_MANAGER_METHOD_NAME(Name) CP_HOOK_CLASS_NAME(CKRenderManager)::CP_FUNC_HOOK_NAME(Name)

int CP_RENDER_MANAGER_METHOD_NAME(GetRenderDriverCount)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetRenderDriverCount);
}

VxDriverDesc *CP_RENDER_MANAGER_METHOD_NAME(GetRenderDriverDescription)(int Driver) {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetRenderDriverDescription, Driver);
}

void CP_RENDER_MANAGER_METHOD_NAME(GetDesiredTexturesVideoFormat)(VxImageDescEx &VideoFormat) {
    CP_CALL_METHOD_PTR(this, s_VTable.GetDesiredTexturesVideoFormat, VideoFormat);
}

void CP_RENDER_MANAGER_METHOD_NAME(SetDesiredTexturesVideoFormat)(VxImageDescEx &VideoFormat) {
    CP_CALL_METHOD_PTR(this, s_VTable.SetDesiredTexturesVideoFormat, VideoFormat);
}

CKRenderContext *CP_RENDER_MANAGER_METHOD_NAME(GetRenderContext)(int pos) {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetRenderContext, pos);
}

CKRenderContext *CP_RENDER_MANAGER_METHOD_NAME(GetRenderContextFromPoint)(CKPOINT &pt) {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetRenderContextFromPoint, pt);
}

int CP_RENDER_MANAGER_METHOD_NAME(GetRenderContextCount)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetRenderContextCount);
}

void CP_RENDER_MANAGER_METHOD_NAME(Process)() {
    CP_CALL_METHOD_PTR(this, s_VTable.Process);
}

void CP_RENDER_MANAGER_METHOD_NAME(FlushTextures)() {
    CP_CALL_METHOD_PTR(this, s_VTable.FlushTextures);
}

CKRenderContext *CP_RENDER_MANAGER_METHOD_NAME(CreateRenderContext)(void *Window, int Driver, CKRECT *rect, CKBOOL Fullscreen, int Bpp, int Zbpp, int StencilBpp, int RefreshRate) {
    auto *rc = CP_CALL_METHOD_PTR(this, s_VTable.CreateRenderContext, Window, Driver, rect, Fullscreen, Bpp, Zbpp, StencilBpp, RefreshRate);
    RenderHook::HookRenderContext(rc);
    Overlay::ImGuiInitRenderer(m_Context);
    return rc;
}

CKERROR CP_RENDER_MANAGER_METHOD_NAME(DestroyRenderContext)(CKRenderContext *context) {
    Overlay::ImGuiShutdownRenderer(m_Context);
    RenderHook::UnhookRenderContext(context);
    return CP_CALL_METHOD_PTR(this, s_VTable.DestroyRenderContext, context);
}

void CP_RENDER_MANAGER_METHOD_NAME(RemoveRenderContext)(CKRenderContext *context) {
    CP_CALL_METHOD_PTR(this, s_VTable.RemoveRenderContext, context);
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

bool CP_HOOK_CLASS_NAME(CKRenderManager)::Hook(CKRenderManager *man) {
    if (!man)
        return false;

    utils::LoadVTable<CP_CLASS_VTABLE_NAME(CKRenderManager)<CKRenderManager>>(man, s_VTable);

#define HOOK_RENDER_MANAGER_VIRTUAL_METHOD(Instance, Name) \
    utils::HookVirtualMethod(Instance, &CP_HOOK_CLASS_NAME(CKRenderManager)::CP_FUNC_HOOK_NAME(Name), (offsetof(CP_CLASS_VTABLE_NAME(CKRenderManager)<CKRenderManager>, Name) / sizeof(void*)))

    // HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, GetRenderDriverCount);
    // HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, GetRenderDriverDescription);
    // HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, GetDesiredTexturesVideoFormat);
    // HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, SetDesiredTexturesVideoFormat);
    // HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, GetRenderContext);
    // HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, GetRenderContextFromPoint);
    // HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, GetRenderContextCount);
    // HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, Process);
    // HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, FlushTextures);
    HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, CreateRenderContext);
    HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, DestroyRenderContext);
    // HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, RemoveRenderContext);
    // HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, CreateVertexBuffer);
    // HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, DestroyVertexBuffer);
    // HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, SetRenderOptions);
    // HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, GetEffectDescription);
    // HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, GetEffectCount);
    // HOOK_RENDER_MANAGER_VIRTUAL_METHOD(man, AddEffect);

#undef HOOK_RENDER_MANAGER_VIRTUAL_METHOD

    void *base = utils::GetModuleBaseAddress("CK2_3D.dll");
    assert(base != nullptr);

    CKRenderedScene::Hook(base);
    CKSceneGraphNode::Hook(base);
    CKSceneGraphRootNode::Hook(base);

    CP_HOOK_CLASS_NAME(CK3dEntity)::Hook(nullptr, base);
    CP_HOOK_CLASS_NAME(CKLight)::Hook(nullptr, base);
    g_UpdateTransparency = utils::ForceReinterpretCast<CKBOOL *>(base, 0x90CCC);

    return true;
}

void CP_HOOK_CLASS_NAME(CKRenderManager)::Unhook(CKRenderManager *man) {
    if (man)
        utils::SaveVTable<CP_CLASS_VTABLE_NAME(CKRenderManager)<CKRenderManager>>(man, s_VTable);

    CKRenderedScene::Unhook();
    CKSceneGraphNode::Unhook();
    CKSceneGraphRootNode::Unhook();

    CP_HOOK_CLASS_NAME(CK3dEntity)::Unhook(nullptr);
    CP_HOOK_CLASS_NAME(CKLight)::Unhook(nullptr);
}

bool CP_HOOK_CLASS_NAME(CKRenderContext)::s_DisableRender = false;
bool CP_HOOK_CLASS_NAME(CKRenderContext)::s_EnableWidescreenFix = false;
CP_CLASS_VTABLE_NAME(CKRenderContext)<CKRenderContext> CP_HOOK_CLASS_NAME(CKRenderContext)::s_VTable = {};
CP_DEFINE_METHOD_PTRS(CP_HOOK_CLASS_NAME(CKRenderContext), CallSprite3DBatches);
CP_DEFINE_METHOD_PTRS(CP_HOOK_CLASS_NAME(CKRenderContext), UpdateProjection);
CP_DEFINE_METHOD_PTRS(CP_HOOK_CLASS_NAME(CKRenderContext), SetClipRect);

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
    CP_CALL_METHOD_PTR(this, s_VTable.DetachAll);
}

void CP_RENDER_CONTEXT_METHOD_NAME(ForceCameraSettingsUpdate)() {
    CP_CALL_METHOD_PTR(this, s_VTable.ForceCameraSettingsUpdate);
}

void CP_RENDER_CONTEXT_METHOD_NAME(PrepareCameras)(CK_RENDER_FLAGS Flags) {
    CP_CALL_METHOD_PTR(this, s_VTable.PrepareCameras, Flags);
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

//    return CP_CALL_METHOD_PTR(this, s_VTable.Render, Flags);

    VxTimeProfiler renderProfiler;

    if (!m_Active)
        return CKERR_RENDERCONTEXTINACTIVE;
    if (!m_RasterizerContext)
        return CKERR_INVALIDRENDERCONTEXT;
    if (!Flags)
        Flags = m_RenderFlags;

    CKTimeManager *tm = m_Context->GetTimeManager();

    if ((tm->GetLimitOptions() & CK_FRAMERATE_SYNC) != 0) {
        Flags = static_cast<CK_RENDER_FLAGS>(Flags & CK_RENDER_CLEARBACK);
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

    ++m_TimeFpsCalc;

    float renderTime = m_RenderTimeProfiler.Current();
    if (renderTime <= 1000.0f) {
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
    return CP_CALL_METHOD_PTR(this, s_VTable.GoFullScreen, Width, Height, Bpp, Driver, RefreshRate);
}

CKERROR CP_RENDER_CONTEXT_METHOD_NAME(StopFullScreen)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.StopFullScreen);
}

CKBOOL CP_RENDER_CONTEXT_METHOD_NAME(IsFullScreen)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.IsFullScreen);
}

int CP_RENDER_CONTEXT_METHOD_NAME(GetDriverIndex)() {
    return CP_CALL_METHOD_PTR(this, s_VTable.GetDriverIndex);
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
    return CP_CALL_METHOD_PTR(this, s_VTable.Resize, PosX, PosY, SizeX, SizeY, Flags);
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

void CKRenderContextHook::CallSprite3DBatches() {
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

    VxRect rect(0.0f, 0.0f, (float) m_WindowRect.right, (float) m_WindowRect.bottom);
    auto *background = Get2dRoot(TRUE);
    background->SetRect(rect);
    auto *foreground = Get2dRoot(FALSE);
    foreground->SetRect(rect);

    return TRUE;
}

void CP_HOOK_CLASS_NAME(CKRenderContext)::SetClipRect(VxRect &rect) {
    CP_CALL_METHOD_ORIG(SetClipRect, rect);
}

void CP_HOOK_CLASS_NAME(CKRenderContext)::SetFullViewport(CKViewportData &data, int width, int height) {
    data.ViewY = 0;
    data.ViewX = 0;
    data.ViewWidth = width;
    data.ViewHeight = height;
}

bool CP_HOOK_CLASS_NAME(CKRenderContext)::Hook(CKRenderContext *rc) {
    if (!rc)
        return false;

    utils::LoadVTable<CP_CLASS_VTABLE_NAME(CKRenderContext)<CKRenderContext>>(rc, s_VTable);

#define HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(Instance, Name) \
utils::HookVirtualMethod(Instance, &CP_HOOK_CLASS_NAME(CKRenderContext)::CP_FUNC_HOOK_NAME(Name), (offsetof(CP_CLASS_VTABLE_NAME(CKRenderContext)<CKRenderContext>, Name) / sizeof(void*)))

    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, AddObject);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, RemoveObject);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, IsObjectAttached);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, Compute3dRootObjects);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, Compute2dRootObjects);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, Get2dRoot);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, DetachAll);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, ForceCameraSettingsUpdate);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, PrepareCameras);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, Clear);
    HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, DrawScene);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, BackToFront);
    HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, Render);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, AddPreRenderCallBack);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, RemovePreRenderCallBack);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, AddPostRenderCallBack);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, RemovePostRenderCallBack);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, AddPostSpriteRenderCallBack);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, RemovePostSpriteRenderCallBack);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetDrawPrimitiveStructure);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetDrawPrimitiveIndices);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, Transform);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, TransformVertices);
    HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GoFullScreen);
    HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, StopFullScreen);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, IsFullScreen);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetDriverIndex);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, ChangeDriver);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetWindowHandle);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, ScreenToClient);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, ClientToScreen);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetWindowRect);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetWindowRect);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetHeight);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetWidth);
    HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, Resize);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetViewRect);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetViewRect);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetPixelFormat);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetState);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetState);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetTexture);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetTextureStageState);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetRasterizerContext);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetClearBackground);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetClearBackground);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetClearZBuffer);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetClearZBuffer);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetGlobalRenderMode);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetGlobalRenderMode);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetCurrentRenderOptions);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetCurrentRenderOptions);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, ChangeCurrentRenderOptions);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetCurrentExtents);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetCurrentExtents);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetAmbientLightRGB);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetAmbientLight);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetAmbientLight);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetFogMode);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetFogStart);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetFogEnd);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetFogDensity);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetFogColor);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetFogMode);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetFogStart);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetFogEnd);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetFogDensity);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetFogColor);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, DrawPrimitive);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetWorldTransformationMatrix);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetProjectionTransformationMatrix);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetViewTransformationMatrix);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetWorldTransformationMatrix);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetProjectionTransformationMatrix);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetViewTransformationMatrix);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetUserClipPlane);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetUserClipPlane);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, Pick);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, PointPick);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, RectPick);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, AttachViewpointToCamera);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, DetachViewpointFromCamera);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetAttachedCamera);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetViewpoint);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetBackgroundMaterial);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetBoundingBox);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetStats);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetCurrentMaterial);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, Activate);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, DumpToMemory);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, CopyToVideo);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, DumpToFile);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetDirectXInfo);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, WarnEnterThread);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, WarnExitThread);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, Pick2D);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetRenderTarget);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, AddRemoveSequence);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetTransparentMode);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, AddDirtyRect);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, RestoreScreenBackup);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetStencilFreeMask);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, UsedStencilBits);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetFirstFreeStencilBits);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, LockCurrentVB);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, ReleaseCurrentVB);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetTextureMatrix);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, SetStereoParameters);
    // HOOK_RENDER_CONTEXT_VIRTUAL_METHOD(rc, GetStereoParameters);

#undef HOOK_RENDER_CONTEXT_VIRTUAL_METHOD

    void *base = utils::GetModuleBaseAddress("CK2_3D.dll");
    assert(base != nullptr);

    CP_ADD_METHOD_HOOK(CallSprite3DBatches, base, 0x6DC61);
    CP_ADD_METHOD_HOOK(UpdateProjection, base, 0x6C68D);
    CP_ADD_METHOD_HOOK(SetClipRect, base, 0x6C808);

    return true;
}

void CP_HOOK_CLASS_NAME(CKRenderContext)::Unhook(CKRenderContext *rc) {
    if (rc)
        utils::SaveVTable<CP_CLASS_VTABLE_NAME(CKRenderContext)<CKRenderContext>>(rc, s_VTable);

    CP_REMOVE_METHOD_HOOK(CallSprite3DBatches);
    CP_REMOVE_METHOD_HOOK(UpdateProjection);
    CP_REMOVE_METHOD_HOOK(SetClipRect);
}

void CP_HOOK_CLASS_NAME(CKRenderObject)::RemoveFromRenderContext(CKRenderContext *rc) {
    auto *dev = (CP_HOOK_CLASS_NAME(CKRenderContext) *) rc;

    m_Mask &= ~dev->m_MaskFree;
    if (CKIsChildClassOf(this, CKCID_3DENTITY) && !m_Context->IsInClearAll()) {
        auto *ent = (CP_HOOK_CLASS_NAME(CK3dEntity) *) this;
        if (dev->m_Start)
            ent->m_SceneGraphNode->m_RenderContextMask = m_Mask;
        else
            ent->m_SceneGraphNode->SetRenderContextMask(m_Mask, 0);
    }
}

CP_DEFINE_METHOD_PTRS(CP_HOOK_CLASS_NAME(CK3dEntity), WorldMatrixChanged);

void CP_HOOK_CLASS_NAME(CK3dEntity)::WorldMatrixChanged(CKBOOL invalidateBox, CKBOOL inverse) {
    CP_CALL_METHOD_ORIG_FORCE(CK3dEntity, WorldMatrixChanged, invalidateBox, inverse);
}

bool CP_HOOK_CLASS_NAME(CK3dEntity)::Hook(void *vtable, void *base) {
    assert(base != nullptr);

    CP_ADD_METHOD_HOOK(WorldMatrixChanged, base, 0x62D6);

    return true;
}

void CP_HOOK_CLASS_NAME(CK3dEntity)::Unhook(void *vtable) {
    CP_REMOVE_METHOD_HOOK(WorldMatrixChanged);
}

CP_DEFINE_METHOD_PTRS(CP_HOOK_CLASS_NAME(CKLight), Setup);

CKBOOL CP_HOOK_CLASS_NAME(CKLight)::Setup(CKRasterizerContext *rst, int index) {
    return CP_CALL_METHOD_ORIG_FORCE(CKLight, Setup, rst, index);
}

bool CP_HOOK_CLASS_NAME(CKLight)::Hook(void *vtable, void *base) {
    assert(base != nullptr);

    CP_ADD_METHOD_HOOK(Setup, base, 0x1B0C2);

    return true;
}

void CP_HOOK_CLASS_NAME(CKLight)::Unhook(void *vtable) {
    CP_REMOVE_METHOD_HOOK(Setup);
}

namespace RenderHook {
    void HookRenderManager(CKRenderManager *man) {
        CP_HOOK_CLASS_NAME(CKRenderManager)::Hook(man);
    }

    void UnhookRenderManager(CKRenderManager *man) {
        CP_HOOK_CLASS_NAME(CKRenderManager)::Unhook(man);
    }

    void HookRenderContext(CKRenderContext *rc) {
        CP_HOOK_CLASS_NAME(CKRenderContext)::Hook(rc);
    }

    void UnhookRenderContext(CKRenderContext *rc) {
        CP_HOOK_CLASS_NAME(CKRenderContext)::Unhook(rc);
    }

    void DisableRender(bool disable) {
        CP_HOOK_CLASS_NAME(CKRenderContext)::s_DisableRender = disable;
    }

    void EnableWidescreenFix(bool enable) {
        CP_HOOK_CLASS_NAME(CKRenderContext)::s_EnableWidescreenFix = enable;
    }
}