#include <gtest/gtest.h>
#include <array>
#include <memory>
#include "CKAll.h"
#include "TextureTestObserver.h"
#include "CKRecordingRasterizer.h"
#include "RCK2dEntity.h"
#include "RCKMaterial.h"
#include "RCKCamera.h"
#include "Hooks/RenderHook.h"
#include "Behavior/Edit/Journal.h"
#include "Behavior/Engine/Access.h"
#include "Behavior/Engine/Graph.h"
#include "Behavior/Engine/Message.h"
#include "Behavior/PrototypeCatalog.h"
#include "UI/imgui_impl_ck2.h"
#include "imgui_internal.h"

using namespace BML::Behavior::Internal;

namespace {
class DrawObserver : public TextureTestObserver {
public:
    int Draws = 0, Clears = 0;
    std::array<VxVector4, 4> DrawPositions{};
    int DrawPositionCount = 0;
    CKViewportData DrawViewport;
    CKBOOL Clear(CKDWORD, CKDWORD, float, CKDWORD, int, CKRECT *) override { ++Clears; return TRUE; }
    CKBOOL BeginScene() override { return TRUE; }
    CKBOOL EndScene() override { return TRUE; }
    CKBOOL BeginOverlayPhase() override { return TRUE; }
    void InitDefaultRenderStatesValue() override {}
    CKBOOL EnableLight(CKDWORD, CKBOOL) override { return TRUE; }
    CKBOOL DrawPrimitive(VXPRIMITIVETYPE, CKWORD *, int, VxDrawPrimitiveData *data) override {
        ++Draws;
        DrawViewport = Viewport;
        DrawPositionCount = 0;
        if (data && !(data->Flags & CKRST_DP_VBUFFER) && data->PositionPtr &&
            data->PositionStride >= sizeof(VxVector4)) {
            XPtrStrided<VxVector4> positions(data->PositionPtr, data->PositionStride);
            for (int i = 0; i < data->VertexCount && i < int(DrawPositions.size()); ++i) {
                DrawPositions[i] = *positions;
                ++DrawPositionCount;
                ++positions;
            }
        }
        return TRUE;
    }
};
class TestRenderContext : public RCKRenderContext {
public:
    bool FailGeometry = false;
    bool RejectOptionsRemoval = false;
    CKBOOL RemoveRenderOptionsCallBack(CK_RENDEROPTIONSCALLBACK function, void *argument) override {
        return RejectOptionsRemoval ? FALSE : RCKRenderContext::RemoveRenderOptionsCallBack(function, argument);
    }
    explicit TestRenderContext(CKContext *context) : RCKRenderContext(context) {}
    VxDrawPrimitiveData *GetDrawPrimitiveStructure(CKRST_DPFLAGS flags, int count) override {
        return FailGeometry ? nullptr : RCKRenderContext::GetDrawPrimitiveStructure(flags, count);
    }
};
class SdlEngineTest : public ::testing::Test {
protected:
    CKContext *Context = nullptr;
    RCKRenderManager *Manager = nullptr;
    TestRenderContext *Device = nullptr;
    DrawObserver State;
    CKRasterizerDriver Driver;
    void SetUp() override {
        SetProcessorSpecific_FunctionsPtr();
        ASSERT_EQ(CK_OK, CKStartUp());
        CKCLASSREGISTERCID(RCKRenderObject, CKCID_BEOBJECT);
        CKCLASSREGISTERCID(RCK3dEntity, CKCID_RENDEROBJECT);
        CKCLASSREGISTERCID(RCK2dEntity, CKCID_RENDEROBJECT);
        CKCLASSREGISTERCID(RCKMaterial, CKCID_BEOBJECT);
        CKCLASSREGISTERCID(RCKTexture, CKCID_BEOBJECT);
        CKCLASSREGISTERCID(RCKCamera, CKCID_3DENTITY);
        CKCLASSREGISTERCID(RCKRenderContext, CKCID_OBJECT);
        CKBuildClassHierarchyTable();
        ASSERT_EQ(CK_OK, CKCreateContext(&Context, nullptr, 0, 0));
        Manager = new RCKRenderManager(Context);
        Manager->m_Drivers[0].CapsUpToDate = TRUE;
        Manager->m_Drivers[0].RasterizerDriver = &Driver;
        Manager->m_Drivers[0].Caps3D.MaxTextureWidth = 1024;
        Manager->m_Drivers[0].Caps3D.MaxTextureHeight = 1024;
        VxImageDescEx format; VxPixelFormat2ImageDesc(_32_ARGB8888, format);
        Manager->m_Drivers[0].TextureFormats.PushBack(format);
        ASSERT_TRUE(Manager->RegisterRasterizerContext(&State, 0));
        Device = new TestRenderContext(Context);
        Device->m_RasterizerContext = &State;
        Device->m_RasterizerDriver = &Driver;
        Device->m_DriverIndex = 0;
        Device->m_Active = TRUE;
        Device->m_Settings.m_Rect = CKRECT{0, 0, 640, 360};
        VxRect rect(0, 0, 640, 360); Device->SetViewRect(rect);
        Context->m_BehaviorContext.CurrentRenderContext = Device;
    }
    void TearDown() override {
        if (ImGui::GetCurrentContext()) {
            if (ImGui::GetIO().BackendRendererUserData) ImGui_ImplCK2_Shutdown();
            ImGui::DestroyContext();
        }
        if (Device) {
            Device->m_RasterizerContext = nullptr;
            Device->m_RasterizerDriver = nullptr;
            delete Device;
        }
        if (Manager) Manager->ForgetRasterizerContext(&State);
        if (Context) CKCloseContext(Context);
        CKShutdown();
    }
    CKTexture *Texture() {
        auto *texture = static_cast<CKTexture *>(Context->CreateObject(CKCID_TEXTURE));
        EXPECT_TRUE(texture->Create(2, 2, 32));
        return texture;
    }
    void RenderUi(CKTexture *texture, ImVec2 displayPos = ImVec2(0, 0),
                  ImVec2 framebufferScale = ImVec2(1, 1)) {
        const ImVec2 displaySize(640 / framebufferScale.x, 360 / framebufferScale.y);
        ImDrawList list(ImGui::GetDrawListSharedData());
        list._ResetForNewFrame();
        list.PushClipRect(displayPos, ImVec2(displayPos.x + displaySize.x, displayPos.y + displaySize.y));
        list.PushTexture(ImTextureRef(reinterpret_cast<ImTextureID>(texture)));
        list.AddRectFilled(ImVec2(displayPos.x + 10, displayPos.y + 10),
                           ImVec2(displayPos.x + 80, displayPos.y + 80), IM_COL32_WHITE);
        ImDrawData data;
        data.Valid = true;
        data.DisplayPos = displayPos;
        data.DisplaySize = displaySize;
        data.FramebufferScale = framebufferScale;
        ASSERT_GT(list.VtxBuffer.Size, 0);
        ASSERT_GT(list.IdxBuffer.Size, 0);
        data.AddDrawList(&list);
        data.CmdListsCount = data.CmdLists.Size;
        ASSERT_EQ(1, data.CmdListsCount);
        ImGui_ImplCK2_RenderDrawData(&data);
    }
};

struct BehaviorCallbackTrace {
    int Calls = 0;
    int Provider = 0;
    CKBehaviorContext Frame{};
    CKBehavior *Current = nullptr;
};

TEST_F(SdlEngineTest, OrphanLinkRetirementUsesNativeCleanupForSurvivingChild) {
    auto *graph = static_cast<CKBehavior *>(Context->CreateObject(
        CKCID_BEHAVIOR, "deleted graph", CK_OBJECTCREATION_DYNAMIC));
    auto *child = static_cast<CKBehavior *>(Context->CreateObject(CKCID_BEHAVIOR));
    ASSERT_NE(nullptr, graph);
    ASSERT_NE(nullptr, child);
    graph->UseGraph();
    child->UseGraph();
    ASSERT_EQ(CK_OK, graph->AddSubBehavior(child));
    CKBehaviorIO *source = child->CreateOutput("surviving source");
    CKBehaviorIO *destination = graph->CreateOutput("deleted destination");
    CKBehaviorIO *replacementDestination = child->CreateInput("surviving destination");
    ASSERT_NE(nullptr, source);
    ASSERT_NE(nullptr, destination);
    ASSERT_NE(nullptr, replacementDestination);
    auto *link = static_cast<CKBehaviorLink *>(Context->CreateObject(CKCID_BEHAVIORLINK));
    ASSERT_NE(nullptr, link);
    ASSERT_EQ(CK_OK, link->SetInBehaviorIO(source));
    ASSERT_EQ(CK_OK, link->SetOutBehaviorIO(destination));
    ASSERT_EQ(CK_OK, graph->AddSubBehaviorLink(link));
    ASSERT_EQ(link, graph->RemoveSubBehaviorLink(link));
    const CK_ID graphID = graph->GetID(), childID = child->GetID(),
                destinationID = destination->GetID(), linkID = link->GetID();

    ASSERT_EQ(CK_OK, Context->DestroyObject(graph));
    ASSERT_EQ(nullptr, Context->GetObject(graphID));
    ASSERT_EQ(child, Context->GetObject(childID));
    ASSERT_EQ(nullptr, Context->GetObject(destinationID));
    ASSERT_EQ(nullptr, link->GetOutBehaviorIO());
    ASSERT_EQ(1, source->GetLinkCount());
    ASSERT_EQ(CK_OK, Engine::DestroyOrphanLink(Context, link));
    EXPECT_EQ(nullptr, Context->GetObject(linkID));
    EXPECT_EQ(0, source->GetLinkCount());

    // The source array remains owned and reusable by CK2 after retirement.
    auto *replacement = static_cast<CKBehaviorLink *>(Context->CreateObject(CKCID_BEHAVIORLINK));
    ASSERT_NE(nullptr, replacement);
    ASSERT_EQ(CK_OK, replacement->SetInBehaviorIO(source));
    ASSERT_EQ(CK_OK, replacement->SetOutBehaviorIO(replacementDestination));
    ASSERT_EQ(1, source->GetLinkCount());
    EXPECT_EQ(replacement, source->GetLink(0));
    ASSERT_EQ(CK_OK, Context->DestroyObject(replacement));
    EXPECT_EQ(0, source->GetLinkCount());
}

TEST_F(SdlEngineTest, OrphanLinkRetirementFollowsNativeDeferredDeletionTiming) {
    auto *graph = static_cast<CKBehavior *>(Context->CreateObject(CKCID_BEHAVIOR));
    ASSERT_NE(nullptr, graph);
    graph->UseGraph();
    CKBehaviorIO *source = graph->CreateInput("source");
    CKBehaviorIO *destination = graph->CreateOutput("destination");
    ASSERT_NE(nullptr, source);
    ASSERT_NE(nullptr, destination);
    auto *link = static_cast<CKBehaviorLink *>(Context->CreateObject(CKCID_BEHAVIORLINK));
    ASSERT_NE(nullptr, link);
    ASSERT_EQ(CK_OK, link->SetInBehaviorIO(source));
    ASSERT_EQ(CK_OK, link->SetOutBehaviorIO(destination));
    ASSERT_EQ(CK_OK, graph->AddSubBehaviorLink(link));
    ASSERT_EQ(link, graph->RemoveSubBehaviorLink(link));
    const CK_ID linkID = link->GetID();

    Context->m_DeferDestroyObjects = TRUE;
    EXPECT_EQ(CK_OK, Engine::DestroyOrphanLink(Context, link));
    EXPECT_EQ(link, Context->GetObject(linkID));
    EXPECT_EQ(source, link->GetInBehaviorIO());
    EXPECT_EQ(destination, link->GetOutBehaviorIO());
    EXPECT_EQ(1, source->GetLinkCount());
    Context->m_DeferDestroyObjects = FALSE;
    ASSERT_EQ(CK_OK, Context->m_ObjectManager->PostProcess());
    EXPECT_EQ(nullptr, Context->GetObject(linkID));
    EXPECT_EQ(0, source->GetLinkCount());
}

int RecordBehaviorCallback(const CKBehaviorContext &frame) {
    auto *trace = static_cast<BehaviorCallbackTrace *>(frame.CallbackArg);
    ++trace->Calls;
    trace->Provider = 1;
    trace->Frame = frame;
    trace->Current = frame.Context->GetBehaviorManager()->m_CurrentBehavior;
    return 37;
}

int ReplacementBehaviorCallback(const CKBehaviorContext &frame) {
    auto *trace = static_cast<BehaviorCallbackTrace *>(frame.CallbackArg);
    ++trace->Calls;
    trace->Provider = 2;
    trace->Frame = frame;
    return 53;
}

TEST_F(SdlEngineTest, PublicSetParentRestoresReferencesWithoutChangingGraphMembership) {
    BehaviorCallbackTrace trace;
    std::unique_ptr<CKBehaviorPrototype> prototype(CreateCKBehaviorPrototype("SDL parent test"));
    prototype->SetBehaviorCallbackFct(RecordBehaviorCallback,
                                    CKCB_BEHAVIORATTACH | CKCB_BEHAVIORDETACH, &trace);
    auto *parent = static_cast<CKBehavior *>(Context->CreateObject(CKCID_BEHAVIOR));
    auto *child = static_cast<CKBehavior *>(Context->CreateObject(CKCID_BEHAVIOR));
    parent->UseGraph();
    ASSERT_EQ(CK_OK, child->InitFromPrototype(prototype.get()));
    const auto flags = child->GetFlags();
    CKBeObject *const owner = child->GetOwner();

    child->SetParent(parent);
    EXPECT_EQ(parent, child->GetParent());
    EXPECT_EQ(0, parent->GetSubBehaviorCount());
    EXPECT_EQ(owner, child->GetOwner());
    EXPECT_EQ(flags, child->GetFlags());
    EXPECT_EQ(0, trace.Calls);

    Engine::SetParentId(child, nullptr);
    EXPECT_EQ(nullptr, child->GetParent());
    Engine::SetParentId(child, parent);
    EXPECT_EQ(parent, child->GetParent());
    child->SetParent(nullptr);
    EXPECT_EQ(nullptr, child->GetParent());
    EXPECT_EQ(0, trace.Calls);
    Engine::SetParentId(nullptr, parent);

    child->SetCallbackFunction(nullptr);
    ASSERT_EQ(CK_OK, parent->AddSubBehavior(child));
    EXPECT_EQ(1, parent->GetSubBehaviorCount());
    EXPECT_EQ(child, parent->RemoveSubBehavior(child));
    EXPECT_EQ(nullptr, child->GetParent());
}

TEST_F(SdlEngineTest, BehaviorCreateUsesPublicCallbackAndRestoresDispatchContext) {
    BehaviorCallbackTrace trace;
    std::unique_ptr<CKBehaviorPrototype> prototype(CreateCKBehaviorPrototype("SDL callback test"));
    prototype->SetBehaviorCallbackFct(RecordBehaviorCallback, CKCB_BEHAVIORCREATE, &trace);
    auto *behavior = static_cast<CKBehavior *>(Context->CreateObject(CKCID_BEHAVIOR));
    auto *outer = static_cast<CKBehavior *>(Context->CreateObject(CKCID_BEHAVIOR));
    ASSERT_EQ(CK_OK, behavior->InitFromPrototype(prototype.get()));
    EXPECT_EQ(0, trace.Calls);

    int outerArgument = 0;
    Context->m_BehaviorContext.Behavior = outer;
    Context->m_BehaviorContext.CallbackArg = &outerArgument;
    Context->m_BehaviorContext.CallbackMessage = CKM_BEHAVIORRESET;
    Context->m_BehaviorContext.CurrentRenderContext = nullptr;
    Context->GetBehaviorManager()->m_CurrentBehavior = outer;
    CKBehaviorContext frame = Context->m_BehaviorContext;
    frame.CurrentRenderContext = Device;

    EXPECT_EQ(37, Engine::Send(Context, behavior, CKM_BEHAVIORCREATE, &frame));
    EXPECT_EQ(1, trace.Calls);
    EXPECT_EQ(Context, trace.Frame.Context);
    EXPECT_EQ(behavior, trace.Frame.Behavior);
    EXPECT_EQ(&trace, trace.Frame.CallbackArg);
    EXPECT_EQ(CKM_BEHAVIORCREATE, trace.Frame.CallbackMessage);
    EXPECT_EQ(Device, trace.Frame.CurrentRenderContext);
    EXPECT_EQ(behavior, trace.Current);
    EXPECT_EQ(outer, Context->m_BehaviorContext.Behavior);
    EXPECT_EQ(&outerArgument, Context->m_BehaviorContext.CallbackArg);
    EXPECT_EQ(CKM_BEHAVIORRESET, Context->m_BehaviorContext.CallbackMessage);
    EXPECT_EQ(nullptr, Context->m_BehaviorContext.CurrentRenderContext);
    EXPECT_EQ(outer, Context->GetBehaviorManager()->m_CurrentBehavior);
}

TEST_F(SdlEngineTest, BehaviorMessagesRespectMasksAndLiveCallbackReplacement) {
    BehaviorCallbackTrace first, second;
    std::unique_ptr<CKBehaviorPrototype> prototype(CreateCKBehaviorPrototype("SDL callback mask test"));
    prototype->SetBehaviorCallbackFct(RecordBehaviorCallback, CKCB_BEHAVIORCREATE, &first);
    auto *behavior = static_cast<CKBehavior *>(Context->CreateObject(CKCID_BEHAVIOR));
    ASSERT_EQ(CK_OK, behavior->InitFromPrototype(prototype.get()));
    EXPECT_EQ(CK_OK, Engine::Send(Context, behavior, CKM_BEHAVIORRESET));
    EXPECT_EQ(0, first.Calls);
    EXPECT_EQ(37, Engine::Send(Context, behavior, CKM_BEHAVIORCREATE));

    behavior->SetCallbackFunction(ReplacementBehaviorCallback);
    EXPECT_EQ(53, Engine::Send(Context, behavior, CKM_BEHAVIORCREATE));
    EXPECT_EQ(2, first.Calls);
    EXPECT_EQ(2, first.Provider);

    prototype->SetBehaviorCallbackFct(RecordBehaviorCallback, CKCB_BEHAVIOREDITED, &second);
    ASSERT_EQ(CK_OK, behavior->InitFctPtrFromPrototype(prototype.get()));
    EXPECT_EQ(CK_OK, Engine::Send(Context, behavior, CKM_BEHAVIORCREATE));
    EXPECT_EQ(0, second.Calls);
    EXPECT_EQ(37, Engine::Send(Context, behavior, CKM_BEHAVIOREDITED));
    EXPECT_EQ(1, second.Calls);
    EXPECT_EQ(&second, second.Frame.CallbackArg);
}

TEST_F(SdlEngineTest, BehaviorMessagesHandleMissingTargetsAndGraphsWithoutCallbacks) {
    auto *graph = static_cast<CKBehavior *>(Context->CreateObject(CKCID_BEHAVIOR));
    graph->UseGraph();
    EXPECT_EQ(CKERR_INVALIDOBJECT, Engine::Send(nullptr, graph, CKM_BEHAVIORCREATE));
    EXPECT_EQ(CKERR_INVALIDOBJECT, Engine::Send(Context, nullptr, CKM_BEHAVIORCREATE));
    EXPECT_EQ(CK_OK, Engine::Send(Context, graph, CKM_BEHAVIORCREATE));
    EXPECT_EQ(CK_OK, Engine::Send(Context, graph, CKM_MAX_BEHAVIOR_CALLBACKS));
}

struct ChildAttachState {
    CKBehavior *RedirectParent = nullptr;
    CKBehavior *ChangePriority = nullptr;
    int Priority = 0;
    int Calls = 0;
    CKERROR Result = CK_OK;
};

int MutateChildOnAttach(const CKBehaviorContext &frame) {
    auto &state = *static_cast<ChildAttachState *>(frame.CallbackArg);
    ++state.Calls;
    if (state.ChangePriority)
        state.ChangePriority->SetPriority(state.Priority);
    if (state.RedirectParent)
        frame.Behavior->SetParent(state.RedirectParent);
    return state.Result;
}

CKBehavior *CreateGraph(CKContext *context) {
    auto *graph = static_cast<CKBehavior *>(context->CreateObject(CKCID_BEHAVIOR));
    graph->UseGraph();
    return graph;
}

CKBehavior *CreateChild(CKContext *context, int priority = 0) {
    auto *child = static_cast<CKBehavior *>(context->CreateObject(CKCID_BEHAVIOR));
    child->SetPriority(priority);
    return child;
}

TEST_F(SdlEngineTest, ChildAttachKeepsExistingEqualPriorityOrder) {
    CKBehavior *graph = CreateGraph(Context);
    for (int i = 0; i < 3; ++i)
        ASSERT_EQ(CK_OK, graph->AddSubBehavior(CreateChild(Context, 5)));
    ASSERT_EQ(CK_OK, graph->MoveSubBehavior(0, 2));
    CKBehavior *before[] = {graph->GetSubBehavior(0), graph->GetSubBehavior(1),
                            graph->GetSubBehavior(2)};
    CKBehavior *child = CreateChild(Context, 5);
    ASSERT_EQ(CK_OK, Engine::AddChild(graph, child));
    ASSERT_EQ(4, graph->GetSubBehaviorCount());
    for (int i = 0; i < 3; ++i)
        EXPECT_EQ(before[i], graph->GetSubBehavior(i));
    EXPECT_EQ(child, graph->GetSubBehavior(3));
    EXPECT_EQ(graph, child->GetParent());
}

TEST_F(SdlEngineTest, ChildAttachUsesFinalCallbackPriorityAndKeepsTiesStable) {
    CKBehavior *graph = CreateGraph(Context);
    auto *owner = static_cast<CKBeObject *>(Context->CreateObject(CKCID_BEOBJECT));
    ASSERT_NE(nullptr, owner);
    ASSERT_EQ(CK_OK, graph->SetOwner(owner, FALSE));
    CKBehavior *high = CreateChild(Context, 10), *low = CreateChild(Context, 0);
    ASSERT_EQ(CK_OK, graph->AddSubBehavior(high));
    ASSERT_EQ(CK_OK, graph->AddSubBehavior(CreateChild(Context, 5)));
    ASSERT_EQ(CK_OK, graph->AddSubBehavior(CreateChild(Context, 5)));
    ASSERT_EQ(CK_OK, graph->AddSubBehavior(low));
    ASSERT_EQ(CK_OK, graph->MoveSubBehavior(1, 2));
    CKBehavior *first = graph->GetSubBehavior(1), *second = graph->GetSubBehavior(2);
    ChildAttachState state;
    std::unique_ptr<CKBehaviorPrototype> prototype(CreateCKBehaviorPrototype("SDL attach priority"));
    prototype->SetApplyToClassID(CKCID_BEOBJECT);
    prototype->SetBehaviorCallbackFct(MutateChildOnAttach, CKCB_BEHAVIORATTACH, &state);
    CKBehavior *child = CreateChild(Context, -10);
    ASSERT_EQ(CK_OK, child->InitFromPrototype(prototype.get()));
    state.ChangePriority = child;
    state.Priority = 5;
    ASSERT_EQ(CK_OK, Engine::AddChild(graph, child));
    EXPECT_EQ(1, state.Calls);
    CKBehavior *expected[] = {high, first, second, child, low};
    ASSERT_EQ(5, graph->GetSubBehaviorCount());
    for (int i = 0; i < 5; ++i)
        EXPECT_EQ(expected[i], graph->GetSubBehavior(i));
    EXPECT_EQ(owner, child->GetOwner());
}

TEST_F(SdlEngineTest, RejectedChildAttachRetainsOriginalRelations) {
    CKBehavior *graph = CreateGraph(Context);
    auto *owner = static_cast<CKBeObject *>(Context->CreateObject(CKCID_BEOBJECT));
    ASSERT_EQ(CK_OK, graph->SetOwner(owner, FALSE));
    ChildAttachState state;
    state.Result = CKERR_INVALIDPARAMETER;
    std::unique_ptr<CKBehaviorPrototype> prototype(CreateCKBehaviorPrototype("SDL rejected attach"));
    prototype->SetApplyToClassID(CKCID_BEOBJECT);
    prototype->SetBehaviorCallbackFct(MutateChildOnAttach, CKCB_BEHAVIORATTACH, &state);
    CKBehavior *child = CreateChild(Context);
    ASSERT_EQ(CK_OK, child->InitFromPrototype(prototype.get()));
    const auto flags = child->GetFlags();
    const auto compatible = graph->GetCompatibleClassID();
    EXPECT_EQ(CKERR_INVALIDPARAMETER, Engine::AddChild(graph, child));
    EXPECT_EQ(1, state.Calls);
    EXPECT_EQ(nullptr, child->GetParent());
    EXPECT_EQ(nullptr, child->GetOwner());
    EXPECT_EQ(flags, child->GetFlags());
    EXPECT_EQ(0, graph->GetSubBehaviorCount());
    EXPECT_EQ(compatible, graph->GetCompatibleClassID());
}

TEST_F(SdlEngineTest, InconsistentChildAttachRollsBackThroughPublicRelations) {
    CKBehavior *graph = CreateGraph(Context), *other = CreateGraph(Context);
    auto *owner = static_cast<CKBeObject *>(Context->CreateObject(CKCID_BEOBJECT));
    ASSERT_EQ(CK_OK, graph->SetOwner(owner, FALSE));
    for (int i = 0; i < 2; ++i)
        ASSERT_EQ(CK_OK, graph->AddSubBehavior(CreateChild(Context, 5)));
    CKBehavior *first = graph->GetSubBehavior(0), *second = graph->GetSubBehavior(1);
    ChildAttachState state;
    state.RedirectParent = other;
    std::unique_ptr<CKBehaviorPrototype> prototype(CreateCKBehaviorPrototype("SDL inconsistent attach"));
    prototype->SetApplyToClassID(CKCID_BEOBJECT);
    prototype->SetBehaviorCallbackFct(MutateChildOnAttach, CKCB_BEHAVIORATTACH, &state);
    CKBehavior *child = CreateChild(Context, 5);
    ASSERT_EQ(CK_OK, child->InitFromPrototype(prototype.get()));
    const auto flags = child->GetFlags();
    EXPECT_EQ(CKERR_INVALIDOBJECT, Engine::AddChild(graph, child));
    EXPECT_EQ(nullptr, child->GetParent());
    EXPECT_EQ(nullptr, child->GetOwner());
    EXPECT_EQ(flags, child->GetFlags());
    ASSERT_EQ(2, graph->GetSubBehaviorCount());
    EXPECT_EQ(first, graph->GetSubBehavior(0));
    EXPECT_EQ(second, graph->GetSubBehavior(1));
    EXPECT_EQ(0, other->GetSubBehaviorCount());
}

TEST_F(SdlEngineTest, FailedChildAttachPreservesCallbackPriorityChanges) {
    CKBehavior *graph = CreateGraph(Context), *other = CreateGraph(Context);
    auto *owner = static_cast<CKBeObject *>(Context->CreateObject(CKCID_BEOBJECT));
    ASSERT_EQ(CK_OK, graph->SetOwner(owner, FALSE));
    for (int i = 0; i < 2; ++i)
        ASSERT_EQ(CK_OK, graph->AddSubBehavior(CreateChild(Context, 5)));
    CKBehavior *first = graph->GetSubBehavior(0), *second = graph->GetSubBehavior(1);
    ChildAttachState state;
    state.RedirectParent = other;
    state.ChangePriority = second;
    state.Priority = 8;
    std::unique_ptr<CKBehaviorPrototype> prototype(CreateCKBehaviorPrototype("SDL rollback priority"));
    prototype->SetApplyToClassID(CKCID_BEOBJECT);
    prototype->SetBehaviorCallbackFct(MutateChildOnAttach, CKCB_BEHAVIORATTACH, &state);
    CKBehavior *child = CreateChild(Context, 5);
    ASSERT_EQ(CK_OK, child->InitFromPrototype(prototype.get()));
    EXPECT_EQ(CKERR_INVALIDOBJECT, Engine::AddChild(graph, child));
    EXPECT_EQ(nullptr, child->GetParent());
    ASSERT_EQ(2, graph->GetSubBehaviorCount());
    EXPECT_EQ(second, graph->GetSubBehavior(0));
    EXPECT_EQ(first, graph->GetSubBehavior(1));
    EXPECT_EQ(8, second->GetPriority());
}

TEST_F(SdlEngineTest, ChildAttachRejectsInvalidMembershipAndSupportsReattachment) {
    CKBehavior *graph = CreateGraph(Context), *other = CreateGraph(Context);
    CKBehavior *child = CreateChild(Context), *block = CreateChild(Context);
    block->UseFunction();
    EXPECT_EQ(CKERR_INVALIDPARAMETER, Engine::AddChild(nullptr, child));
    EXPECT_EQ(CKERR_INVALIDPARAMETER, Engine::AddChild(graph, nullptr));
    EXPECT_EQ(CKERR_INVALIDPARAMETER, Engine::AddChild(graph, graph));
    EXPECT_EQ(CKERR_INVALIDPARAMETER, Engine::AddChild(block, child));
    ASSERT_EQ(CK_OK, Engine::AddChild(graph, child));
    EXPECT_EQ(CKERR_INVALIDPARAMETER, Engine::AddChild(graph, child));
    EXPECT_EQ(CKERR_INVALIDPARAMETER, Engine::AddChild(other, child));
    ASSERT_EQ(child, graph->RemoveSubBehavior(child));
    ASSERT_EQ(nullptr, child->GetParent());
    ASSERT_EQ(CK_OK, Engine::AddChild(other, child));
    EXPECT_EQ(other, child->GetParent());
    EXPECT_EQ(0, graph->GetSubBehaviorCount());
    EXPECT_EQ(1, other->GetSubBehaviorCount());
}

TEST_F(SdlEngineTest, RenderAdapterRestoresOnlyItsProjectionOption) {
    Device->SetCurrentRenderOptions(CK_RENDER_CLEARZ | CK_RENDER_ADJUSTFOV);
    RenderHook::EnableWidescreenFix(false);
    RenderHook first, second;
    ASSERT_TRUE(first.Attach(Device));
    EXPECT_TRUE(first.IsAttached());
    EXPECT_FALSE(second.Attach(Device));
    EXPECT_FALSE(Device->GetCurrentRenderOptions() & CK_RENDER_ADJUSTFOV);
    RenderHook::EnableWidescreenFix(true);
    EXPECT_TRUE(Device->GetCurrentRenderOptions() & CK_RENDER_ADJUSTFOV);
    EXPECT_TRUE(RenderHook::IsSkipRenderAvailable());
    const CKDWORD options = Device->GetCurrentRenderOptions();
    RenderHook::SkipNextRender();
    EXPECT_EQ(options, Device->GetCurrentRenderOptions());
    Device->ChangeCurrentRenderOptions(CK_RENDER_CLEARBACK, 0);
    ASSERT_TRUE(first.Detach());
    EXPECT_EQ(CKDWORD(CK_RENDER_CLEARZ | CK_RENDER_CLEARBACK | CK_RENDER_ADJUSTFOV),
              Device->GetCurrentRenderOptions());
    EXPECT_FALSE(RenderHook::IsSkipRenderAvailable());
    RenderHook::EnableWidescreenFix(false);
}

TEST_F(SdlEngineTest, JournalRestoresNodeAndSourceOrderThroughPublicMoves) {
    auto *graph = static_cast<CKBehavior *>(Context->CreateObject(CKCID_BEHAVIOR));
    graph->UseGraph();
    for (int i = 0; i < 3; ++i) {
        auto *node = static_cast<CKBehavior *>(Context->CreateObject(CKCID_BEHAVIOR));
        ASSERT_EQ(CK_OK, graph->AddSubBehavior(node));
    }
    CKBehaviorIO *source = graph->CreateInput("source");
    CKBehaviorIO *sink = graph->GetSubBehavior(0)->CreateInput("sink");
    for (int i = 0; i < 3; ++i) {
        auto *link = static_cast<CKBehaviorLink *>(Context->CreateObject(CKCID_BEHAVIORLINK));
        ASSERT_EQ(CK_OK, link->SetInBehaviorIO(source));
        ASSERT_EQ(CK_OK, link->SetOutBehaviorIO(sink));
        ASSERT_EQ(CK_OK, graph->AddSubBehaviorLink(link));
    }
    NativeGraphOrder before;
    ASSERT_TRUE(CaptureOrder(graph, before));
    ASSERT_EQ(CK_OK, graph->MoveSubBehavior(0, 2));
    ASSERT_EQ(CK_OK, source->MoveLink(0, 2));
    RevertSubject subject;
    EXPECT_FALSE(ValidateOrder(Context, graph, before, subject));
    ASSERT_TRUE(ArrangeOrder(Context, graph, before, {}, {}, true));
    EXPECT_TRUE(ValidateOrder(Context, graph, before, subject));
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(before.Nodes[i].Address, graph->GetSubBehavior(i));
        EXPECT_EQ(before.Sources[0].Links[i].Address, source->GetLink(i));
    }
}

