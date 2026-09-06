#include "Behavior/Sessions.h"
#include "Behavior/FrameStore.h"

#include <cstdlib>
#include <new>
#include <stdexcept>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

namespace {
thread_local bool g_CountAllocations = false;
thread_local std::size_t g_AllocationCount = 0;

template <class Function>
std::size_t CountAllocations(Function &&function) {
    g_AllocationCount = 0;
    g_CountAllocations = true;
    function();
    g_CountAllocations = false;
    return g_AllocationCount;
}
}

void *operator new(std::size_t size) {
    if (g_CountAllocations)
        ++g_AllocationCount;
    if (void *memory = std::malloc(size ? size : 1))
        return memory;
    throw std::bad_alloc();
}

void *operator new[](std::size_t size) {
    return ::operator new(size);
}

void operator delete(void *memory) noexcept {
    std::free(memory);
}

void operator delete[](void *memory) noexcept {
    std::free(memory);
}

void operator delete(void *memory, std::size_t) noexcept {
    std::free(memory);
}

void operator delete[](void *memory, std::size_t) noexcept {
    std::free(memory);
}

namespace BML::Behavior::Internal {
void AdvanceBehaviorSessionRuntime();
std::size_t LiveBehaviorSessionInstances();
void ResetBehaviorSessionRuntimeStateReads();
std::size_t BehaviorSessionRuntimeStateReads();
std::size_t BehaviorSessionRuntimeWorldResets();
void ResetBehaviorSessionRuntimeClosePendingCalls();
std::size_t BehaviorSessionRuntimeClosePendingCalls();
} // namespace BML::Behavior::Internal

namespace {

using namespace BML::Behavior::Internal;

class FakeGraphSource final : public GraphSource {
public:
    Status Refer(void *object, NativeRef &out) override {
        if (!object)
            return {Error::InvalidState, CKERR_INVALIDOBJECT,
                    CKBR_PARAMETERERROR, "The fake graph root is stale."};
        out = {41, object};
        return {};
    }

    Status Read(const NativeRef &root, GraphView view,
                GraphModel &out) override {
        if (!root)
            return {Error::InvalidState, CKERR_INVALIDOBJECT,
                    CKBR_PARAMETERERROR, "The fake graph root is stale."};
        out = {};
        out.View = view;
        out.Generation = 7;
        GraphNode node;
        node.Id = root.Id;
        node.Name = "Owned Behavior";
        out.Nodes.push_back(std::move(node));
        return {};
    }

    Status ReadLayout(const NativeRef &, Layout &out) override {
        out = {};
        out.Generation = LayoutFingerprintValue;
        return {};
    }

    Status ReadValue(const NativeRef &, std::uint64_t, const Slot &, ReadMode,
                     GraphValue &) override {
        return {Error::Unavailable, CKERR_NOTIMPLEMENTED,
                CKBR_PARAMETERERROR, "Not used by this test."};
    }

    Status GraphFingerprint(const NativeRef &, GraphView,
                            std::uint64_t &out) override {
        ++GraphFingerprintCalls;
        out = GraphFingerprintValue;
        return GraphFingerprintStatus;
    }

    Status LayoutFingerprint(const NativeRef &,
                             std::uint64_t &out) override {
        ++LayoutFingerprintCalls;
        out = LayoutFingerprintValue;
        return LayoutFingerprintStatus;
    }

    std::uint64_t GraphFingerprintValue = 7;
    int GraphFingerprintCalls = 0;
    Status GraphFingerprintStatus;
    std::uint64_t LayoutFingerprintValue = 11;
    int LayoutFingerprintCalls = 0;
    Status LayoutFingerprintStatus;
};

struct WatchReferences {
    static void Retain(void *state) {
        ++static_cast<WatchReferences *>(state)->Retains;
    }
    static void Release(void *state) {
        ++static_cast<WatchReferences *>(state)->Releases;
    }

    int Retains = 0;
    int Releases = 0;
};

struct ReentrantWatchReferences {
    static void Retain(void *state) {
        auto &self = *static_cast<ReentrantWatchReferences *>(state);
        ++self.Retains;
        if (self.CloseSessionOnRetain)
            self.Owner->CloseSession(self.Session);
    }

    static void Release(void *state) {
        auto &self = *static_cast<ReentrantWatchReferences *>(state);
        ++self.Releases;
        if (self.WatchToClose)
            self.Owner->CloseWatch(self.WatchToClose);
    }

    Sessions *Owner = nullptr;
    std::uintptr_t Session = 0;
    std::uintptr_t WatchToClose = 0;
    bool CloseSessionOnRetain = false;
    int Retains = 0;
    int Releases = 0;
};

struct ReentrantOwnerRegistration {
    static void Retain(void *state) {
        ++static_cast<ReentrantOwnerRegistration *>(state)->Retains;
    }

    static void Release(void *state) {
        auto &self = *static_cast<ReentrantOwnerRegistration *>(state);
        ++self.Releases;
        self.Generation = self.Owner->RegisterOwner(self.OwnerId);
        self.Opened = self.Owner->OpenSession(self.OwnerId, self.Session);
        if (self.Opened) {
            self.Run = self.Owner->Spawn(
                self.Session, nullptr, BlockSpec(CKGUID(21, 22)));
        }
    }

