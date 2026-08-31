#include "Behavior/Execution.h"
#include "Behavior/OutcomeStore.h"

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
    : m_Outcomes(std::make_shared<OutcomeStore>(retention)) {}

Execution::Execution(std::shared_ptr<OutcomeStore> outcomes)
    : m_Outcomes(std::move(outcomes)) {
    if (!m_Outcomes)
        m_Outcomes = std::make_shared<OutcomeStore>(OutcomeRetention::Signals());
}

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
    if (!native.Executed) {
        if (!native.Fault)
            native.Fault = Fault(
                ExecutionError::InvalidState,
                "Behavior execution was rejected before the native call.");
        FailBeforeExecute(native.Fault);
        return {AdmissionState::Failed, native.Fault, std::nullopt};
    }
    ExecutionOutcome outcome;
    outcome.Sequence = m_NextSequence++;
    outcome.Frame = frame;
    outcome.ReturnCode = native.ReturnCode;

    ExecutionFault outputFault;
    if (!native.Fault &&
        !adapter.ReadOutputs(outcome.ActiveOutputs, outcome.Pouts,
                             outputFault)) {
        if (!outputFault)
            outputFault = Fault(
                ExecutionError::PoutReadFailed,
                "Behavior outputs could not be copied into the Outcome.");
        native.Fault = outputFault;
        outcome.Pouts.clear();
    }

    if (!outcome.ActiveOutputs.empty()) {
        ExecutionFault clearFault;
        if (!adapter.ClearOutputs(outcome.ActiveOutputs, clearFault) &&
            !native.Fault) {
            if (!clearFault)
                clearFault = Fault(ExecutionError::OutputUnavailable,
                                   "Active Behavior outputs could not be cleared.");
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
    outcome.GraphActive = native.Kind == BehaviorKind::Graph && native.Active;
    outcome.QueuedInput = !m_QueuedInputs.empty();

    if (fatal) {
        m_Managed = false;
        m_NativeContinuation = false;
        m_QueuedInputs.clear();
        m_TerminalError = outcome.Fault;
        m_State = ExecutionState::Failed;
        outcome.NativeContinuation = false;
        outcome.GraphActive = false;
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
        outcome.GraphActive = false;
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
    return m_Outcomes->Drain();
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

void Execution::Retain(ExecutionOutcome outcome) {
    OutcomeRetainResult result = m_Outcomes->Retain(std::move(outcome));
    if (!result.Overflowed)
        return;
    m_TerminalError = std::move(result.TerminalFault);
    m_Managed = false;
    m_NativeContinuation = false;
    m_QueuedInputs.clear();
    m_State = ExecutionState::Closing;
}

} // namespace BML::Behavior