const CKGUID prototypeGuid(0x41df192a, 0x584a81ab);
void RegisterDeclarations(XObjectDeclarationArray *declarations) {
    auto *declaration = CreateCKObjectDeclaration("SDL provider test");
    declaration->SetGuid(prototypeGuid);
    declaration->SetVersion(1);
    CKStoreDeclaration(declarations, declaration);
}
class DeclarationManager : public CKPluginManager {
public:
    using CKPluginManager::InitializeBehaviors;
};
TEST_F(SdlEngineTest, CatalogDetectsSameVersionReloadWithoutACountChange) {
    DeclarationManager manager;
    CKPluginEntry entry;
    entry.m_IndexInCategory = -1;
    entry.m_BehaviorsInfo = new CKPluginEntryBehaviorsData;
    manager.InitializeBehaviors(RegisterDeclarations, entry);
    PrototypeCatalog catalog(MakeCKPrototypeSource(Context));
    ASSERT_TRUE(catalog.TracksRetirement());
    PrototypeRef before;
    ASSERT_TRUE(catalog.Resolve({prototypeGuid, 0}, before));
    const int count = CKGetPrototypeDeclarationCount();
    ASSERT_EQ(CK_OK, CKRemovePrototypeDeclaration(CKGetObjectDeclarationFromGuid(prototypeGuid)));
    manager.InitializeBehaviors(RegisterDeclarations, entry);
    ASSERT_EQ(count, CKGetPrototypeDeclarationCount());
    EXPECT_FALSE(catalog.Validate(before));
    PrototypeRef after;
    ASSERT_TRUE(catalog.Resolve({prototypeGuid, 0}, after));
    EXPECT_NE(before.Generation, after.Generation);
    ASSERT_EQ(CK_OK, CKRemovePrototypeDeclaration(CKGetObjectDeclarationFromGuid(prototypeGuid)));
    EXPECT_FALSE(catalog.Validate(after));
}