    Sessions *Owner = nullptr;
    std::string OwnerId;
    std::uint64_t Generation = 0;
    std::uintptr_t Session = 0;
    Status Opened;
    OpenRun Run;
    int Retains = 0;
    int Releases = 0;
};

Slot Input(const char *name) {
    Slot input;
    input.Kind = SlotKind::Input;
    input.Name = name;
    input.RequireUnique = true;
    return input;
}

WatchSpec GraphWatchSpec(GraphView view = GraphView::Logical) {
    WatchSpec spec;
    spec.Kind = WatchKind::GraphChanged;
    spec.View = view;
    return spec;
}

WatchSpec LayoutWatchSpec() {
    WatchSpec spec;
    spec.Kind = WatchKind::LayoutChanged;
    return spec;
}

TEST(BehaviorSessions, OwnerGenerationMakesOldSessionsStale) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t first = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", first));
    ASSERT_NE(first, 0u);

    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    RunInfo info;
    EXPECT_EQ(sessions.ReadRun(first, info).Code, Error::InvalidState);
    std::uintptr_t second = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", second));
    EXPECT_GT(second, first);
}

TEST(BehaviorSessions, RegisterOwnerKeepsAReentrantOwnerGeneration) {
    Runtime runtime(nullptr);
    auto source = std::make_unique<FakeGraphSource>();
    Sessions sessions(runtime, nullptr, std::move(source));
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    ReentrantOwnerRegistration registration;
    registration.Owner = &sessions;
    registration.OwnerId = "mod";
    std::uintptr_t watch = 0;
    ASSERT_TRUE(sessions.OpenWatch(
        session, reinterpret_cast<void *>(1), nullptr, GraphWatchSpec(),
        PlanCallbackState::Retained(
            &registration, &ReentrantOwnerRegistration::Retain,
            &ReentrantOwnerRegistration::Release),
        [](const WatchEvent &) {}, watch));

    const std::uint64_t generation = sessions.RegisterOwner("mod");
    EXPECT_EQ(registration.Releases, 1);
    ASSERT_NE(registration.Generation, 0u);
    EXPECT_EQ(generation, registration.Generation);
    ASSERT_TRUE(registration.Opened);
    ASSERT_NE(registration.Session, 0u);
    SessionOwner owner;
    ASSERT_TRUE(sessions.ReadOwner(registration.Session, owner));
    EXPECT_EQ(owner.Generation, registration.Generation);
    ASSERT_TRUE(registration.Run);
    EXPECT_EQ(LiveBehaviorSessionInstances(), 1u);
    sessions.RetireOwner("mod");
    EXPECT_EQ(LiveBehaviorSessionInstances(), 0u);
}

TEST(BehaviorSessions, SessionExposesItsCurrentOwnerGeneration) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    const std::uint64_t generation = sessions.RegisterOwner("mod");
    ASSERT_NE(generation, 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    SessionOwner owner;
    ASSERT_TRUE(sessions.ReadOwner(session, owner));
    EXPECT_EQ(owner.Id, "mod");
    EXPECT_EQ(owner.Generation, generation);

    sessions.RetireOwner("mod");
    EXPECT_EQ(sessions.ReadOwner(session, owner).Code, Error::InvalidState);
    EXPECT_FALSE(owner);
}

TEST(BehaviorSessions, RetiringOwnerClosesSessionsIdempotently) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    sessions.RetireOwner("mod");
    sessions.RetireOwner("mod");
    sessions.CloseSession(session);

    std::uintptr_t rejected = 0;
    EXPECT_EQ(sessions.OpenSession("mod", rejected).Code,
              Error::InvalidState);
    EXPECT_EQ(rejected, 0u);
}

TEST(BehaviorSessions, CloseSessionMayRunOffTheGameThread) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    std::thread close([&] { sessions.CloseSession(session); });
    close.join();
    sessions.CloseSession(session);
}

TEST(BehaviorSessions, SessionOwnerMayBeReadOffTheGameThread) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    const std::uint64_t generation = sessions.RegisterOwner("mod");
    ASSERT_NE(generation, 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    SessionOwner owner;
    Status status;
    std::thread read([&] { status = sessions.ReadOwner(session, owner); });
    read.join();

    ASSERT_TRUE(status);
    EXPECT_EQ(owner.Id, "mod");
    EXPECT_EQ(owner.Generation, generation);
}

TEST(BehaviorSessions, OtherOperationsRequireTheGameThread) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    Status status;
    std::thread open([&] {
        std::uintptr_t session = 0;
        status = sessions.OpenSession("mod", session);
    });
    open.join();
    EXPECT_EQ(status.Code, Error::WrongThread);
}

TEST(BehaviorSessions, ReadyCallKeepsItsInstanceUntilTheRunCloses) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    BlockSpec block(CKGUID(1, 2));
    OpenRun run = sessions.Call(
        session, nullptr, block, Input("Run"));
    ASSERT_TRUE(run);
    EXPECT_EQ(run.Info.Kind, RunKind::Call);
    EXPECT_EQ(run.Info.State, RunState::Ready);
    EXPECT_EQ(LiveBehaviorSessionInstances(), 1u);

    std::shared_ptr<FrameStore> frames = sessions.Frames(run.Id);
    ASSERT_NE(frames, nullptr);
    ASSERT_EQ(frames->Read().size(), 1u);
    EXPECT_FALSE(frames->Read().front().NativeContinuation);
    EXPECT_FALSE(frames->Read().front().QueuedInput);
    EXPECT_EQ(frames->Read().front().ActiveOutputs.front().Name, "Done");

    sessions.CloseRun(run.Id);
    RunInfo stale;
    EXPECT_EQ(sessions.ReadRun(run.Id, stale).Code, Error::InvalidState);
    EXPECT_EQ(LiveBehaviorSessionInstances(), 1u);
    sessions.ProcessFrame();
    EXPECT_EQ(LiveBehaviorSessionInstances(), 0u);
}

