#ifndef BML_BEHAVIOR_WATCH_H
#define BML_BEHAVIOR_WATCH_H

#include <atomic>
#include <functional>
#include <memory>

#include "Behavior/Callback.h"
#include "Behavior/Graph.h"

namespace BML::Behavior {

enum class WatchKind {
    GraphChanged,
    LayoutChanged,
    SampledValueChanged,
    ExactValueChanged,
};

struct WatchSpec {
    WatchKind Kind = WatchKind::GraphChanged;
    GraphView View = GraphView::Logical;
    NativeRef Root;
    NativeRef Node;
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

class WatchBinding final : public CallbackResource {
public:
    using Function = std::function<void(const WatchEvent &)>;

    WatchBinding(PlanCallbackState state, Function function);

    Status Invoke(const WatchEvent &event) noexcept;
    void CloseAdmission() noexcept override;
    [[nodiscard]] bool RetireAtSafePoint() noexcept override;

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

    GraphSource &m_Source;
    WatchSpec m_Spec;
    std::shared_ptr<WatchBinding> m_Binding;
    std::uint64_t m_Fingerprint = 0;
    GraphValue m_Value;
    std::uint64_t m_Sequence = 0;
    std::atomic<bool> m_Open{true};
};

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_WATCH_H