TEST_F(SdlEngineTest, UiAlignsGeometryWithCk2PixelCenters) {
    ImGui::CreateContext();
    ASSERT_TRUE(ImGui_ImplCK2_Init(Context));
    RenderUi(Texture());
    ASSERT_EQ(4, State.DrawPositionCount);
    EXPECT_FLOAT_EQ(9.5f, State.DrawPositions[0].x);
    EXPECT_FLOAT_EQ(9.5f, State.DrawPositions[0].y);
    EXPECT_FLOAT_EQ(79.5f, State.DrawPositions[2].x);
    EXPECT_FLOAT_EQ(79.5f, State.DrawPositions[2].y);
}

TEST_F(SdlEngineTest, UiProjectsDisplayCoordinatesIntoFramebufferPixels) {
    ImGui::CreateContext();
    ASSERT_TRUE(ImGui_ImplCK2_Init(Context));
    CKTexture *texture = Texture();
    VxRect originalViewport(4, 8, 620, 350);
    Device->SetViewRect(originalViewport);
    for (const ImVec2 scale : {ImVec2(2, 2), ImVec2(1.5f, 1.25f)}) {
        SCOPED_TRACE(::testing::Message() << "framebuffer scale " << scale.x << ", " << scale.y);
        RenderUi(texture, ImVec2(100, 50), scale);
        ASSERT_EQ(4, State.DrawPositionCount);
        EXPECT_FLOAT_EQ(10 * scale.x - 0.5f, State.DrawPositions[0].x);
        EXPECT_FLOAT_EQ(10 * scale.y - 0.5f, State.DrawPositions[0].y);
        EXPECT_FLOAT_EQ(80 * scale.x - 0.5f, State.DrawPositions[2].x);
        EXPECT_FLOAT_EQ(80 * scale.y - 0.5f, State.DrawPositions[2].y);
        EXPECT_EQ(0u, State.DrawViewport.ViewX);
        EXPECT_EQ(0u, State.DrawViewport.ViewY);
        EXPECT_EQ(640u, State.DrawViewport.ViewWidth);
        EXPECT_EQ(360u, State.DrawViewport.ViewHeight);
        VxRect restored;
        Device->GetViewRect(restored);
        EXPECT_FLOAT_EQ(originalViewport.left, restored.left);
        EXPECT_FLOAT_EQ(originalViewport.top, restored.top);
        EXPECT_FLOAT_EQ(originalViewport.right, restored.right);
        EXPECT_FLOAT_EQ(originalViewport.bottom, restored.bottom);
    }
}