TEST(BehaviorSessions, ReadsTheGraphOwnedByTheRun) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime, nullptr, std::make_unique<FakeGraphSource>());
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    OpenRun run = sessions.Spawn(session, nullptr, BlockSpec(CKGUID(1, 2)));
    ASSERT_TRUE(run);

    GraphModel graph;
    ASSERT_TRUE(sessions.ReadGraph(run.Id, GraphView::Live, graph));
    EXPECT_EQ(graph.View, GraphView::Live);
    EXPECT_EQ(graph.Generation, 7u);
    ASSERT_EQ(graph.Nodes.size(), 1u);
    EXPECT_EQ(graph.Nodes.front().Id, 41u);
    EXPECT_EQ(graph.Nodes.front().Name, "Owned Behavior");

    sessions.CloseRun(run.Id);
    EXPECT_EQ(sessions.ReadGraph(run.Id, GraphView::Logical, graph).Code,
              Error::InvalidState);
}

TEST(BehaviorSessions, IdleFramesDoNotPollEveryRun) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    std::vector<std::uintptr_t> runs;
    for (int index = 0; index < 32; ++index) {
        OpenRun run = sessions.Spawn(
            session, nullptr, BlockSpec(CKGUID(1, 2)));
        ASSERT_TRUE(run);
        runs.push_back(run.Id);
    }

    ResetBehaviorSessionRuntimeStateReads();
    ResetBehaviorSessionRuntimeClosePendingCalls();
    sessions.ProcessFrame();
    EXPECT_EQ(BehaviorSessionRuntimeStateReads(), 0u);
    EXPECT_EQ(BehaviorSessionRuntimeClosePendingCalls(), 0u);

    RunInfo info;
    ASSERT_TRUE(sessions.ReadRun(runs.front(), info));
    EXPECT_EQ(BehaviorSessionRuntimeStateReads(), 1u);
}

TEST(BehaviorSessions, GraphWatchesShareOneReadingPerGraphAndView) {
    Runtime runtime(nullptr);
    auto source = std::make_unique<FakeGraphSource>();
    FakeGraphSource *graph = source.get();
    Sessions sessions(runtime, nullptr, std::move(source));
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    for (int index = 0; index < 32; ++index) {
        std::uintptr_t watch = 0;
        ASSERT_TRUE(sessions.OpenWatch(
            session, reinterpret_cast<void *>(1), nullptr, GraphWatchSpec(),
            PlanCallbackState::Static(), [](const WatchEvent &) {}, watch));
    }
    const int baselineReads = graph->GraphFingerprintCalls;

    sessions.ProcessFrame();

    EXPECT_EQ(graph->GraphFingerprintCalls, baselineReads + 1);
    sessions.ProcessFrame();
    EXPECT_EQ(graph->GraphFingerprintCalls, baselineReads + 2);
}

TEST(BehaviorSessions, GraphWatchReadingsKeepLogicalAndLiveSeparate) {
    Runtime runtime(nullptr);
    auto source = std::make_unique<FakeGraphSource>();
    FakeGraphSource *graph = source.get();
    Sessions sessions(runtime, nullptr, std::move(source));
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    for (GraphView view : {GraphView::Logical, GraphView::Live}) {
        std::uintptr_t watch = 0;
        ASSERT_TRUE(sessions.OpenWatch(
            session, reinterpret_cast<void *>(1), nullptr,
            GraphWatchSpec(view), PlanCallbackState::Static(),
            [](const WatchEvent &) {}, watch));
    }
    const int baselineReads = graph->GraphFingerprintCalls;

    sessions.ProcessFrame();

    EXPECT_EQ(graph->GraphFingerprintCalls, baselineReads + 2);
}

TEST(BehaviorSessions, DistinctWatchTargetsDoNotAllocateAfterWarmup) {
    constexpr int kWatchCount = 128;

    Runtime runtime(nullptr);
    auto source = std::make_unique<FakeGraphSource>();
    FakeGraphSource *graph = source.get();
    Sessions sessions(runtime, nullptr, std::move(source));
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    for (int index = 0; index < kWatchCount; ++index) {
        std::uintptr_t watch = 0;
        void *root = reinterpret_cast<void *>(
            static_cast<std::uintptr_t>(index + 1));
        ASSERT_TRUE(sessions.OpenWatch(
            session, root, nullptr, GraphWatchSpec(),
            PlanCallbackState::Static(), [](const WatchEvent &) {}, watch));
    }
    for (int index = 0; index < kWatchCount; ++index) {
        std::uintptr_t watch = 0;
        void *node = reinterpret_cast<void *>(
            static_cast<std::uintptr_t>(kWatchCount + index + 1));
        ASSERT_TRUE(sessions.OpenWatch(
            session, nullptr, node, LayoutWatchSpec(),
            PlanCallbackState::Static(), [](const WatchEvent &) {}, watch));
    }

    sessions.ProcessFrame();
    const int warmedGraphReads = graph->GraphFingerprintCalls;
    const int warmedLayoutReads = graph->LayoutFingerprintCalls;
    const std::size_t allocations =
        CountAllocations([&] { sessions.ProcessFrame(); });

    EXPECT_EQ(graph->GraphFingerprintCalls, warmedGraphReads + kWatchCount);
    EXPECT_EQ(graph->LayoutFingerprintCalls, warmedLayoutReads + kWatchCount);
#if !defined(_ITERATOR_DEBUG_LEVEL) || _ITERATOR_DEBUG_LEVEL == 0
    EXPECT_EQ(allocations, 0u);
#endif
}

