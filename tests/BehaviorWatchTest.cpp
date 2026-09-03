#include "Behavior/Watch.h"

#include <chrono>
#include <future>
#include <stdexcept>
#include <thread>

#include <gtest/gtest.h>

namespace {

using namespace BML::Behavior;

class FakeGraph final : public GraphSource {
public:
    Status Refer(void *object, NativeRef &out) override {
        out = {1, object};
        return {};
    }

    Status Read(const NativeRef &, GraphView view, GraphModel &out) override {
        out = {};
        out.View = view;
        out.Fingerprint = Structure;
        return {};
    }

    Status ReadLayout(const NativeRef &, Layout &out) override {
        out = {};
        out.Generation = LayoutMark;
        return {};
    }

    Status ReadValue(const NativeRef &, const Slot &, ReadMode,
                     GraphValue &out) override {
        out = Value;
        return ValueStatus;
    }

    Status GraphFingerprint(const NativeRef &, GraphView view,
                            std::uint64_t &out) override {
        ++GraphFingerprintCalls;
        LastView = view;
        out = Structure;
        return GraphStatus;
    }

    Status LayoutFingerprint(const NativeRef &,
                             std::uint64_t &out) override {
        out = LayoutMark;
        return LayoutStatus;
    }

    std::uint64_t Structure = 10;
    std::uint64_t LayoutMark = 20;
    GraphValue Value{ValueState::Available, ValueRelation::Stored,
                     CKGUID(1, 2), Parameter::Form::Int32,
                     std::int32_t{1}};
    GraphView LastView = GraphView::Logical;
    int GraphFingerprintCalls = 0;
    Status GraphStatus;
    Status LayoutStatus;
    Status ValueStatus;
};

struct References {
    static void Retain(void *state) {
        ++static_cast<References *>(state)->Retains;
    }
    static void Release(void *state) {
        ++static_cast<References *>(state)->Releases;
    }
    int Retains = 0;
    int Releases = 0;
};

WatchSpec GraphSpec(GraphView view = GraphView::Logical) {
    WatchSpec spec;
    spec.Kind = WatchKind::GraphChanged;
    spec.View = view;
    spec.Root = {1, reinterpret_cast<void *>(1)};
    return spec;
}

WatchSpec ValueSpec() {
    WatchSpec spec;
    spec.Kind = WatchKind::SampledValueChanged;
    spec.Node = {2, reinterpret_cast<void *>(2)};
    spec.ValueSlot.Kind = SlotKind::InputParameter;
    spec.ValueSlot.Name = "Value";
    spec.ValueSlot.RequireUnique = true;
    return spec;
}

TEST(BehaviorWatch, GraphWatchUsesTheSelectedViewAndOnlyReportsChanges) {
    FakeGraph source;
    std::vector<WatchEvent> events;
    std::shared_ptr<Watch> watch;
    ASSERT_TRUE(Watch::Open(
        source, GraphSpec(GraphView::Live), PlanCallbackState::Static(),
        [&](const WatchEvent &event) { events.push_back(event); }, watch));

    ASSERT_TRUE(watch->Poll(1));
    EXPECT_TRUE(events.empty());
    source.Structure = 11;
    ASSERT_TRUE(watch->Poll(2));
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].Before, 10u);
    EXPECT_EQ(events[0].After, 11u);
    EXPECT_EQ(events[0].Frame, 2u);
    EXPECT_EQ(events[0].Sequence, 1u);
    EXPECT_EQ(source.LastView, GraphView::Live);
}

TEST(BehaviorWatch, SampledValueWatchDoesNotClaimBetweenSampleWrites) {
    FakeGraph source;
    int calls = 0;
    std::shared_ptr<Watch> watch;
    ASSERT_TRUE(Watch::Open(
        source, ValueSpec(), PlanCallbackState::Static(),
        [&](const WatchEvent &) { ++calls; }, watch));

    source.Value.Data = std::int32_t{2};
    source.Value.Data = std::int32_t{1};
    ASSERT_TRUE(watch->Poll(1));
    EXPECT_EQ(calls, 0);

    source.Value.Data = std::int32_t{3};
    ASSERT_TRUE(watch->Poll(2));
    EXPECT_EQ(calls, 1);
    ASSERT_TRUE(watch->Poll(3));
    EXPECT_EQ(calls, 1);
}