TEST_F(SdlEngineTest, UiRestoresTextureStagesTransformsAndAlphaOnSuccessAndFailure) {
    CKTexture *background = Texture(), *ui = Texture();
    ASSERT_TRUE(Device->SetTexture(background));
    VxMatrix matrix = VxMatrix::Identity(); matrix[3][0] = .375f;
    Device->SetTextureMatrix(matrix);
    Device->SetTextureStageState(CKRST_TSS_MINFILTER, VXTEXTUREFILTER_NEAREST);
    Device->SetTextureStageState(CKRST_TSS_ADDRESS, VXTEXTURE_ADDRESSWRAP);
    Device->SetTextureStageState(CKRST_TSS_OP, CKRST_TOP_ADD);
    Device->SetTextureStageState(CKRST_TSS_ARG1, CKRST_TA_CURRENT);
    Device->SetState(VXRENDERSTATE_ALPHATESTENABLE, TRUE);
    Device->SetState(VXRENDERSTATE_ALPHAFUNC, VXCMP_LESS);
    Device->SetState(VXRENDERSTATE_ALPHAREF, 73);
    Device->SetState(VXRENDERSTATE_BLENDOP, VXBLENDOP_ADD);
    ImGui::CreateContext();
    ASSERT_TRUE(ImGui_ImplCK2_Init(Context));
    const auto check = [&] {
        EXPECT_EQ(background, Device->GetTexture());
        EXPECT_FLOAT_EQ(.375f, Device->GetTextureMatrix()[3][0]);
        EXPECT_EQ(CKDWORD(VXTEXTUREFILTER_NEAREST), Device->GetTextureStageState(CKRST_TSS_MINFILTER));
        EXPECT_EQ(CKDWORD(VXTEXTURE_ADDRESSWRAP), Device->GetTextureStageState(CKRST_TSS_ADDRESS));
        EXPECT_EQ(CKDWORD(CKRST_TOP_ADD), Device->GetTextureStageState(CKRST_TSS_OP));
        EXPECT_EQ(CKDWORD(CKRST_TA_CURRENT), Device->GetTextureStageState(CKRST_TSS_ARG1));
        EXPECT_EQ(CKDWORD(TRUE), Device->GetState(VXRENDERSTATE_ALPHATESTENABLE));
        EXPECT_EQ(CKDWORD(VXCMP_LESS), Device->GetState(VXRENDERSTATE_ALPHAFUNC));
        EXPECT_EQ(73u, Device->GetState(VXRENDERSTATE_ALPHAREF));
    };
    RenderUi(ui);
    ImGui_ImplCK2_Diagnostics diagnostics{};
    ASSERT_TRUE(ImGui_ImplCK2_GetDiagnostics(&diagnostics));
    EXPECT_GT(State.Draws, 0) << "lists=" << diagnostics.DrawListCount
        << " commands=" << diagnostics.DrawCommandCount
        << " uploads=" << diagnostics.GeometryUploadCount
        << " failures=" << diagnostics.GeometryUploadFailureCount
        << " binds=" << diagnostics.TextureBindCallCount;
    check();
    Device->FailGeometry = true;
    RenderUi(ui);
    check();
    Device->FailGeometry = false;
    CKTexture *invalid = Texture();
    State.CreateResult = FALSE;
    RenderUi(invalid); // Failed texture bind resets the whole stage inside CK2.
    check();
    State.CreateResult = TRUE;
}

