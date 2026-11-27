#include "Behavior/Engine/Message.h"
#include "Behavior/Engine/Scope.h"
#include "Behavior/Install/Installations.h"

#include <memory_resource>
#include <gtest/gtest.h>

namespace {
using namespace BML::Behavior::Internal;

CKBehavior *rejectGraph = nullptr;
CKBehavior *foreignParent = nullptr;

class FailingRegistry final : public std::pmr::memory_resource {
public:
    bool FailNext = true;
    unsigned Failures = 0;
private:
    void *do_allocate(std::size_t bytes, std::size_t alignment) override {
        if (std::exchange(FailNext, false)) {
            ++Failures;
            throw std::bad_alloc();
        }
        return std::pmr::new_delete_resource()->allocate(bytes, alignment);
    }
    void do_deallocate(void *memory, std::size_t bytes, std::size_t alignment) override {
        std::pmr::new_delete_resource()->deallocate(memory, bytes, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource &other) const noexcept override {
        return this == &other;
    }
};

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
    EXPECT_EQ(info.State, Installations::SubmissionState::Conflicted);
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

TEST_F(BehaviorTransactionFailure, RegistryAllocationFailureLeavesGraphAndPatchKeyReusable) {
    FailingRegistry registry;
    struct DefaultResource {
        std::pmr::memory_resource *Previous;
        ~DefaultResource() { std::pmr::set_default_resource(Previous); }
    } restore{std::pmr::set_default_resource(&registry)};
    Runtime runtime(context, Issue);
    auto source = MakeCKGraphSource(context, runtime, Issue);
    CKEdit editor(context, runtime, nullptr, *source);
    Ops edit;
    ASSERT_TRUE(editor.Begin(graph, {"mod", "same key"}, edit));
    Node child;
    ASSERT_TRUE(editor.AddGraph(edit, "child", 0, child));
    Patch patch;
    const Status failed = editor.Apply(edit, patch);
    EXPECT_EQ(registry.Failures, 1u);
    EXPECT_EQ(failed.CkError, CKERR_OUTOFMEMORY);
    EXPECT_FALSE(patch);
    EXPECT_EQ(graph->GetSubBehaviorCount(), 0);
    const Status retried = editor.Apply(edit, patch);
    ASSERT_TRUE(retried) << retried.Message;
    EXPECT_EQ(graph->GetSubBehaviorCount(), 1);
    EXPECT_TRUE(editor.Close(patch));
    EXPECT_EQ(graph->GetSubBehaviorCount(), 0);
}

TEST_F(BehaviorTransactionFailure, SubmissionRetriesOnlyMissingScriptAndRestoresFailedReplacement) {
    Runtime runtime(context, Issue);
    auto source = MakeCKGraphSource(context, runtime, Issue);
    Installations edits(context, runtime, nullptr, *source,
        [&](const ObjectRef &ref) { return context->GetObject(ref.Slot); }, Issue);
    const SessionOwner owner{"mod", 1};
    auto *energy = MakeGraph("energy");
    ASSERT_NE(energy, nullptr);
    ASSERT_TRUE(edits.LoadScript("Gameplay_Ingame", Issue(graph)));
    ASSERT_TRUE(edits.LoadScript("Gameplay_Energy", Issue(energy)));
    auto first = std::make_shared<Program>();
    first->AddGraph("ingame marker");
    auto second = std::make_shared<Program>();
    second->RequireOne(NodePattern("prerequisite"));
    second->AddGraph("energy marker");
    const Installations::Rule ingame{{"Gameplay_Ingame"}, 1, first};
    const Installations::Rule energyRule{{"Gameplay_Energy"}, 2, second};
    std::uintptr_t submission = 0;
    ASSERT_TRUE(edits.SubmitEdit(owner, "overclock retry", false, {}, {ingame, energyRule}, submission));
    EXPECT_FALSE(edits.ProcessFrame());
    Installations::SubmissionInfo info;
    ASSERT_TRUE(edits.ReadSubmission(owner, submission, info));
    EXPECT_EQ(info.State, Installations::SubmissionState::Partial);
    EXPECT_NE(info.ApplyFailure.Code, Error::None);
    Installations::InstalledEdit before;
    ASSERT_TRUE(edits.FindInstallation(owner, submission, Issue(graph), 1, before));
    ASSERT_EQ(energy->GetSubBehaviorCount(), 0);

    auto *prerequisite = MakeGraph("prerequisite");
    ASSERT_NE(prerequisite, nullptr);
    ASSERT_EQ(energy->AddSubBehavior(prerequisite), CK_OK);
    ASSERT_TRUE(edits.SetSubmissionEnabled(owner, submission, true));
    ASSERT_TRUE(edits.ProcessFrame());
    ASSERT_EQ(energy->GetSubBehaviorCount(), 2);
    auto invalid = std::make_shared<Program>();
    invalid->RequireOne(NodePattern("missing"));
    const Installations::Rule replacement{{"Gameplay_Energy"}, 4, invalid};
    EXPECT_FALSE(edits.ReplaceSubmission(owner, submission, {},
        {{{"Gameplay_Ingame"}, 3, first}, replacement}));
    ASSERT_TRUE(edits.ReadSubmission(owner, submission, info));
    EXPECT_NE(info.ApplyFailure.Code, Error::None);
    EXPECT_EQ(info.RestoreFailure.Code, Error::None);
    Installations::InstalledEdit after;
    ASSERT_TRUE(edits.FindInstallation(owner, submission, Issue(graph), 1, after));
    EXPECT_EQ(before.Value.Id, after.Value.Id);
    EXPECT_EQ(energy->GetSubBehaviorCount(), 2);
    EXPECT_TRUE(edits.CloseSubmission(owner, submission));
    EXPECT_EQ(energy->GetSubBehaviorCount(), 1);
}
} // namespace