TEST(BehaviorSessions, StableGraphWatchesDoNotAllocateWhilePolling) {
    constexpr int kWatchCount = 32;
    constexpr std::uint64_t kSteadyFrames = 1024;

    FakeGraphSource directSource;
    std::shared_ptr<Watch> directWatch;
    ASSERT_TRUE(Watch::Open(
        directSource, [] {
            WatchSpec spec = GraphWatchSpec();
            spec.Root = {41, reinterpret_cast<void *>(1)};
            return spec;
        }(), PlanCallbackState::Static(), [](const WatchEvent &) {},
        directWatch));
    WatchReadings directReadings;
    ASSERT_TRUE(directWatch->Poll(1, directReadings));

    Runtime runtime(nullptr);
    auto source = std::make_unique<FakeGraphSource>();
    Sessions sessions(runtime, nullptr, std::move(source));
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    for (int index = 0; index < kWatchCount; ++index) {
        std::uintptr_t watch = 0;
        ASSERT_TRUE(sessions.OpenWatch(
            session, reinterpret_cast<void *>(1), nullptr, GraphWatchSpec(),
            PlanCallbackState::Static(), [](const WatchEvent &) {}, watch));
    }
    sessions.ProcessFrame();

    const std::size_t directAllocations = CountAllocations([&] {
        for (std::uint64_t frame = 2; frame <= kSteadyFrames + 1; ++frame) {
            directReadings.BeginFrame(frame);
            for (int index = 0; index < kWatchCount; ++index)
                (void) directWatch->Poll(frame, directReadings);
        }
    });

    Runtime emptyRuntime(nullptr);
    auto emptySource = std::make_unique<FakeGraphSource>();
    Sessions emptySessions(emptyRuntime, nullptr, std::move(emptySource));
    const std::size_t emptyFrameAllocations = CountAllocations([&] {
        for (std::uint64_t frame = 0; frame < kSteadyFrames; ++frame)
            emptySessions.ProcessFrame();
    });
    const std::size_t sessionAllocations = CountAllocations([&] {
        for (std::uint64_t frame = 0; frame < kSteadyFrames; ++frame)
            sessions.ProcessFrame();
    });

    EXPECT_EQ(sessionAllocations,
              directAllocations + emptyFrameAllocations);
#if !defined(_ITERATOR_DEBUG_LEVEL) || _ITERATOR_DEBUG_LEVEL == 0
    EXPECT_EQ(sessionAllocations, 0u);
#endif
}

TEST(BehaviorSessions, LayoutWatchesShareOneReadingPerNode) {
    Runtime runtime(nullptr);
    auto source = std::make_unique<FakeGraphSource>();
    FakeGraphSource *graph = source.get();
    Sessions sessions(runtime, nullptr, std::move(source));
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    for (int index = 0; index < 32; ++index) {
        std::uintptr_t watch = 0;
        ASSERT_TRUE(sessions.OpenWatch(
            session, nullptr, reinterpret_cast<void *>(2), LayoutWatchSpec(),
            PlanCallbackState::Static(), [](const WatchEvent &) {}, watch));
    }
    const int baselineReads = graph->LayoutFingerprintCalls;

    sessions.ProcessFrame();

    EXPECT_EQ(graph->LayoutFingerprintCalls, baselineReads + 1);
    sessions.ProcessFrame();
    EXPECT_EQ(graph->LayoutFingerprintCalls, baselineReads + 2);
}

TEST(BehaviorSessions, WatchCallbackClosesRunAtTheCurrentSafePoint) {
    Runtime runtime(nullptr);
    auto source = std::make_unique<FakeGraphSource>();
    FakeGraphSource *graph = source.get();
    Sessions sessions(runtime, nullptr, std::move(source));
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));
    const OpenRun run = sessions.Spawn(
        session, nullptr, BlockSpec(CKGUID(1, 2)));
    ASSERT_TRUE(run);
    ASSERT_EQ(LiveBehaviorSessionInstances(), 1u);

    std::uintptr_t watch = 0;
    ASSERT_TRUE(sessions.OpenWatch(
        session, reinterpret_cast<void *>(1), nullptr, GraphWatchSpec(),
        PlanCallbackState::Static(),
        [&](const WatchEvent &) { sessions.CloseRun(run.Id); }, watch));

    ResetBehaviorSessionRuntimeClosePendingCalls();
    graph->GraphFingerprintValue = 8;
    sessions.ProcessFrame();

    EXPECT_EQ(LiveBehaviorSessionInstances(), 0u);
    EXPECT_EQ(BehaviorSessionRuntimeClosePendingCalls(), 1u);
    RunInfo stale;
    EXPECT_EQ(sessions.ReadRun(run.Id, stale).Code, Error::InvalidState);
}