TEST_F(SdlEngineTest, UiRestoresTheProductionFixedFunctionStateThroughPublicCk2Queries) {
    // The recorder runs the production fixed-function pipeline without a GPU.
    // Test code observes its device interface; the BML backend sees CK2 only.
    CKRecordingRasterizer rasterizer;
    ASSERT_TRUE(rasterizer.Start(nullptr));
    auto *driver = static_cast<CKRecordingRasterizerDriver *>(rasterizer.GetDriver(0));
    ASSERT_NE(nullptr, driver);
    CKTextureDesc format; VxPixelFormat2ImageDesc(_32_ARGB8888, format.Format);
    driver->AddTextureFormat(format);
    CKRasterizerContext *recording = driver->CreateContext();
    ASSERT_NE(nullptr, recording);
    ASSERT_TRUE(recording->Create(nullptr, 0, 0, 640, 360, 32, FALSE, 60, 24, 8));
    ASSERT_TRUE(Manager->RegisterRasterizerContext(recording, 0));
    struct RestoreContext {
        RCKRenderManager *Manager;
        TestRenderContext *Device;
        CKRasterizerContext *PreviousContext;
        CKRasterizerDriver *PreviousDriver;
        ~RestoreContext() {
            Manager->ForgetRasterizerContext(Device->m_RasterizerContext);
            Device->m_RasterizerContext = PreviousContext;
            Device->m_RasterizerDriver = PreviousDriver;
        }
    } restore{Manager, Device, &State, &Driver};
    Device->m_RasterizerContext = recording;
    Device->m_RasterizerDriver = driver;
    CKTexture *background = Texture(), *ui = Texture();
    ASSERT_TRUE(Device->SetTexture(background));
    ASSERT_TRUE(Device->SetTexture(background, FALSE, 1));
    Device->SetTextureStageState(CKRST_TSS_OP, CKRST_TOP_ADD, 1);
    VxMatrix matrix = VxMatrix::Identity(); matrix[3][0] = .25f;
    Device->SetTextureMatrix(matrix, 1);
    const CKDWORD operation = Device->GetTextureStageState(CKRST_TSS_OP, 1);
    const CKDWORD blend = Device->GetState(VXRENDERSTATE_BLENDOP);
    const CKDWORD alpha = Device->GetState(VXRENDERSTATE_ALPHATESTENABLE);
    ImGui::CreateContext();
    ASSERT_TRUE(ImGui_ImplCK2_Init(Context));
    ASSERT_TRUE(recording->BeginScene());
    RenderUi(ui);
    ImGui_ImplCK2_Diagnostics diagnostics{};
    ASSERT_TRUE(ImGui_ImplCK2_GetDiagnostics(&diagnostics));
    EXPECT_GT(diagnostics.DrawCallCount, 0u);
    EXPECT_EQ(background, Device->GetTexture());
    EXPECT_EQ(background, Device->GetTexture(1));
    EXPECT_EQ(operation, Device->GetTextureStageState(CKRST_TSS_OP, 1));
    EXPECT_FLOAT_EQ(.25f, Device->GetTextureMatrix(1)[3][0]);
    EXPECT_EQ(blend, Device->GetState(VXRENDERSTATE_BLENDOP));
    EXPECT_EQ(alpha, Device->GetState(VXRENDERSTATE_ALPHATESTENABLE));
    EXPECT_TRUE(recording->EndScene());
    ImGui_ImplCK2_Shutdown();
    ImGui::DestroyContext();
}

} // namespace


