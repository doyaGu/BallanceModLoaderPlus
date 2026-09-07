#ifndef BML_BEHAVIOR_WATCH_H
#define BML_BEHAVIOR_WATCH_H

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "Behavior/Callback.h"
#include "Behavior/Graph.h"

namespace BML::Behavior::Internal {

enum class WatchKind {
    GraphChanged,
    LayoutChanged,
    SampledValueChanged,
};

struct WatchSpec {
    WatchKind Kind = WatchKind::GraphChanged;
    GraphView View = GraphView::Logical;
    NativeRef Root;
    NativeRef Node;
    std::uint64_t LayoutGeneration = 0;
    Slot ValueSlot;
    ReadMode Read = ReadMode::NonForcing;
};

struct WatchEvent {
    WatchKind Kind = WatchKind::GraphChanged;
    std::uint64_t Sequence = 0;
    std::uint64_t Frame = 0;
    std::uint64_t Before = 0;
    std::uint64_t After = 0;
    GraphValue PreviousValue;
    GraphValue CurrentValue;
};

enum class WatchState {
    Active,
    Failed,
};

struct WatchInfo {
    WatchState State = WatchState::Active;
    Status Diagnostic;
};

// One polling frame observes a graph or layout once, even when several
// Watches ask the same question. Baselines remain independent because a Watch
// may be opened between native graph changes.
class WatchReadings final {
public:
    void BeginFrame(std::uint64_t frame) noexcept;
    Status GraphFingerprint(GraphSource &source, const NativeRef &root,
                            GraphView view, std::uint64_t &out);
    Status LayoutFingerprint(GraphSource &source, const NativeRef &node,
                             std::uint64_t &out);

private:
    struct GraphKey {
        GraphSource *Source = nullptr;
        NativeRef Root;
        GraphView View = GraphView::Logical;

        friend bool operator==(const GraphKey &, const GraphKey &) = default;
    };

    struct LayoutKey {
        GraphSource *Source = nullptr;
        NativeRef Node;

        friend bool operator==(const LayoutKey &, const LayoutKey &) = default;
    };

    struct GraphKeyHash {
        std::size_t operator()(const GraphKey &key) const noexcept;
    };

    struct LayoutKeyHash {
        std::size_t operator()(const LayoutKey &key) const noexcept;
    };

    struct GraphReading {
        std::uint64_t Frame = 0;
        std::uint64_t Fingerprint = 0;
        Status Result;
    };

    struct LayoutReading {
        std::uint64_t Frame = 0;
        std::uint64_t Fingerprint = 0;
        Status Result;
    };

    template <class Readings>
    void ForgetUnused(Readings &readings);

    std::unordered_map<GraphKey, GraphReading, GraphKeyHash> m_Graphs;
    std::unordered_map<LayoutKey, LayoutReading, LayoutKeyHash> m_Layouts;
    std::uint64_t m_Frame = 1;
};

class WatchBinding final : public CallbackResource {
public:
    using Function = std::function<void(const WatchEvent &)>;

    WatchBinding(PlanCallbackState state, Function function);

    Status Invoke(const WatchEvent &event) noexcept;
    void CloseAdmission() noexcept override;
    void AdmitThrough(std::shared_ptr<const CallbackAdmission> admission) override {
        m_Lease.AdmitThrough(std::move(admission));
    }
    [[nodiscard]] bool RetireAtSafePoint() noexcept override;
    // False when the callback lease never opened, e.g. the state was already
    // retired or its Retain was still in progress.
    [[nodiscard]] bool IsOpen() const noexcept;

private:
    PlanCallbackState m_State;
    CallbackLease m_Lease;
    Function m_Function;
    std::atomic<bool> m_Retired{false};
};

class Watch final {
public:
    static Status Open(GraphSource &source, WatchSpec spec,
                       PlanCallbackState state,
                       WatchBinding::Function callback,
                       std::shared_ptr<Watch> &out);

    Status Poll(std::uint64_t frame);
    Status Poll(std::uint64_t frame, WatchReadings &readings);
    [[nodiscard]] WatchInfo Read() const;
    void Close() noexcept;
    [[nodiscard]] bool RetireAtSafePoint() noexcept;
    [[nodiscard]] bool IsOpen() const noexcept {
        return m_Open.load(std::memory_order_acquire);
    }

private:
    Watch(GraphSource &source, WatchSpec spec,
          std::shared_ptr<WatchBinding> binding)
        : m_Source(source), m_Spec(std::move(spec)),
          m_Binding(std::move(binding)) {}

    Status ReadBaseline();
    Status Poll(std::uint64_t frame, WatchReadings *readings);
    void Fail(Status status);

    GraphSource &m_Source;
    WatchSpec m_Spec;
    std::shared_ptr<WatchBinding> m_Binding;
    std::uint64_t m_Fingerprint = 0;
    GraphValue m_Value;
    std::uint64_t m_Sequence = 0;
    mutable std::mutex m_StateMutex;
    WatchInfo m_Info;
    std::atomic<bool> m_Open{true};
};

} // namespace BML::Behavior::Internal

#endif // BML_BEHAVIOR_WATCH_H