TEST(BehaviorSessions, QueuedRunCloseCompletesBeforeWatchCallbacks) {
    Runtime runtime(nullptr);
    auto source = std::make_unique<FakeGraphSource>();
    FakeGraphSource *graph = source.get();
    Sessions sessions(runtime, nullptr, std::move(source));
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));
    const OpenRun run = sessions.Spawn(
        session, nullptr, BlockSpec(CKGUID(1, 2)));
    ASSERT_TRUE(run);

    std::size_t instancesSeenByCallback = 1;
    std::uintptr_t watch = 0;
    ASSERT_TRUE(sessions.OpenWatch(
        session, reinterpret_cast<void *>(1), nullptr, GraphWatchSpec(),
        PlanCallbackState::Static(),
        [&](const WatchEvent &) {
            instancesSeenByCallback = LiveBehaviorSessionInstances();
        }, watch));

    sessions.CloseRun(run.Id);
    graph->GraphFingerprintValue = 8;
    sessions.ProcessFrame();

    EXPECT_EQ(instancesSeenByCallback, 0u);
    EXPECT_EQ(LiveBehaviorSessionInstances(), 0u);
}

TEST(BehaviorSessions, WatchCallbackCannotReenterFrameProcessing) {
    Runtime runtime(nullptr);
    auto source = std::make_unique<FakeGraphSource>();
    FakeGraphSource *graph = source.get();
    Sessions sessions(runtime, nullptr, std::move(source));
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    int callbacks = 0;
    std::uintptr_t watch = 0;
    ASSERT_TRUE(sessions.OpenWatch(
        session, reinterpret_cast<void *>(1), nullptr, GraphWatchSpec(),
        PlanCallbackState::Static(),
        [&](const WatchEvent &) {
            ++callbacks;
            sessions.ProcessFrame();
        }, watch));
    const int baselineReads = graph->GraphFingerprintCalls;

    graph->GraphFingerprintValue = 8;
    sessions.ProcessFrame();

    EXPECT_EQ(callbacks, 1);
    EXPECT_EQ(graph->GraphFingerprintCalls, baselineReads + 1);
    sessions.ProcessFrame();
    EXPECT_EQ(graph->GraphFingerprintCalls, baselineReads + 2);
}

TEST(BehaviorSessions, WatchCallbackCanCloseTheRemainingFrameWatches) {
    Runtime runtime(nullptr);
    auto source = std::make_unique<FakeGraphSource>();
    FakeGraphSource *graph = source.get();
    Sessions sessions(runtime, nullptr, std::move(source));
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    std::vector<std::uintptr_t> watches(3);
    int callbacks = 0;
    for (std::uintptr_t &watch : watches) {
        ASSERT_TRUE(sessions.OpenWatch(
            session, reinterpret_cast<void *>(1), nullptr, GraphWatchSpec(),
            PlanCallbackState::Static(),
            [&](const WatchEvent &) {
                ++callbacks;
                for (std::uintptr_t current : watches)
                    sessions.CloseWatch(current);
            }, watch));
    }

    graph->GraphFingerprintValue = 8;
    sessions.ProcessFrame();

    EXPECT_EQ(callbacks, 1);
    WatchInfo info;
    for (std::uintptr_t watch : watches)
        EXPECT_EQ(sessions.ReadWatch(watch, info).Code, Error::InvalidState);
}

TEST(BehaviorSessions, FailedWatchRemainsReadableAndIsNotPolledAgain) {
    Runtime runtime(nullptr);
    auto source = std::make_unique<FakeGraphSource>();
    FakeGraphSource *graph = source.get();
    Sessions sessions(runtime, nullptr, std::move(source));
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    WatchReferences references;
    std::uintptr_t watch = 0;
    ASSERT_TRUE(sessions.OpenWatch(
        session, reinterpret_cast<void *>(1), nullptr, GraphWatchSpec(),
        PlanCallbackState::Retained(
            &references, &WatchReferences::Retain, &WatchReferences::Release),
        [](const WatchEvent &) { throw std::runtime_error("callback failed"); },
        watch));
    EXPECT_EQ(references.Retains, 1);
    WatchInfo info;
    ASSERT_TRUE(sessions.ReadWatch(watch, info));
    EXPECT_EQ(info.State, WatchState::Active);
    EXPECT_TRUE(info.Diagnostic);

    graph->GraphFingerprintValue = 8;
    sessions.ProcessFrame();
    ASSERT_TRUE(sessions.ReadWatch(watch, info));
    EXPECT_EQ(info.State, WatchState::Failed);
    EXPECT_EQ(info.Diagnostic.Code, Error::CallbackFailed);
    EXPECT_EQ(info.Diagnostic.Message, "callback failed");
    EXPECT_EQ(references.Releases, 1);

    const int reads = graph->GraphFingerprintCalls;
    graph->GraphFingerprintValue = 9;
    sessions.ProcessFrame();
    ASSERT_TRUE(sessions.ReadWatch(watch, info));
    EXPECT_EQ(info.Diagnostic.Code, Error::CallbackFailed);
    EXPECT_EQ(info.Diagnostic.Message, "callback failed");
    EXPECT_EQ(graph->GraphFingerprintCalls, reads);
    EXPECT_EQ(references.Releases, 1);

    sessions.CloseWatch(watch);
    EXPECT_EQ(sessions.ReadWatch(watch, info).Code, Error::InvalidState);
}

TEST(BehaviorSessions, WatchRetainMayCloseItsSessionWithoutInvalidatingOpen) {
    Runtime runtime(nullptr);
    auto source = std::make_unique<FakeGraphSource>();
    Sessions sessions(runtime, nullptr, std::move(source));
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    ReentrantWatchReferences references;
    references.Owner = &sessions;
    references.Session = session;
    references.CloseSessionOnRetain = true;
    std::uintptr_t watch = 0;
    Status opened = sessions.OpenWatch(
        session, reinterpret_cast<void *>(1), nullptr, GraphWatchSpec(),
        PlanCallbackState::Retained(
            &references, &ReentrantWatchReferences::Retain,
            &ReentrantWatchReferences::Release),
        [](const WatchEvent &) {}, watch);

    EXPECT_EQ(opened.Code, Error::InvalidState);
    EXPECT_EQ(watch, 0u);
    EXPECT_EQ(references.Retains, 1);
    sessions.ProcessFrame();
    EXPECT_EQ(references.Releases, 1);
}