void ForeignRenderOptions(CKRenderContext *, CK_RENDER_FLAGS *, void *argument) {
    ++*static_cast<int *>(argument);
}
struct RequestDuringRender { int Calls = 0; bool Nested = false; CKERROR NestedResult = CKERR_INVALIDOPERATION; };
void RequestRenderSkip(CKRenderContext *context, void *argument) {
    auto &state = *static_cast<RequestDuringRender *>(argument);
    if (++state.Calls != 1) return;
    RenderHook::SkipNextRender();
    if (state.Nested)
        state.NestedResult = context->Render(static_cast<CK_RENDER_FLAGS>(CK_RENDER_SKIP3D | CK_RENDER_DOBACKTOFRONT));
}

TEST_F(SdlEngineTest, SkipRequestSurvivesProcessAndPauseButOnlySkipsOneRender) {
    auto *level = static_cast<CKLevel *>(Context->CreateObject(CKCID_LEVEL));
    ASSERT_NE(nullptr, level);
    Context->SetCurrentLevel(level);
    ASSERT_EQ(CK_OK, Context->Play());
    Device->SetCurrentRenderOptions(CK_RENDER_SKIPDRAWSCENE | CK_RENDER_DOBACKTOFRONT | CK_RENDER_CLEARBACK);
    const CKDWORD options = Device->GetCurrentRenderOptions();
    RenderHook hook; ASSERT_TRUE(hook.Attach(Device));
    RenderHook::SkipNextRender(); RenderHook::SkipNextRender();
    ASSERT_TRUE(hook.Attach(Device)); // An idempotent attach preserves the request.
    for (int i = 0; i < 3; ++i) EXPECT_EQ(CK_OK, Context->Process());
    EXPECT_EQ(CK_OK, Context->Pause());
    EXPECT_EQ(options, Device->GetCurrentRenderOptions());
    EXPECT_EQ(CK_OK, Device->Render());
    EXPECT_EQ(0, State.Clears); EXPECT_EQ(0, State.PresentCount);
    EXPECT_EQ(options, Device->GetCurrentRenderOptions());
    EXPECT_EQ(CK_OK, Device->Render());
    EXPECT_EQ(1, State.Clears); EXPECT_EQ(1, State.PresentCount);
    EXPECT_EQ(CK_OK, Device->Render());
    EXPECT_EQ(2, State.Clears); EXPECT_EQ(2, State.PresentCount);
    EXPECT_EQ(options, Device->GetCurrentRenderOptions());
}

