#ifndef BML_BEHAVIOR_SESSIONS_H
#define BML_BEHAVIOR_SESSIONS_H

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Behavior/Runtime.h"
#include "Behavior/Watch.h"

namespace BML::Behavior::Internal {

enum class OwnerState {
    Active,
    Retiring,
    Draining,
    Released,
};

enum class RunKind {
    Call,
    Task,
    Instance,
};

struct RunInfo {
    RunKind Kind = RunKind::Instance;
    RunState State = RunState::Ready;
    Status LastStatus;
    DetachedCompatibility Detached = DetachedCompatibility::Unverified;
    PrototypeRef Prototype;
};

struct OpenRun {
    Status Result;
    std::uintptr_t Id = 0;
    RunInfo Info;

    [[nodiscard]] explicit operator bool() const noexcept {
        return Id != 0 && static_cast<bool>(Result);
    }
};

struct SessionOwner {
    std::string Id;
    std::uint64_t Generation = 0;

    [[nodiscard]] explicit operator bool() const noexcept {
        return !Id.empty() && Generation != 0;
    }
};

// Owns the public Session and Run aggregates for every active Mod generation.
// Runtime remains responsible for each native Behavior Instance.
class Sessions final {
public:
    explicit Sessions(Runtime &runtime,
                      PrototypeCatalog *catalog = nullptr,
                      std::unique_ptr<GraphSource> graph = {});
    ~Sessions();

    std::uint64_t RegisterOwner(std::string ownerId);
    void RetireOwner(const std::string &ownerId);

    Status OpenSession(const std::string &ownerId, std::uintptr_t &sessionId);
    void CloseSession(std::uintptr_t sessionId);
    // Reads only Loader-owned identity and may be used by close requests from
    // any thread. It never enters CK2.
    Status ReadOwner(std::uintptr_t sessionId, SessionOwner &out) const;
    // Reads the active generation of a registered owner without opening a
    // Session. The Loader uses it for the Patches its own modules own.
    Status ReadOwner(const std::string &ownerId, SessionOwner &out) const;

    OpenRun Call(std::uintptr_t sessionId, CKBeObject *owner,
                 const BlockSpec &block, const Slot &input,
                 FrameRetention retention = FrameRetention::Signals());
    OpenRun Start(std::uintptr_t sessionId, CKBeObject *owner,
                  const BlockSpec &block, const Slot &input,
                  FrameRetention retention = FrameRetention::Signals());
    OpenRun Spawn(std::uintptr_t sessionId, CKBeObject *owner,
                  const BlockSpec &block,
                  FrameRetention retention = FrameRetention::Signals());
    // Parks a Block inside a live graph and keeps a Run for it. The graph
    // never activates the Block, so the Run is what drives it.
    OpenRun Attach(std::uintptr_t sessionId, CKBehavior *graph,
                   const BlockSpec &block,
                   FrameRetention retention = FrameRetention::Signals());
    RunResult Continue(std::uintptr_t runId);
    RunResult Pulse(std::uintptr_t runId, const Slot &input);

    Status ReadRun(std::uintptr_t runId, RunInfo &info) const;
    Status FindPrototypes(std::uintptr_t sessionId,
                          const PrototypeQuery &query,
                          std::vector<PrototypeInfo> &out);
    Status ReadDeclaredLayout(std::uintptr_t sessionId,
                              PrototypeRef prototype, Layout &out);
    Status ReadLiveLayout(std::uintptr_t runId, Layout &out) const;
    Status Set(std::uintptr_t runId, std::uint64_t layoutGeneration,
               const Slot &slot, const Parameter::Binding &value,
               std::uint64_t &currentGeneration);
    Status Bind(std::uintptr_t runId, std::uint64_t layoutGeneration,
                const Slot &slot, CKBehavior *source,
                std::uint64_t sourceLayoutGeneration,
                const Slot &sourceSlot, Parameter::BindingKind relation,
                std::uint64_t &currentGeneration);
    Status Configure(std::uintptr_t runId, const BlockSpec &settings,
                     std::uint64_t &layoutGeneration);
    Status ReadGraph(std::uintptr_t runId, GraphView view,
                     GraphModel &out);
    Status ReadGraph(std::uintptr_t sessionId, void *root,
                     GraphView view, GraphModel &out);
    Status ReadNodeLayout(std::uintptr_t sessionId, void *node, Layout &out);
    Status ReadGraphValue(std::uintptr_t sessionId, void *node,
                          std::uint64_t layoutGeneration, const Slot &slot,
                          ReadMode mode, GraphValue &out);
    Status OpenWatch(std::uintptr_t sessionId, void *root, void *node,
                     WatchSpec spec, PlanCallbackState state,
                     WatchBinding::Function callback,
                     std::uintptr_t &watchId);
    Status ReadWatch(std::uintptr_t watchId, WatchInfo &info) const;
    void CloseWatch(std::uintptr_t watchId);
    std::shared_ptr<FrameStore> Frames(std::uintptr_t runId) const;
    void CloseRun(std::uintptr_t runId);