TEST(BehaviorSessions, WatchReleaseMayCloseAnotherWatchDuringCollection) {
    Runtime runtime(nullptr);
    auto source = std::make_unique<FakeGraphSource>();
    Sessions sessions(runtime, nullptr, std::move(source));
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    ReentrantWatchReferences firstReferences;
    firstReferences.Owner = &sessions;
    WatchReferences secondReferences;
    std::uintptr_t first = 0;
    std::uintptr_t second = 0;
    ASSERT_TRUE(sessions.OpenWatch(
        session, reinterpret_cast<void *>(1), nullptr, GraphWatchSpec(),
        PlanCallbackState::Retained(
            &firstReferences, &ReentrantWatchReferences::Retain,
            &ReentrantWatchReferences::Release),
        [](const WatchEvent &) {}, first));
    ASSERT_TRUE(sessions.OpenWatch(
        session, reinterpret_cast<void *>(1), nullptr, GraphWatchSpec(),
        PlanCallbackState::Retained(
            &secondReferences, &WatchReferences::Retain,
            &WatchReferences::Release),
        [](const WatchEvent &) {}, second));
    firstReferences.WatchToClose = second;

    sessions.CloseWatch(first);
    sessions.ProcessFrame();

    WatchInfo info;
    EXPECT_EQ(sessions.ReadWatch(first, info).Code, Error::InvalidState);
    EXPECT_EQ(sessions.ReadWatch(second, info).Code, Error::InvalidState);
    EXPECT_EQ(firstReferences.Releases, 1);
    EXPECT_EQ(secondReferences.Releases, 1);
}

TEST(BehaviorSessions, VanishedGraphFailsWatchUntilWorldReset) {
    Runtime runtime(nullptr);
    auto source = std::make_unique<FakeGraphSource>();
    FakeGraphSource *graph = source.get();
    Sessions sessions(runtime, nullptr, std::move(source));
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    int callbacks = 0;
    std::uintptr_t watch = 0;
    ASSERT_TRUE(sessions.OpenWatch(
        session, reinterpret_cast<void *>(1), nullptr, GraphWatchSpec(),
        PlanCallbackState::Static(),
        [&](const WatchEvent &) { ++callbacks; }, watch));
    graph->GraphFingerprintStatus = {
        Error::InvalidState, CKERR_INVALIDOBJECT, CKBR_BEHAVIORERROR,
        "The watched graph vanished."};
    sessions.ProcessFrame();

    WatchInfo info;
    ASSERT_TRUE(sessions.ReadWatch(watch, info));
    EXPECT_EQ(info.State, WatchState::Failed);
    EXPECT_EQ(info.Diagnostic.Code, Error::InvalidState);
    EXPECT_EQ(info.Diagnostic.Message, "The watched graph vanished.");
    EXPECT_EQ(callbacks, 0);

    const int reads = graph->GraphFingerprintCalls;
    graph->GraphFingerprintStatus = {};
    graph->GraphFingerprintValue = 8;
    sessions.ProcessFrame();
    EXPECT_EQ(graph->GraphFingerprintCalls, reads);
    EXPECT_EQ(callbacks, 0);

    sessions.ResetWorld();
    EXPECT_EQ(sessions.ReadWatch(watch, info).Code, Error::InvalidState);
}

TEST(BehaviorSessions, ClosingWatchTwiceRetiresItsCallbackOnceAtSafePoint) {
    Runtime runtime(nullptr);
    auto source = std::make_unique<FakeGraphSource>();
    FakeGraphSource *graph = source.get();
    Sessions sessions(runtime, nullptr, std::move(source));
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    WatchReferences references;
    int callbacks = 0;
    std::uintptr_t watch = 0;
    ASSERT_TRUE(sessions.OpenWatch(
        session, reinterpret_cast<void *>(1), nullptr, GraphWatchSpec(),
        PlanCallbackState::Retained(
            &references, &WatchReferences::Retain, &WatchReferences::Release),
        [&](const WatchEvent &) { ++callbacks; }, watch));
    EXPECT_EQ(references.Retains, 1);

    sessions.CloseWatch(watch);
    sessions.CloseWatch(watch);
    WatchInfo info;
    EXPECT_EQ(sessions.ReadWatch(watch, info).Code, Error::InvalidState);
    EXPECT_EQ(references.Releases, 0);

    graph->GraphFingerprintValue = 8;
    sessions.ProcessFrame();
    EXPECT_EQ(callbacks, 0);
    EXPECT_EQ(references.Releases, 1);
    sessions.ProcessFrame();
    EXPECT_EQ(references.Releases, 1);
}

