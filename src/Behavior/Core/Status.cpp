#include "Behavior/Core/Status.h"

#include "Behavior/Execution.h"

namespace BML::Behavior::Internal {

const char *DescribeError(Error error) noexcept {
    switch (error) {
    case Error::None: return "none";
    case Error::WrongThread: return "wrong thread";
    case Error::ContextExpired: return "context expired";
    case Error::PrototypeNotFound: return "prototype not found";
    case Error::PrototypeChanged: return "prototype changed";
    case Error::RequiredManagerMissing: return "required manager missing";
    case Error::CreateFailed: return "creation failed";
    case Error::InitFailed: return "initialization failed";
    case Error::OwnerInvalid: return "invalid owner";
    case Error::TargetInvalid: return "invalid target";
    case Error::CallbackFailed: return "callback failed";
    case Error::SlotNotFound: return "slot not found";
    case Error::AmbiguousSlot: return "ambiguous slot";
    case Error::StaleLayout: return "stale layout";
    case Error::LayoutUnavailable: return "layout unavailable";
    case Error::TypeMismatch: return "type mismatch";
    case Error::ParameterTypeUnavailable: return "parameter type unavailable";
    case Error::ParameterTypeUnsupported: return "parameter type unsupported";
    case Error::InvalidArgument: return "invalid argument";
    case Error::ValueWriteFailed: return "value write failed";
    case Error::SourceInvalid: return "invalid source";
    case Error::InvalidState: return "invalid state";
    case Error::Busy: return "busy";
    case Error::Unavailable: return "unavailable";
    case Error::ExecutionFailed: return "execution failed";
    case Error::OperationInvalid: return "invalid parameter operation";
    case Error::UnsupportedBreak: return "unsupported break";
    case Error::UnsupportedPout: return "unsupported Pout";
    case Error::PoutUnavailable: return "Pout unavailable";
    case Error::FrameQueueFull: return "frame queue full";
    case Error::ExecutionCancelled: return "execution cancelled";
    case Error::DetachedUnsupported: return "detached execution unsupported";
    case Error::ObserverUnavailable: return "observer unavailable";
    case Error::GraphChanged: return "graph changed";
    case Error::InvalidGraphLocality: return "invalid graph locality";
    case Error::InvalidDelay: return "invalid Link delay";
    case Error::UnconfirmedSameFrameCycle:
        return "unconfirmed same-frame cycle";
    case Error::SharedSourceCycle: return "shared-source cycle";
    case Error::PushCycle: return "Pout destination cycle";
    case Error::InterfaceUnsupported: return "interface change unsupported";
    case Error::SourceConflict: return "Pin source conflict";
    case Error::SourceOrderCycle: return "Pin source order cycle";
    case Error::OrderingTargetMismatch: return "ordering target mismatch";
    case Error::OverlayOrderCycle: return "overlay order cycle";
    case Error::LinkNotFound: return "Link not found";
    case Error::PathAmbiguous: return "ambiguous Path";
    case Error::PathCycle: return "cyclic Path";
    case Error::QueryNotFound: return "query not found";
    case Error::QueryAmbiguous: return "ambiguous query";
    case Error::WorldBoundValue: return "world-bound value";
    case Error::RevertConflict: return "Patch revert conflict";
    case Error::TargetCardinality: return "target cardinality mismatch";
    case Error::RedirectConflict: return "Link Redirect conflict";
    }
    return "unknown";
}

const char *DescribePhase(Phase phase) noexcept {
    switch (phase) {
    case Phase::None: return "none";
    case Phase::PrototypeResolution: return "prototype resolution";
    case Phase::ManagerValidation: return "manager validation";
    case Phase::Creation: return "creation";
    case Phase::Initialization: return "initialization";
    case Phase::StaticLayout: return "static layout";
    case Phase::OwnerBinding: return "owner binding";
    case Phase::TargetBinding: return "target binding";
    case Phase::Settings: return "settings";
    case Phase::LifecycleCallback: return "lifecycle callback";
    case Phase::ParameterBinding: return "parameter binding";
    case Phase::Execution: return "execution";
    case Phase::Edit: return "edit";
    case Phase::Teardown: return "teardown";
    }
    return "unknown";
}

Error ToError(ExecutionError error) noexcept {
    switch (error) {
    case ExecutionError::None: return Error::None;
    case ExecutionError::SelectorNotFound: return Error::SlotNotFound;
    case ExecutionError::SelectorAmbiguous: return Error::AmbiguousSlot;
    case ExecutionError::LayoutStale: return Error::StaleLayout;
    case ExecutionError::UnsupportedBreak: return Error::UnsupportedBreak;
    case ExecutionError::UnsupportedPout: return Error::UnsupportedPout;
    case ExecutionError::PoutReadFailed: return Error::PoutUnavailable;
    case ExecutionError::FrameQueueFull: return Error::FrameQueueFull;
    case ExecutionError::Cancelled: return Error::ExecutionCancelled;
    case ExecutionError::InvalidState:
    case ExecutionError::ActivationFailed: return Error::InvalidState;
    case ExecutionError::OutUnavailable:
    case ExecutionError::NativeFailed: return Error::ExecutionFailed;
    }
    return Error::ExecutionFailed;
}

} // namespace BML::Behavior::Internal