TEST(BehaviorWatch, NonForcingValueCanBecomeIndeterminate) {
    FakeGraph source;
    WatchEvent observed;
    std::shared_ptr<Watch> watch;
    ASSERT_TRUE(Watch::Open(
        source, ValueSpec(), PlanCallbackState::Static(),
        [&](const WatchEvent &event) { observed = event; }, watch));

    source.Value.State = ValueState::Indeterminate;
    source.Value.Relation = ValueRelation::Operation;
    source.Value.Data = std::monostate{};
    ASSERT_TRUE(watch->Poll(7));
    EXPECT_EQ(observed.PreviousValue.State, ValueState::Available);
    EXPECT_EQ(observed.CurrentValue.State, ValueState::Indeterminate);
    EXPECT_EQ(observed.CurrentValue.Relation, ValueRelation::Operation);
}

TEST(BehaviorWatch, SelfCloseStopsAdmissionWithoutWaiting) {
    FakeGraph source;
    References references;
    std::shared_ptr<Watch> watch;
    ASSERT_TRUE(Watch::Open(
        source, GraphSpec(),
        PlanCallbackState::Retained(
            &references, &References::Retain, &References::Release),
        [&](const WatchEvent &) { watch->Close(); }, watch));
    EXPECT_EQ(references.Retains, 1);

    source.Structure = 12;
    ASSERT_TRUE(watch->Poll(1));
    EXPECT_FALSE(watch->IsOpen());
    EXPECT_EQ(references.Releases, 0);
    EXPECT_TRUE(watch->RetireAtSafePoint());
    EXPECT_EQ(references.Releases, 1);
    EXPECT_TRUE(watch->RetireAtSafePoint());
    EXPECT_EQ(references.Releases, 1);
}

TEST(BehaviorWatch, CallbackExceptionFailsTheWatchAndKeepsTheFirstDiagnostic) {
    FakeGraph source;
    References references;
    std::shared_ptr<Watch> watch;
    ASSERT_TRUE(Watch::Open(
        source, GraphSpec(),
        PlanCallbackState::Retained(
            &references, &References::Retain, &References::Release),
        [](const WatchEvent &) { throw std::runtime_error("watch failed"); },
        watch));
    EXPECT_EQ(references.Retains, 1);
    source.Structure = 13;
    const Status status = watch->Poll(1);
    EXPECT_EQ(status.Code, Error::CallbackFailed);
    EXPECT_EQ(status.Details.Stage, Phase::LifecycleCallback);
    EXPECT_EQ(status.Message, "watch failed");
    EXPECT_FALSE(watch->IsOpen());

    const WatchInfo failed = watch->Read();
    EXPECT_EQ(failed.State, WatchState::Failed);
    EXPECT_EQ(failed.Diagnostic.Code, Error::CallbackFailed);
    EXPECT_EQ(failed.Diagnostic.Details.Stage, Phase::LifecycleCallback);
    EXPECT_EQ(failed.Diagnostic.Message, "watch failed");

    const int reads = source.GraphFingerprintCalls;
    source.GraphStatus = {Error::InvalidState, CKERR_INVALIDOBJECT,
                          CKBR_BEHAVIORERROR, "graph vanished later"};
    const Status repeated = watch->Poll(2);
    EXPECT_EQ(repeated.Code, Error::CallbackFailed);
    EXPECT_EQ(repeated.Message, "watch failed");
    EXPECT_EQ(source.GraphFingerprintCalls, reads);

    EXPECT_TRUE(watch->RetireAtSafePoint());
    EXPECT_EQ(references.Releases, 1);
    EXPECT_EQ(watch->Read().State, WatchState::Failed);
}

TEST(BehaviorWatch, CloseFromAnotherThreadDoesNotWaitForTheCallback) {
    FakeGraph source;
    std::promise<void> entered;
    std::promise<void> leave;
    std::shared_future<void> mayLeave = leave.get_future().share();
    std::shared_ptr<Watch> watch;
    ASSERT_TRUE(Watch::Open(
        source, GraphSpec(), PlanCallbackState::Static(),
        [&](const WatchEvent &) {
            entered.set_value();
            mayLeave.wait();
        }, watch));
    source.Structure = 14;

    std::thread polling([&] { (void) watch->Poll(1); });
    const auto admission = entered.get_future().wait_for(
        std::chrono::seconds(2));
    if (admission != std::future_status::ready) {
        leave.set_value();
        polling.join();
        FAIL() << "The Watch callback was not admitted.";
    }
    const auto before = std::chrono::steady_clock::now();
    watch->Close();
    const auto elapsed = std::chrono::steady_clock::now() - before;
    EXPECT_LT(elapsed, std::chrono::milliseconds(100));
    EXPECT_FALSE(watch->IsOpen());

    leave.set_value();
    polling.join();
    EXPECT_TRUE(watch->RetireAtSafePoint());
}

} // namespace