TEST(BehaviorSessions, LiveEditsHonorLayoutGeneration) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));
    OpenRun run = sessions.Spawn(session, nullptr, BlockSpec(CKGUID(1, 2)));
    ASSERT_TRUE(run);

    Slot pin = Slot::Named(SlotKind::InputParameter, "Value", CKPGUID_INT);
    Parameter::Binding value;
    std::uint64_t generation = 0;
    ASSERT_TRUE(sessions.Set(run.Id, 1, pin, value, generation));
    EXPECT_EQ(generation, 1u);

    Status stale = sessions.Set(run.Id, 2, pin, value, generation);
    EXPECT_EQ(stale.Code, Error::StaleLayout);
    EXPECT_EQ(generation, 0u);

    BlockSpec settings;
    ASSERT_TRUE(sessions.Configure(run.Id, settings, generation));
    EXPECT_EQ(generation, 2u);
    EXPECT_EQ(sessions.Set(run.Id, 1, pin, value, generation).Code,
              Error::StaleLayout);
    ASSERT_TRUE(sessions.Set(run.Id, 2, pin, value, generation));

    auto *source = reinterpret_cast<CKBehavior *>(
        static_cast<std::uintptr_t>(77));
    ASSERT_TRUE(sessions.Bind(
        run.Id, 2, pin, source,
        0,
        Slot::Named(SlotKind::OutputParameter, "Value"),
        Parameter::BindingKind::Direct, generation));
    EXPECT_EQ(generation, 2u);
}

TEST(BehaviorSessions, FailedLiveSettingsMakeTheRunTerminal) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));
    OpenRun run = sessions.Spawn(session, nullptr, BlockSpec(CKGUID(1, 2)));
    ASSERT_TRUE(run);

    BlockSpec settings(CKGUID(91, 92));
    std::uint64_t generation = 0;
    const Status failed = sessions.Configure(run.Id, settings, generation);
    ASSERT_EQ(failed.Code, Error::CallbackFailed);
    EXPECT_EQ(generation, 0u);

    RunInfo info;
    ASSERT_TRUE(sessions.ReadRun(run.Id, info));
    EXPECT_EQ(info.State, RunState::Failed);
    EXPECT_EQ(info.LastStatus.Code, Error::CallbackFailed);
    EXPECT_EQ(info.LastStatus.CkError, CKERR_INVALIDPARAMETER);

    const Status repeated = sessions.Configure(run.Id, settings, generation);
    EXPECT_EQ(repeated.Code, Error::CallbackFailed);
    EXPECT_EQ(repeated.CkError, CKERR_INVALIDPARAMETER);
    EXPECT_EQ(generation, 0u);

    const RunResult rejected = sessions.Pulse(run.Id, Input("Run"));
    EXPECT_FALSE(rejected);
    EXPECT_EQ(rejected.Detail.Code, Error::CallbackFailed);
    EXPECT_EQ(LiveBehaviorSessionInstances(), 1u);

    sessions.CloseRun(run.Id);
    EXPECT_EQ(LiveBehaviorSessionInstances(), 1u);
    sessions.ProcessFrame();
    EXPECT_EQ(LiveBehaviorSessionInstances(), 0u);
}

TEST(BehaviorSessions, ContinuePromotesTheSameCallAndRetainsBothFrames) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    BlockSpec block(CKGUID(3, 4));
    OpenRun run = sessions.Call(
        session, nullptr, block, Input("Pending"));
    ASSERT_TRUE(run);
    ASSERT_EQ(run.Info.Kind, RunKind::Call);
    ASSERT_EQ(run.Info.State, RunState::Pending);
    ASSERT_EQ(LiveBehaviorSessionInstances(), 1u);

    RunResult continued = sessions.Continue(run.Id);
    ASSERT_TRUE(continued.Detail);
    EXPECT_EQ(continued.State, RunState::Pending);
    RunInfo info;
    ASSERT_TRUE(sessions.ReadRun(run.Id, info));
    EXPECT_EQ(info.Kind, RunKind::Task);
    EXPECT_EQ(info.State, RunState::Pending);

    AdvanceBehaviorSessionRuntime();
    sessions.ProcessFrame();
    ASSERT_TRUE(sessions.ReadRun(run.Id, info));
    EXPECT_EQ(info.Kind, RunKind::Task);
    EXPECT_EQ(info.State, RunState::Ready);
    EXPECT_EQ(LiveBehaviorSessionInstances(), 1u);

    std::shared_ptr<FrameStore> frames = sessions.Frames(run.Id);
    ASSERT_NE(frames, nullptr);
    const std::vector<RunFrame> captured = frames->Read();
    ASSERT_EQ(captured.size(), 2u);
    EXPECT_TRUE(captured[0].NativeContinuation);
    EXPECT_FALSE(captured[1].NativeContinuation);
    EXPECT_FALSE(captured[1].QueuedInput);

    RunResult pulsed = sessions.Pulse(run.Id, Input("Run"));
    ASSERT_TRUE(pulsed);
    EXPECT_EQ(pulsed.State, RunState::Ready);
    EXPECT_EQ(LiveBehaviorSessionInstances(), 1u);
    sessions.CloseRun(run.Id);
    sessions.ProcessFrame();
    EXPECT_EQ(LiveBehaviorSessionInstances(), 0u);
}

TEST(BehaviorSessions, FailedActivationStillUsesRunOwnedTeardown) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    OpenRun run = sessions.Call(
        session, nullptr, BlockSpec(CKGUID(11, 12)), Input("Fail"));
    ASSERT_TRUE(run);
    EXPECT_EQ(run.Info.State, RunState::Failed);
    EXPECT_EQ(run.Info.LastStatus.Code, Error::ExecutionFailed);
    EXPECT_EQ(LiveBehaviorSessionInstances(), 1u);

    const std::shared_ptr<FrameStore> frames = sessions.Frames(run.Id);
    ASSERT_NE(frames, nullptr);
    ASSERT_EQ(frames->Read().size(), 1u);
    EXPECT_EQ(frames->Read().front().Fault.Code,
              ExecutionError::NativeFailed);

    sessions.ProcessFrame();
    EXPECT_EQ(LiveBehaviorSessionInstances(), 1u);
    sessions.CloseRun(run.Id);
    sessions.ProcessFrame();
    EXPECT_EQ(LiveBehaviorSessionInstances(), 0u);
}