    void ProcessFrame();
    void ResetWorld();

    [[nodiscard]] GraphSource *Graph() noexcept { return m_Graph.get(); }

private:
    struct Owner {
        std::string Id;
        std::uint64_t Generation = 0;
        OwnerState State = OwnerState::Released;
    };

    struct Session {
        std::uintptr_t Id = 0;
        std::string OwnerId;
        std::uint64_t OwnerGeneration = 0;
    };

    struct Run {
        std::uintptr_t Id = 0;
        std::uintptr_t SessionId = 0;
        std::string OwnerId;
        std::uint64_t OwnerGeneration = 0;
        RunInfo Info;
        Instance Block;
        std::shared_ptr<FrameStore> Frames;
    };

    struct OwnedWatch {
        std::uintptr_t Id = 0;
        std::uintptr_t SessionId = 0;
        std::string OwnerId;
        std::uint64_t OwnerGeneration = 0;
        std::shared_ptr<Watch> Value;
    };

    [[nodiscard]] Status Ready() const;
    [[nodiscard]] std::uintptr_t NextId();
    [[nodiscard]] Session *FindSession(std::uintptr_t sessionId);
    [[nodiscard]] const Session *FindSession(std::uintptr_t sessionId) const;
    [[nodiscard]] std::shared_ptr<Run> FindRun(std::uintptr_t runId);
    [[nodiscard]] std::shared_ptr<const Run> FindRun(
        std::uintptr_t runId) const;
    [[nodiscard]] bool SessionIsActive(const Session &session) const;
    [[nodiscard]] OpenRun AddRun(const Session &session, RunKind kind,
                                 Instance block, RunResult result,
                                 DetachedCompatibility compatibility,
                                 PrototypeRef prototype);
    void QueueClose(std::shared_ptr<Run> run);
    [[nodiscard]] bool CloseQueuedRuns();
    void CloseOwner(const std::string &ownerId, std::uint64_t generation);
    void DrainOwner(const std::string &ownerId, std::uint64_t generation);
    void QueueWatch(std::shared_ptr<Watch> watch);
    void CollectWatches();
    void RetireWorldHandles();

    Runtime &m_Runtime;
    PrototypeCatalog *m_Catalog = nullptr;
    std::unique_ptr<GraphSource> m_Graph;
    std::thread::id m_Thread;
    mutable std::recursive_mutex m_Mutex;
    std::uintptr_t m_NextId = 1;
    std::uint64_t m_NextOwnerGeneration = 1;
    std::unordered_map<std::string, Owner> m_Owners;
    std::unordered_map<std::uintptr_t, Session> m_Sessions;
    std::unordered_map<std::uintptr_t, std::shared_ptr<Run>> m_Runs;
    std::vector<std::shared_ptr<Run>> m_CloseQueue;
    std::unordered_map<std::uintptr_t, OwnedWatch> m_Watches;
    std::vector<std::shared_ptr<Watch>> m_ClosingWatches;
    std::vector<std::pair<std::uintptr_t, std::shared_ptr<Watch>>>
        m_FrameWatches;
    WatchReadings m_WatchReadings;
    bool m_ProcessingFrame = false;
    std::uint64_t m_Frame = 0;
};

} // namespace BML::Behavior::Internal

#endif // BML_BEHAVIOR_SESSIONS_H