TEST_F(SdlEngineTest, SkipRequestCoversExplicitOptionsAndIgnoresOtherContexts) {
    Device->SetCurrentRenderOptions(CK_RENDER_CLEARZ);
    RenderHook hook; ASSERT_TRUE(hook.Attach(Device));
    const CKDWORD options = Device->GetCurrentRenderOptions();
    RenderHook::SkipNextRender();
    TestRenderContext other(Context);
    EXPECT_EQ(CKERR_RENDERCONTEXTINACTIVE, other.Render());
    const auto flags = static_cast<CK_RENDER_FLAGS>(CK_RENDER_SKIPDRAWSCENE | CK_RENDER_DOBACKTOFRONT);
    EXPECT_EQ(CK_OK, Device->Render(flags)); EXPECT_EQ(0, State.PresentCount);
    EXPECT_EQ(CK_OK, Device->Render(flags)); EXPECT_EQ(1, State.PresentCount);
    EXPECT_EQ(options, Device->GetCurrentRenderOptions());
    // Direct helpers neither consume the request nor receive its local flag.
    RenderHook::SkipNextRender();
    EXPECT_EQ(CK_OK, Device->BackToFront(CK_RENDER_DOBACKTOFRONT)); EXPECT_EQ(2, State.PresentCount);
    EXPECT_EQ(CK_OK, Device->Render(flags)); EXPECT_EQ(2, State.PresentCount);
    EXPECT_EQ(CK_OK, Device->Render(flags)); EXPECT_EQ(3, State.PresentCount);
}

