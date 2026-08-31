#ifndef BML_BEHAVIOR_EXECUTION_H
#define BML_BEHAVIOR_EXECUTION_H

#include <cstddef>
#include <cstdint>
#include <array>
#include <optional>
#include <memory>
#include <string>
#include <vector>

namespace BML::Behavior {

enum class ExecutionState {
    Idle,
    Pending,
    Running,
    Closing,
    Closed,
    Failed,
};

enum class ExecutionError {
    None,
    InvalidState,
    SelectorNotFound,
    SelectorAmbiguous,
    LayoutStale,
    ActivationFailed,
    NativeFailed,
    UnsupportedBreak,
    OutputUnavailable,
    UnsupportedPout,
    PoutReadFailed,
    OutcomeQueueFull,
    Cancelled,
};

struct ExecutionFault {
    ExecutionError Code = ExecutionError::None;
    int NativeCode = 0;
    std::string Message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return Code != ExecutionError::None;
    }
};

enum class InputSelector {
    Index,
    Name,
};

struct ExecutionInput {
    InputSelector Selector = InputSelector::Index;
    int Index = -1;
    std::uint64_t LayoutGeneration = 0;
    std::string Name;
    int Occurrence = 0;
    bool RequireUnique = false;

    static ExecutionInput At(int index, std::uint64_t layoutGeneration);
    static ExecutionInput Named(std::string name, int occurrence = 0,
                                bool requireUnique = false);

    [[nodiscard]] bool operator==(const ExecutionInput &other) const noexcept;
};

struct ResolvedInput {
    int Index = -1;
    std::string Name;
    int Occurrence = 0;
};

struct ExecutionOutput {
    int Index = -1;
    std::string Name;
    int Occurrence = 0;
};

enum class PoutKind : std::uint32_t {
    Bool,
    Int32,
    Float32,
    Utf8,
    Vec2,
    Vec3,
    Quaternion,
    Euler,
    Rect,
    Color,
    Box,
    Mat4,
    Object,
};

struct Pout {
    int Index = -1;
    std::string Name;
    int Occurrence = 0;
    std::uint32_t TypeGuid1 = 0;
    std::uint32_t TypeGuid2 = 0;
    PoutKind Kind = PoutKind::Int32;
    std::int32_t Int32 = 0;
    float Float32 = 0.0f;
    std::array<float, 16> Components{};
    std::uint32_t ComponentCount = 0;
    std::string Text;
    std::uint32_t ObjectDomain = 0;
    std::uint32_t ObjectSlot = 0;
    std::uint32_t ObjectGeneration = 0;
};

enum class BehaviorKind {
    Function,
    Graph,
};

struct NativeExecution {
    BehaviorKind Kind = BehaviorKind::Function;
    int ReturnCode = 0;
    bool Retry = false;
    bool Active = false;
    bool Error = false;
    bool Break = false;
    ExecutionFault Fault;
    bool Executed = true;
};

class ExecutionAdapter {
public:
    virtual ~ExecutionAdapter() = default;

    virtual bool Resolve(const ExecutionInput &input, ResolvedInput &resolved,
                         ExecutionFault &fault) = 0;
    virtual bool Activate(const ResolvedInput &input, ExecutionFault &fault) = 0;
    virtual NativeExecution Execute() = 0;
    virtual bool ReadOutputs(std::vector<ExecutionOutput> &activeOutputs,
                             std::vector<Pout> &pouts,
                             ExecutionFault &fault) = 0;
    virtual bool ClearOutputs(const std::vector<ExecutionOutput> &outputs,
                              ExecutionFault &fault) = 0;
};

enum class RetentionKind {
    Signals,
    EachFrame,
    Latest,
    Ignore,
};

struct OutcomeRetention {
    RetentionKind Kind = RetentionKind::Signals;
    std::size_t Capacity = 64;

    static OutcomeRetention Signals(std::size_t capacity = 64);
    static OutcomeRetention EachFrame(std::size_t capacity);
    static OutcomeRetention Latest();
    static OutcomeRetention Ignore();
};

struct OutcomeOverflow {
    std::size_t Dropped = 0;
    RetentionKind Policy = RetentionKind::Signals;
    std::size_t Capacity = 0;
    ExecutionFault Cause;
};

struct ExecutionOutcome {
    std::uint64_t Sequence = 0;
    std::uint64_t Frame = 0;
    int ReturnCode = 0;
    bool NativeContinuation = false;
    bool GraphActive = false;
    bool QueuedInput = false;
    bool Terminal = false;
    ExecutionFault Fault;
    std::vector<ExecutionOutput> ActiveOutputs;
    std::vector<Pout> Pouts;
    std::optional<OutcomeOverflow> Overflow;
};

enum class AdmissionState {
    Executed,
    Queued,
    Failed,
};

struct ExecutionResult {
    AdmissionState State = AdmissionState::Failed;
    ExecutionFault Fault;
    std::optional<ExecutionOutcome> Outcome;

    [[nodiscard]] explicit operator bool() const noexcept {
        return State != AdmissionState::Failed;
    }
};

class Execution final {
public:
    explicit Execution(OutcomeRetention retention = OutcomeRetention::Signals());
    explicit Execution(std::shared_ptr<class OutcomeStore> outcomes);

    ExecutionResult Pulse(const ExecutionInput &input, std::uint64_t frame,
                          ExecutionAdapter &adapter);
    ExecutionResult Call(const ExecutionInput &input, std::uint64_t frame,
                         ExecutionAdapter &adapter);
    ExecutionResult Step(std::uint64_t frame, ExecutionAdapter &adapter);

    void Continue() noexcept;
    void RequestClose(ExecutionFault reason = {}) noexcept;
    void MarkClosed() noexcept;

    [[nodiscard]] ExecutionState State() const noexcept { return m_State; }
    [[nodiscard]] bool Managed() const noexcept { return m_Managed; }
    [[nodiscard]] bool NeedsFrame() const noexcept;
    [[nodiscard]] const ExecutionFault &TerminalError() const noexcept {
        return m_TerminalError;
    }
    [[nodiscard]] std::uint64_t NextSequence() const noexcept {
        return m_NextSequence;
    }
    [[nodiscard]] std::vector<ExecutionOutcome> Drain();
    [[nodiscard]] const std::shared_ptr<class OutcomeStore> &Outcomes() const noexcept {
        return m_Outcomes;
    }

private:
    ExecutionResult Admit(const ExecutionInput &input, std::uint64_t frame,
                          ExecutionAdapter &adapter, bool managed);
    ExecutionResult Run(std::uint64_t frame, ExecutionAdapter &adapter);
    bool Queue(const ExecutionInput &input);
    void FailBeforeExecute(ExecutionFault fault) noexcept;
    void Retain(ExecutionOutcome outcome);

    ExecutionState m_State = ExecutionState::Idle;
    bool m_Managed = false;
    bool m_NativeContinuation = false;
    bool m_CloseRequested = false;
    std::uint64_t m_LastFrame = static_cast<std::uint64_t>(-1);
    std::uint64_t m_NextSequence = 1;
    std::vector<ExecutionInput> m_QueuedInputs;
    std::shared_ptr<class OutcomeStore> m_Outcomes;
    ExecutionFault m_TerminalError;
};

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_EXECUTION_H
