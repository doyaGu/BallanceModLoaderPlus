#include "Behavior/Execution.h"
#include "Behavior/FrameStore.h"

#include <algorithm>
#include <utility>

namespace BML::Behavior::Internal {

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

FrameRetention FrameRetention::Signals(std::size_t capacity) {
    return {RetentionKind::Signals, capacity, false};
}

FrameRetention FrameRetention::EachFrame(std::size_t capacity) {
    return {RetentionKind::EachFrame, capacity, false};
}

FrameRetention FrameRetention::Latest() {
    return {RetentionKind::Latest, 1, false};
}

FrameRetention FrameRetention::Ignore() {
    return {RetentionKind::Ignore, 0, false};
}

Execution::Execution(FrameRetention retention)
    : m_Frames(std::make_shared<FrameStore>(retention)) {}

Execution::Execution(std::shared_ptr<FrameStore> frames)
    : m_Frames(std::move(frames)) {
    if (!m_Frames)
        m_Frames = std::make_shared<FrameStore>(FrameRetention::Signals());
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

    if (m_State == ExecutionState::Running || m_State == ExecutionState::Pending ||
        m_LastFrame == frame) {
        Queue(input);
        if (m_State != ExecutionState::Running)
            m_State = ExecutionState::Pending;
        return {AdmissionState::Queued, {}, std::nullopt};
    }

    return Run(frame, adapter, &resolved);
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

ExecutionResult Execution::Run(std::uint64_t ordinal, ExecutionAdapter &adapter,
                               const ResolvedInput *admitted) {
    m_ResolvedInputs.clear();
    if (admitted) {
        m_ResolvedInputs.push_back(*admitted);
    } else {
        m_ResolvedInputs.reserve(m_QueuedInputs.size());
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
            m_ResolvedInputs.push_back(entry);
        }
    }

    for (const ResolvedInput &input : m_ResolvedInputs) {
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
    m_LastFrame = ordinal;

    // Running is terminal for admission and close alike, so an exception
    // thrown by the adapter or by Frame construction must not strand the Run
    // there. Any unwinding past this point fails the Run instead.
    struct RunningScope {
        Execution &Owner;
        bool Armed = true;
        void Disarm() noexcept { Armed = false; }
        ~RunningScope() {
            if (!Armed || Owner.m_State != ExecutionState::Running)
                return;
            Owner.m_Failure = Fault(
                ExecutionError::InvalidState,
                "Behavior execution was interrupted by an exception.");
            Owner.m_Managed = false;
            Owner.m_NativeContinuation = false;
            Owner.m_QueuedInputs.clear();
            Owner.m_State = ExecutionState::Failed;
        }
    } running{*this};

    NativeExecution native = adapter.Execute();
    if (!native.Executed) {
        if (!native.Fault)
            native.Fault = Fault(
                ExecutionError::InvalidState,
                "Behavior execution was rejected before the native call.");
        FailBeforeExecute(native.Fault);
        return {AdmissionState::Failed, native.Fault, std::nullopt};
    }
    RunFrame frame;
    frame.Sequence = m_NextSequence++;
    frame.Frame = ordinal;
    frame.ReturnCode = native.ReturnCode;

    ExecutionFault outputFault;
    if (!native.Fault && !adapter.ReadOutputs(frame.ActiveOutputs,
                                               outputFault)) {
        if (!outputFault)
            outputFault = Fault(
                ExecutionError::OutUnavailable,
                "Active Behavior outputs could not be read.");
        native.Fault = outputFault;
    }

    bool fatal = false;
    if (native.Break) {
        frame.Fault = Fault(
            ExecutionError::UnsupportedBreak,
            "CKBR_BREAK requires the Virtools debugger message pump and is not supported for detached execution.",
            native.ReturnCode);
        fatal = true;
    } else if (native.Fault) {
        frame.Fault = native.Fault;
        fatal = true;
    } else if (native.Error && !native.Retry) {
        frame.Fault = Fault(ExecutionError::NativeFailed,
                              "Behavior execution returned a fatal error.",
                              native.ReturnCode);
        fatal = true;
    } else if (native.Error) {
        frame.Fault = Fault(ExecutionError::NativeFailed,
                              "Behavior execution returned an error with retry.",
                              native.ReturnCode);
    }

    if (!fatal)
        m_NativeContinuation = native.Active;

    frame.NativeContinuation = m_NativeContinuation;
    frame.QueuedInput = !m_QueuedInputs.empty();

    if (!fatal && m_Frames->KeepsPouts(frame)) {
        ExecutionFault poutFault;
        if (!adapter.ReadPouts(frame.Pouts, poutFault)) {
            if (!poutFault)
                poutFault = Fault(
                    ExecutionError::PoutReadFailed,
                    "Behavior Pouts could not be copied into the Frame.");
            frame.Pouts.clear();
            if (!fatal)
                frame.Fault = std::move(poutFault);
            fatal = true;
        }
    }

    if (!frame.ActiveOutputs.empty()) {
        ExecutionFault clearFault;
        if (!adapter.ClearOutputs(frame.ActiveOutputs, clearFault)) {
            if (!clearFault)
                clearFault = Fault(ExecutionError::OutUnavailable,
                                   "Active Behavior outputs could not be cleared.");
            if (!fatal)
                frame.Fault = std::move(clearFault);
            fatal = true;
        }
    }

    if (fatal) {
        m_Managed = false;
        m_NativeContinuation = false;
        m_QueuedInputs.clear();
        m_Failure = frame.Fault;
        m_State = ExecutionState::Failed;
        frame.NativeContinuation = false;
        frame.QueuedInput = false;
    } else if (m_CloseRequested) {
        if (!m_Failure) {
            m_Failure = Fault(
                ExecutionError::Cancelled,
                "Behavior execution was closed while native execution was active.");
        }
        m_Managed = false;
        m_NativeContinuation = false;
        m_QueuedInputs.clear();
        m_State = ExecutionState::Closing;
        frame.NativeContinuation = false;
        frame.QueuedInput = false;
        if (!frame.Fault)
            frame.Fault = m_Failure;
    } else if (frame.NativeContinuation || frame.QueuedInput) {
        m_State = ExecutionState::Pending;
    } else {
        m_State = ExecutionState::Idle;
        m_Managed = false;
    }

    FrameInfo info;
    info.Sequence = frame.Sequence;
    info.Frame = frame.Frame;
    info.ReturnCode = frame.ReturnCode;
    info.NativeContinuation = frame.NativeContinuation;
    info.QueuedInput = frame.QueuedInput;
    info.ActiveOutputs.reserve(frame.ActiveOutputs.size());
    for (const ExecutionOutput &output : frame.ActiveOutputs)
        info.ActiveOutputs.push_back(output.Index);

    ExecutionResult result{AdmissionState::Executed, frame.Fault,
                           std::move(info)};
    Retain(std::move(frame), result);
    running.Disarm();
    return result;
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
        m_Failure = std::move(reason);
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

std::vector<RunFrame> Execution::Take() {
    return m_Frames->Take();
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
    m_Failure = std::move(fault);
    m_State = ExecutionState::Failed;
}

void Execution::Retain(RunFrame frame, ExecutionResult &execution) {
    FrameAppendResult retained = m_Frames->Retain(std::move(frame));
    if (!retained.Overflowed)
        return;
    m_Failure = retained.Failure;
    execution.Fault = std::move(retained.Failure);
    execution.Overflow = std::move(retained.Overflow);
    if (execution.Frame) {
        execution.Frame->NativeContinuation = false;
        execution.Frame->QueuedInput = false;
    }
    m_Managed = false;
    m_NativeContinuation = false;
    m_QueuedInputs.clear();
    m_State = ExecutionState::Failed;
}

} // namespace BML::Behavior::Internal