TEST(BehaviorSessions, StartIsManagedFromItsFirstExecution) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    OpenRun run = sessions.Start(
        session, nullptr, BlockSpec(CKGUID(5, 6)),
        Input("Pending"));
    ASSERT_TRUE(run);
    EXPECT_EQ(run.Info.Kind, RunKind::Task);
    EXPECT_EQ(run.Info.State, RunState::Pending);
    EXPECT_EQ(LiveBehaviorSessionInstances(), 1u);

    AdvanceBehaviorSessionRuntime();
    sessions.ProcessFrame();
    RunInfo info;
    ASSERT_TRUE(sessions.ReadRun(run.Id, info));
    EXPECT_EQ(info.State, RunState::Ready);
    EXPECT_EQ(LiveBehaviorSessionInstances(), 1u);
    ASSERT_NE(sessions.Frames(run.Id), nullptr);
    EXPECT_EQ(sessions.Frames(run.Id)->Read().size(), 2u);
    sessions.CloseRun(run.Id);
    sessions.ProcessFrame();
    EXPECT_EQ(LiveBehaviorSessionInstances(), 0u);
}

TEST(BehaviorSessions, ManagedFailureUpdatesRunStateAndDiagnostic) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    OpenRun run = sessions.Start(
        session, nullptr, BlockSpec(CKGUID(15, 16)), Input("PendingFail"));
    ASSERT_TRUE(run);
    ASSERT_EQ(run.Info.State, RunState::Pending);
    ASSERT_TRUE(run.Info.LastStatus);

    AdvanceBehaviorSessionRuntime();
    sessions.ProcessFrame();

    RunInfo info;
    ASSERT_TRUE(sessions.ReadRun(run.Id, info));
    EXPECT_EQ(info.State, RunState::Failed);
    EXPECT_EQ(info.LastStatus.Code, Error::StaleLayout);
    EXPECT_EQ(info.LastStatus.Details.Stage, Phase::Execution);
    EXPECT_EQ(sessions.Frames(run.Id)->Read().size(), 1u);
}

TEST(BehaviorSessions, RejectedPulseDoesNotChangeTheRunState) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));
    OpenRun run = sessions.Spawn(
        session, nullptr, BlockSpec(CKGUID(13, 14)));
    ASSERT_TRUE(run);

    RunResult rejected = sessions.Pulse(run.Id, Input("Missing"));
    EXPECT_FALSE(rejected);
    EXPECT_EQ(rejected.Admission, AdmissionState::Failed);
    RunInfo info;
    ASSERT_TRUE(sessions.ReadRun(run.Id, info));
    EXPECT_EQ(info.State, RunState::Ready);
    EXPECT_EQ(info.LastStatus.Code, Error::SlotNotFound);
    EXPECT_EQ(LiveBehaviorSessionInstances(), 1u);

    RunResult accepted = sessions.Pulse(run.Id, Input("Run"));
    ASSERT_TRUE(accepted);
    EXPECT_EQ(accepted.State, RunState::Ready);
    sessions.CloseRun(run.Id);
    sessions.ProcessFrame();
    EXPECT_EQ(LiveBehaviorSessionInstances(), 0u);
}

TEST(BehaviorSessions, WorldResetClosesRunsButKeepsTheSession) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    OpenRun instance = sessions.Spawn(session, nullptr, BlockSpec(CKGUID(7, 8)));
    ASSERT_TRUE(instance);
    ASSERT_EQ(LiveBehaviorSessionInstances(), 1u);
    RunResult pulse = sessions.Pulse(
        instance.Id, Input("Run"));
    ASSERT_TRUE(pulse.Detail);
    EXPECT_EQ(pulse.State, RunState::Ready);
    sessions.ProcessFrame();
    EXPECT_EQ(LiveBehaviorSessionInstances(), 1u);

    sessions.ResetWorld();
    EXPECT_EQ(BehaviorSessionRuntimeWorldResets(), 1u);
    EXPECT_EQ(LiveBehaviorSessionInstances(), 0u);
    RunInfo stale;
    EXPECT_EQ(sessions.ReadRun(instance.Id, stale).Code, Error::InvalidState);

    OpenRun afterReset = sessions.Spawn(
        session, nullptr, BlockSpec(CKGUID(7, 8)));
    EXPECT_TRUE(afterReset);
}

TEST(BehaviorSessions, OffThreadSessionCloseDefersNativeTeardown) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));
    OpenRun run = sessions.Spawn(session, nullptr, BlockSpec(CKGUID(9, 10)));
    ASSERT_TRUE(run);
    ASSERT_EQ(LiveBehaviorSessionInstances(), 1u);

    std::thread close([&] { sessions.CloseSession(session); });
    close.join();
    EXPECT_EQ(LiveBehaviorSessionInstances(), 1u);
    RunInfo stale;
    EXPECT_EQ(sessions.ReadRun(run.Id, stale).Code, Error::InvalidState);

    sessions.ProcessFrame();
    EXPECT_EQ(LiveBehaviorSessionInstances(), 0u);
}

} // namespace
