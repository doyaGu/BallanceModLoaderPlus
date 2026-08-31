#include "Behavior/Execution.h"

#include <algorithm>
#include <utility>

namespace BML::Behavior {

namespace {

ExecutionFault Fault(ExecutionError code, std::string message, int nativeCode = 0) {
    return {code, nativeCode, std::move(message)};
}

} // namespace

ExecutionInput ExecutionInput::At(int index, std::uint64_t layoutGeneration) {
    ExecutionInput input;
    input.Selector = InputSelector::Index;
    input.Index = index;
    input.LayoutGeneration = layoutGeneration;
    return input;
}

ExecutionInput ExecutionInput::Named(std::string name, int occurrence,
                                     bool requireUnique) {
    ExecutionInput input;
    input.Selector = InputSelector::Name;
    input.Name = std::move(name);
    input.Occurrence = occurrence;
    input.RequireUnique = requireUnique;
    return input;
}

bool ExecutionInput::operator==(const ExecutionInput &other) const noexcept {
    if (Selector != other.Selector)
        return false;
    if (Selector == InputSelector::Index) {
        return Index == other.Index &&
               LayoutGeneration == other.LayoutGeneration;
    }
    return Name == other.Name && Occurrence == other.Occurrence &&
           RequireUnique == other.RequireUnique;
}

OutcomeRetention OutcomeRetention::Signals(std::size_t capacity) {
    return {RetentionKind::Signals, capacity};
}

OutcomeRetention OutcomeRetention::EachFrame(std::size_t capacity) {
    return {RetentionKind::EachFrame, capacity};
}

OutcomeRetention OutcomeRetention::Latest() {
    return {RetentionKind::Latest, 1};
}

OutcomeRetention OutcomeRetention::Ignore() {
    return {RetentionKind::Ignore, 0};
}

Execution::Execution(OutcomeRetention retention)
    : m_Retention(retention) {}

ExecutionResult Execution::Pulse(const ExecutionInput &input, std::uint64_t frame,
                                 ExecutionAdapter &adapter) {
    return Admit(input, frame, adapter, true);
}

ExecutionResult Execution::Call(const ExecutionInput &input, std::uint64_t frame,
                                ExecutionAdapter &adapter) {
    return Admit(input, frame, adapter, false);
}

ExecutionResult Execution::Admit(const ExecutionInput &input, std::uint64_t frame,
                                 ExecutionAdapter &adapter, bool managed) {
    if (m_State == ExecutionState::Closing ||
        m_State == ExecutionState::Closed ||
        m_State == ExecutionState::Failed) {
        return {AdmissionState::Failed,
                Fault(ExecutionError::InvalidState,
                      "Behavior instance is no longer accepting input."),
                std::nullopt};
    }

    ResolvedInput resolved;
    ExecutionFault fault;
    if (!adapter.Resolve(input, resolved, fault)) {
        if (!fault)
            fault = Fault(ExecutionError::SelectorNotFound,
                          "Behavior input could not be resolved.");
        return {AdmissionState::Failed, std::move(fault), std::nullopt};
    }

    if (managed)
        m_Managed = true;

    Queue(input);
    if (m_State == ExecutionState::Running || m_State == ExecutionState::Pending ||
        m_LastFrame == frame) {
        if (m_State != ExecutionState::Running)
            m_State = ExecutionState::Pending;
        return {AdmissionState::Queued, {}, std::nullopt};
    }

    return Run(frame, adapter);
}

ExecutionResult Execution::Step(std::uint64_t frame, ExecutionAdapter &adapter) {
    if (m_State == ExecutionState::Closing ||
        m_State == ExecutionState::Closed ||
        m_State == ExecutionState::Failed) {
        return {AdmissionState::Failed,
                Fault(ExecutionError::InvalidState,
                      "Behavior instance cannot be executed in its current state."),
                std::nullopt};
    }
    if (m_State == ExecutionState::Running || m_LastFrame == frame) {
        if (m_State != ExecutionState::Running)
            m_State = ExecutionState::Pending;
        return {AdmissionState::Queued, {}, std::nullopt};
    }
    return Run(frame, adapter);
}

ExecutionResult Execution::Run(std::uint64_t frame, ExecutionAdapter &adapter) {
    std::vector<ResolvedInput> resolved;
    resolved.reserve(m_QueuedInputs.size());
    for (const ExecutionInput &input : m_QueuedInputs) {
        ResolvedInput entry;
        ExecutionFault fault;
        if (!adapter.Resolve(input, entry, fault)) {
            if (!fault)
                fault = Fault(ExecutionError::SelectorNotFound,
                              "Queued behavior input no longer resolves.");
            FailBeforeExecute(fault);
            return {AdmissionState::Failed, std::move(fault), std::nullopt};
        }
        resolved.push_back(std::move(entry));
    }

    for (const ResolvedInput &input : resolved) {
        ExecutionFault fault;
        if (!adapter.Activate(input, fault)) {
            if (!fault)
                fault = Fault(ExecutionError::ActivationFailed,
                              "Behavior input activation failed.");
            FailBeforeExecute(fault);
            return {AdmissionState::Failed, std::move(fault), std::nullopt};
        }
    }

    m_QueuedInputs.clear();
    m_NativeContinuation = false;
    m_State = ExecutionState::Running;
    m_LastFrame = frame;

    NativeExecution native = adapter.Execute();
    ExecutionOutcome outcome;
    outcome.Sequence = m_NextSequence++;
    outcome.Frame = frame;
    outcome.ReturnCode = native.ReturnCode;

    ExecutionFault captureFault;
    if (!native.Fault &&
        !adapter.CaptureOutputs(outcome.ActiveOutputs, captureFault)) {
        if (!captureFault)
            captureFault = Fault(ExecutionError::CaptureFailed,
                                 "Active behavior outputs could not be captured.");
        native.Fault = captureFault;
    }

    if (!outcome.ActiveOutputs.empty()) {
        ExecutionFault clearFault;
        if (!adapter.ClearOutputs(outcome.ActiveOutputs, clearFault) &&
            !native.Fault) {
            if (!clearFault)
                clearFault = Fault(ExecutionError::CaptureFailed,
                                   "Captured behavior outputs could not be cleared.");
            native.Fault = clearFault;
        }
    }

    bool fatal = false;
    if (native.Break) {
        outcome.Fault = Fault(
            ExecutionError::UnsupportedBreak,
            "CKBR_BREAK requires the Virtools debugger message pump and is not supported for detached execution.",
            native.ReturnCode);
        fatal = true;
    } else if (native.Fault) {
        outcome.Fault = native.Fault;
        fatal = true;
    } else if (native.Error && !native.Retry) {
        outcome.Fault = Fault(ExecutionError::NativeFailed,
                              "Behavior execution returned a terminal error.",
                              native.ReturnCode);
        fatal = true;
    } else if (native.Error) {
        outcome.Fault = Fault(ExecutionError::NativeFailed,
                              "Behavior execution returned an error with retry.",
                              native.ReturnCode);
    }

    if (!fatal) {
        m_NativeContinuation = native.Kind == BehaviorKind::Function
            ? native.Retry : native.Active;
    }

    outcome.NativeContinuation = m_NativeContinuation;
    outcome.QueuedInput = !m_QueuedInputs.empty();

    if (fatal) {
        m_Managed = false;
        m_NativeContinuation = false;
        m_QueuedInputs.clear();
        m_TerminalError = outcome.Fault;
        m_State = ExecutionState::Failed;
        outcome.NativeContinuation = false;
        outcome.QueuedInput = false;
        outcome.Terminal = true;
    } else if (m_CloseRequested) {
        if (!m_TerminalError) {
            m_TerminalError = Fault(
                ExecutionError::Cancelled,
                "Behavior execution was closed while native execution was active.");
        }
        m_Managed = false;
        m_NativeContinuation = false;
        m_QueuedInputs.clear();
        m_State = ExecutionState::Closing;
        outcome.NativeContinuation = false;
        outcome.QueuedInput = false;
        outcome.Terminal = true;
        if (!outcome.Fault)
            outcome.Fault = m_TerminalError;
    } else if (outcome.NativeContinuation || outcome.QueuedInput) {
        m_State = ExecutionState::Pending;
    } else {
        m_State = ExecutionState::Idle;
        m_Managed = false;
        outcome.Terminal = true;
    }

    const ExecutionOutcome returned = outcome;
    Retain(std::move(outcome));
    return {AdmissionState::Executed, returned.Fault, returned};
}

void Execution::Continue() noexcept {
    if (m_State == ExecutionState::Pending)
        m_Managed = true;
}

void Execution::RequestClose(ExecutionFault reason) noexcept {
    if (m_State == ExecutionState::Closed)
        return;
    if (!reason && (!m_QueuedInputs.empty() || m_NativeContinuation)) {
        reason = Fault(ExecutionError::Cancelled,
                       "Pending behavior execution was cancelled.");
    }
    if (reason)
        m_TerminalError = std::move(reason);
    m_CloseRequested = true;
    m_Managed = false;
    m_NativeContinuation = false;
    m_QueuedInputs.clear();
    if (m_State != ExecutionState::Running)
        m_State = ExecutionState::Closing;
}

void Execution::MarkClosed() noexcept {
    m_Managed = false;
    m_NativeContinuation = false;
    m_QueuedInputs.clear();
    m_State = ExecutionState::Closed;
}

bool Execution::NeedsFrame() const noexcept {
    return m_Managed && m_State == ExecutionState::Pending;
}

std::vector<ExecutionOutcome> Execution::Drain() {
    std::vector<ExecutionOutcome> drained;
    drained.reserve(m_Outcomes.size() + (m_Latest ? 1u : 0u) +
                    (m_LastError ? 1u : 0u) +
                    (m_TerminalOutcome ? 1u : 0u));
    for (ExecutionOutcome &outcome : m_Outcomes)
        drained.push_back(std::move(outcome));
    m_Outcomes.clear();
    if (m_Latest)
        drained.push_back(std::move(*m_Latest));
    if (m_LastError)
        drained.push_back(std::move(*m_LastError));
    if (m_TerminalOutcome)
        drained.push_back(std::move(*m_TerminalOutcome));
    m_Latest.reset();
    m_LastError.reset();
    m_TerminalOutcome.reset();
    std::sort(drained.begin(), drained.end(),
              [](const ExecutionOutcome &left, const ExecutionOutcome &right) {
                  return left.Sequence < right.Sequence;
              });
    drained.erase(
        std::unique(drained.begin(), drained.end(),
                    [](const ExecutionOutcome &left, const ExecutionOutcome &right) {
                        return left.Sequence == right.Sequence;
                    }),
        drained.end());
    return drained;
}

bool Execution::Queue(const ExecutionInput &input) {
    if (std::find(m_QueuedInputs.begin(), m_QueuedInputs.end(), input) !=
        m_QueuedInputs.end()) {
        return false;
    }
    m_QueuedInputs.push_back(input);
    return true;
}

void Execution::FailBeforeExecute(ExecutionFault fault) noexcept {
    m_Managed = false;
    m_NativeContinuation = false;
    m_QueuedInputs.clear();
    m_TerminalError = std::move(fault);
    m_State = ExecutionState::Failed;
}

bool Execution::ShouldRetain(const ExecutionOutcome &outcome) const noexcept {
    switch (m_Retention.Kind) {
    case RetentionKind::Signals:
        return outcome.Sequence == 1 || !outcome.ActiveOutputs.empty() ||
               outcome.Terminal || static_cast<bool>(outcome.Fault);
    case RetentionKind::EachFrame:
        return true;
    case RetentionKind::Latest:
    case RetentionKind::Ignore:
        return false;
    }
    return false;
}

void Execution::Retain(ExecutionOutcome outcome) {
    if (m_Retention.Kind == RetentionKind::Latest) {
        if (outcome.Terminal) {
            StoreTerminal(std::move(outcome));
        } else if (outcome.Fault) {
            m_LastError = std::move(outcome);
        } else {
            m_Latest = std::move(outcome);
        }
        return;
    }

    if (m_Retention.Kind == RetentionKind::Ignore) {
        if (outcome.Terminal)
            StoreTerminal(std::move(outcome));
        else if (outcome.Fault)
            m_LastError = std::move(outcome);
        return;
    }

    if (!ShouldRetain(outcome))
        return;

    if (m_Outcomes.size() < m_Retention.Capacity) {
        m_Outcomes.push_back(std::move(outcome));
        return;
    }

    ExecutionOutcome terminal = outcome;
    terminal.Terminal = true;
    terminal.NativeContinuation = false;
    terminal.QueuedInput = false;
    terminal.Overflow = OutcomeOverflow{1, m_Retention.Kind,
                                        m_Retention.Capacity,
                                        terminal.Fault};
    terminal.Fault = Fault(
        ExecutionError::OutcomeQueueFull,
        "Behavior outcome retention is full; execution was stopped.",
        outcome.ReturnCode);
    m_TerminalError = terminal.Fault;
    m_Managed = false;
    m_NativeContinuation = false;
    m_QueuedInputs.clear();
    m_State = ExecutionState::Closing;
    StoreTerminal(std::move(terminal));
}

void Execution::StoreTerminal(ExecutionOutcome outcome) {
    m_TerminalOutcome = std::move(outcome);
}

} // namespace BML::Behavior
