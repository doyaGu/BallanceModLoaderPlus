#ifndef BML_BEHAVIOR_EXECUTION_H
#define BML_BEHAVIOR_EXECUTION_H

#include <cstddef>
#include <cstdint>
#include <optional>
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
    CaptureFailed,
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
};

class ExecutionAdapter {
public:
    virtual ~ExecutionAdapter() = default;

    virtual bool Resolve(const ExecutionInput &input, ResolvedInput &resolved,
                         ExecutionFault &fault) = 0;
    virtual bool Activate(const ResolvedInput &input, ExecutionFault &fault) = 0;
    virtual NativeExecution Execute() = 0;
    virtual bool CaptureOutputs(std::vector<ExecutionOutput> &outputs,
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
};

struct ExecutionOutcome {
    std::uint64_t Sequence = 0;
    std::uint64_t Frame = 0;
    int ReturnCode = 0;
    bool NativeContinuation = false;
    bool QueuedInput = false;
    bool Terminal = false;
    ExecutionFault Fault;
    std::vector<ExecutionOutput> ActiveOutputs;
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

private:
    ExecutionResult Admit(const ExecutionInput &input, std::uint64_t frame,
                          ExecutionAdapter &adapter, bool managed);
    ExecutionResult Run(std::uint64_t frame, ExecutionAdapter &adapter);
    bool Queue(const ExecutionInput &input);
    void FailBeforeExecute(ExecutionFault fault) noexcept;
    void Retain(ExecutionOutcome outcome);
    void StoreTerminal(ExecutionOutcome outcome);
    [[nodiscard]] bool ShouldRetain(const ExecutionOutcome &outcome) const noexcept;

    OutcomeRetention m_Retention;
    ExecutionState m_State = ExecutionState::Idle;
    bool m_Managed = false;
    bool m_NativeContinuation = false;
    bool m_CloseRequested = false;
    bool m_Executed = false;
    std::uint64_t m_LastFrame = static_cast<std::uint64_t>(-1);
    std::uint64_t m_NextSequence = 1;
    std::vector<ExecutionInput> m_QueuedInputs;
    std::vector<ExecutionOutcome> m_Outcomes;
    std::optional<ExecutionOutcome> m_Latest;
    std::optional<ExecutionOutcome> m_LastError;
    std::optional<ExecutionOutcome> m_TerminalOutcome;
    ExecutionFault m_TerminalError;
};

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_EXECUTION_H
