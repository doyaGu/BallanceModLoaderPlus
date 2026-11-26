#include "Behavior/Engine/Message.h"
#include "Behavior/Engine/Scope.h"
#include "Behavior/Install/Installations.h"

#include <gtest/gtest.h>

namespace {
using namespace BML::Behavior::Internal;

CKBehavior *rejectGraph = nullptr;
CKBehavior *foreignParent = nullptr;

class BehaviorTransactionFailure : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(CKStartUp(), CK_OK);
        ASSERT_EQ(CKCreateContext(&context, nullptr, 0,
                                  CK_CONFIG_DISABLEDINPUT | CK_CONFIG_DISABLEDSOUND), CK_OK);
        graph = MakeGraph("target");
        ASSERT_NE(graph, nullptr);
    }
    void TearDown() override {
        rejectGraph = foreignParent = nullptr;
        if (context)
            CKCloseContext(context);
        CKShutdown();
    }
    CKBehavior *MakeGraph(const char *name) {
        auto *node = CKBehavior::Cast(context->CreateObject(CKCID_BEHAVIOR, const_cast<char *>(name)));
        if (node)
            node->UseGraph();
        return node;
    }
    static ObjectRef Issue(const void *object) {
        if (!object)
            return {};
        auto *native = static_cast<const CKObject *>(object);
        return {1, static_cast<std::uint32_t>(const_cast<CKObject *>(native)->GetID()), 1};
    }

    CKContext *context = nullptr;
    CKBehavior *graph = nullptr;
};

} // namespace

namespace BML::Behavior::Internal::Engine {
// Reparent the graph after a real node was created so the inverse detects a conflict.
int Send(CKContext *context, CKBehavior *behavior, CKDWORD message,
         const CKBehaviorContext *frame) {
    if (message == CKM_BEHAVIOREDITED && behavior == rejectGraph && rejectGraph) {
        rejectGraph = nullptr;
        behavior->SetParent(foreignParent);
        return CKERR_INVALIDPARAMETER;
    }
    Scope scope(context, behavior, frame);
    return behavior->CallCallbackFunction(message);
}
} // namespace BML::Behavior::Internal::Engine

namespace {
TEST_F(BehaviorTransactionFailure, DeferredSubmissionSeparatesApplyAndInverseErrors) {
    Runtime runtime(context, Issue);
    auto source = MakeCKGraphSource(context, runtime, Issue);
    Installations edits(context, runtime, nullptr, *source,
        [&](const ObjectRef &ref) { return context->GetObject(ref.Slot); }, Issue);
    SessionOwner owner{"mod", 1};
    auto program = std::make_shared<Program>();
    program->AddGraph("child");
    Installations::Target target;
    target.Graph = Issue(graph);
    target.Body = program;
    target.Binding = 1;
    std::uintptr_t submission = 0;
    auto *manager = context->GetBehaviorManager();
    auto *previous = manager->m_CurrentBehavior;
    manager->m_CurrentBehavior = graph;
    auto submitted = edits.SubmitEdit(owner, "fault", false, {target}, {}, submission);
    manager->m_CurrentBehavior = previous;
    ASSERT_TRUE(submitted) << submitted.Message;
    ASSERT_NE(submission, 0u);
    ASSERT_EQ(graph->GetSubBehaviorCount(), 0);
    foreignParent = MakeGraph("changed parent");
    ASSERT_NE(foreignParent, nullptr);
    rejectGraph = graph;
    edits.ProcessFrame();
    Installations::SubmissionInfo info;
    ASSERT_TRUE(edits.ReadSubmission(owner, submission, info));
    EXPECT_EQ(info.State, Installations::SubmissionState::Failed);
    EXPECT_EQ(info.ApplyFailure.Code, Error::CallbackFailed);
    EXPECT_EQ(info.ApplyFailure.CkError, CKERR_INVALIDPARAMETER);
    EXPECT_EQ(info.ApplyFailure.Details.Stage, Phase::Edit);
    EXPECT_EQ(info.RestoreFailure.Code, Error::RevertConflict);
    EXPECT_EQ(info.RestoreFailure.CkError, CKERR_INVALIDOBJECT);
    EXPECT_EQ(info.RestoreFailure.Details.Stage, Phase::Teardown);
    EXPECT_EQ(graph->GetSubBehaviorCount(), 1);
    graph->SetParent(nullptr);
    auto closed = edits.CloseSubmission(owner, submission);
    EXPECT_TRUE(closed) << closed.Message;
    EXPECT_EQ(graph->GetSubBehaviorCount(), 0);
}

TEST_F(BehaviorTransactionFailure, QueuedNativePatchKeepsItsApplyDiagnostic) {
    Runtime runtime(context, Issue);
    auto source = MakeCKGraphSource(context, runtime, Issue);
    Installations edits(context, runtime, nullptr, *source,
        [&](const ObjectRef &ref) { return context->GetObject(ref.Slot); }, Issue);
    SessionOwner owner{"mod", 1};
    Ops ops;
    ASSERT_TRUE(edits.Begin(owner, graph, "queued fault", ops));
    Node child;
    ASSERT_TRUE(edits.AddGraph(ops, "child", 0, child));
    auto *manager = context->GetBehaviorManager();
    auto *previous = manager->m_CurrentBehavior;
    manager->m_CurrentBehavior = graph;
    PatchId patch = 0;
    auto queued = edits.Apply(owner, ops, patch);
    manager->m_CurrentBehavior = previous;
    ASSERT_TRUE(queued);
    foreignParent = MakeGraph("changed parent");
    rejectGraph = graph;
    edits.ProcessFrame();
    PatchInfo info;
    ASSERT_TRUE(edits.Read(owner, patch, info));
    EXPECT_EQ(info.ApplyFailure.Code, Error::CallbackFailed);
    EXPECT_EQ(info.RestoreFailure.Code, Error::RevertConflict);
    graph->SetParent(nullptr);
    EXPECT_TRUE(edits.Close(owner, patch));
    EXPECT_EQ(graph->GetSubBehaviorCount(), 0);
}

TEST_F(BehaviorTransactionFailure, ScriptSelectionRetainsBothFaultsAndCanCloseAfterRepair) {
    Runtime runtime(context, Issue);
    auto source = MakeCKGraphSource(context, runtime, Issue);
    Installations edits(context, runtime, nullptr, *source,
        [&](const ObjectRef &ref) { return context->GetObject(ref.Slot); }, Issue);
    SessionOwner owner{"mod", 1};
    auto program = std::make_shared<Program>();
    program->AddGraph("child");
    Installations::Rule rule;
    rule.Scripts.Name = "script";
    rule.Body = program;
    rule.Binding = 1;
    std::uintptr_t submission = 0;
    ASSERT_TRUE(edits.LoadScript("script", Issue(graph)));
    ASSERT_TRUE(edits.SubmitEdit(owner, "each fault", false, {}, {rule}, submission));
    foreignParent = MakeGraph("changed parent");
    rejectGraph = graph;
    EXPECT_FALSE(edits.ProcessFrame());
    Installations::SubmissionInfo info;
    ASSERT_TRUE(edits.ReadSubmission(owner, submission, info));
    EXPECT_EQ(info.ApplyFailure.Code, Error::CallbackFailed);
    EXPECT_EQ(info.RestoreFailure.Code, Error::RevertConflict);
    EXPECT_EQ(graph->GetSubBehaviorCount(), 1);
    graph->SetParent(nullptr);
    EXPECT_TRUE(edits.CloseSubmission(owner, submission));
    EXPECT_EQ(graph->GetSubBehaviorCount(), 0);
}
} // namespace