TEST_F(SdlEngineTest, RequestInsideRenderAffectsTheNextRenderIncludingNestedCalls) {
    const auto flags = static_cast<CK_RENDER_FLAGS>(CK_RENDER_SKIP3D | CK_RENDER_DOBACKTOFRONT);
    Device->SetCurrentRenderOptions(flags);
    RenderHook hook; ASSERT_TRUE(hook.Attach(Device));
    RequestDuringRender state;
    Device->AddPostSpriteRenderCallBack(RequestRenderSkip, &state);
    EXPECT_EQ(CK_OK, Device->Render()); EXPECT_EQ(1, State.PresentCount);
    EXPECT_EQ(CK_OK, Device->Render()); EXPECT_EQ(1, State.PresentCount);
    EXPECT_EQ(CK_OK, Device->Render()); EXPECT_EQ(2, State.PresentCount);
    EXPECT_EQ(2, state.Calls);
    Device->RemovePostSpriteRenderCallBack(RequestRenderSkip, &state);
    state = {}; state.Nested = true;
    Device->AddPostSpriteRenderCallBack(RequestRenderSkip, &state);
    EXPECT_EQ(CK_OK, Device->Render()); EXPECT_EQ(CK_OK, state.NestedResult);
    EXPECT_EQ(3, State.PresentCount); EXPECT_EQ(1, state.Calls);
    EXPECT_EQ(CK_OK, Device->Render()); EXPECT_EQ(4, State.PresentCount);
    EXPECT_EQ(2, state.Calls);
    EXPECT_EQ(CKDWORD(flags), Device->GetCurrentRenderOptions());
    Device->RemovePostSpriteRenderCallBack(RequestRenderSkip, &state);
}

TEST_F(SdlEngineTest, RenderHookRefusesAnOccupiedSlotWithoutChangingItsOwnerOrOptions) {
    int calls = 0;
    const CKDWORD options = CK_RENDER_SKIPDRAWSCENE | CK_RENDER_DOBACKTOFRONT | CK_RENDER_ADJUSTFOV;
    Device->SetCurrentRenderOptions(options);
    ASSERT_TRUE(Device->SetRenderOptionsCallBack(ForeignRenderOptions, &calls));
    RenderHook hook;
    EXPECT_FALSE(hook.Attach(Device)); EXPECT_FALSE(hook.IsAttached());
    EXPECT_FALSE(RenderHook::IsSkipRenderAvailable());
    EXPECT_EQ(options, Device->GetCurrentRenderOptions());
    EXPECT_TRUE(hook.Detach());
    EXPECT_EQ(CK_OK, Device->Render()); EXPECT_EQ(1, calls);
    ASSERT_TRUE(Device->RemoveRenderOptionsCallBack(ForeignRenderOptions, &calls));
    ASSERT_TRUE(hook.Attach(Device));
    EXPECT_TRUE(hook.Detach());
    EXPECT_TRUE(Device->SetRenderOptionsCallBack(ForeignRenderOptions, &calls));
    EXPECT_TRUE(Device->RemoveRenderOptionsCallBack(ForeignRenderOptions, &calls));
}

TEST_F(SdlEngineTest, ResetDetachAndContextChangesDiscardPendingRequests) {
    Device->SetCurrentRenderOptions(CK_RENDER_SKIPDRAWSCENE | CK_RENDER_DOBACKTOFRONT);
    RenderHook hook; ASSERT_TRUE(hook.Attach(Device));
    RenderHook::SkipNextRender(); RenderHook::ResetPendingSkip(nullptr);
    EXPECT_EQ(CK_OK, Device->Render()); EXPECT_EQ(0, State.PresentCount);
    RenderHook::SkipNextRender(); RenderHook::ResetPendingSkip(Context);
    EXPECT_EQ(CK_OK, Device->Render()); EXPECT_EQ(1, State.PresentCount);
    RenderHook::SkipNextRender(); ASSERT_TRUE(hook.Detach()); ASSERT_TRUE(hook.Attach(Device));
    EXPECT_EQ(CK_OK, Device->Render()); EXPECT_EQ(2, State.PresentCount);
    RenderHook::SkipNextRender();
    TestRenderContext other(Context);
    ASSERT_TRUE(hook.Attach(&other));
    EXPECT_EQ(CKERR_RENDERCONTEXTINACTIVE, other.Render());
    EXPECT_EQ(CK_OK, Device->Render()); EXPECT_EQ(3, State.PresentCount);
    ASSERT_TRUE(hook.Detach()); // other is destroyed before hook.
}

TEST_F(SdlEngineTest, FailedRenderHookRemovalRetainsOwnershipUntilRetry) {
    Device->SetCurrentRenderOptions(CK_RENDER_SKIPDRAWSCENE | CK_RENDER_DOBACKTOFRONT);
    RenderHook hook; ASSERT_TRUE(hook.Attach(Device));
    RenderHook::SkipNextRender(); Device->RejectOptionsRemoval = true;
    EXPECT_FALSE(hook.Detach()); EXPECT_TRUE(hook.IsAttached());
    EXPECT_TRUE(RenderHook::IsSkipRenderAvailable());
    EXPECT_EQ(CK_OK, Device->Render()); EXPECT_EQ(0, State.PresentCount);
    Device->RejectOptionsRemoval = false;
    EXPECT_TRUE(hook.Detach()); EXPECT_FALSE(hook.IsAttached());
    EXPECT_EQ(CK_OK, Device->Render()); EXPECT_EQ(1, State.PresentCount);
}

TEST_F(SdlEngineTest, RepeatedRenderHookLifetimesReleaseTheSlotAndPendingRequest) {
    Device->SetCurrentRenderOptions(CK_RENDER_SKIPDRAWSCENE | CK_RENDER_DOBACKTOFRONT);
    for (int i = 0; i < 5; ++i) {
        {
            RenderHook hook; ASSERT_TRUE(hook.Attach(Device));
            RenderHook::SkipNextRender();
        }
        EXPECT_FALSE(RenderHook::IsSkipRenderAvailable());
        EXPECT_EQ(CK_OK, Device->Render()); EXPECT_EQ(i + 1, State.PresentCount);
    }
}
