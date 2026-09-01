#ifndef BML_BEHAVIOR_STATUS_H
#define BML_BEHAVIOR_STATUS_H

#include <string>
#include <utility>

#include "Behavior/Layout.h"

namespace BML::Behavior {

enum class Error {
    None,
    WrongThread,
    ContextExpired,
    PrototypeNotFound,
    PrototypeChanged,
    PrototypeLoadFailed,
    RequiredManagerMissing,
    CreateFailed,
    InitFailed,
    OwnerInvalid,
    TargetInvalid,
    CallbackFailed,
    SlotNotFound,
    AmbiguousSlot,
    StaleLayout,
    LayoutUnavailable,
    TypeMismatch,
    ParameterTypeUnavailable,
    ParameterTypeUnsupported,
    ValueWriteFailed,
    SourceInvalid,
    InvalidState,
    Unavailable,
    ExecutionFailed,
    OperationInvalid,
    UnsupportedBreak,
    UnsupportedPout,
    PoutUnavailable,
    FrameQueueFull,
    ExecutionCancelled,
    DetachedUnsupported,
    ObserverUnavailable,
    GraphChanged,
    InvalidGraphLocality,
    InvalidDelay,
    UnconfirmedSameFrameCycle,
    SharedSourceCycle,
    PushCycle,
    InterfaceUnsupported,
    OrderingTargetMismatch,
    OverlayOrderCycle,
    LinkNotFound,
    PathAmbiguous,
    PathCycle,
    RevertConflict,
    TargetCardinality,
};

enum class Phase {
    None,
    PrototypeResolution,
    ManagerValidation,
    Creation,
    Initialization,
    StaticLayout,
    OwnerBinding,
    TargetBinding,
    Settings,
    LifecycleCallback,
    ParameterBinding,
    Execution,
    Edit,
    Teardown,
};

struct Diagnostic {
    Diagnostic()
        : Prototype(), RequiredManager(), Selector(), ActualType(),
          OperationGuid() {}

    Phase Stage = Phase::None;
    CKGUID Prototype = CKGUID();
    CKGUID RequiredManager = CKGUID();
    Slot Selector;
    CKGUID ActualType = CKGUID();
    CKGUID OperationGuid = CKGUID();
    CKDWORD CallbackMessage = 0;
};

struct Status {
    Status() = default;
    Status(Error error, CKERROR ckError, int behaviorResult,
           std::string message)
        : Code(error), CkError(ckError), BehaviorResult(behaviorResult),
          Message(std::move(message)) {}

    Error Code = Error::None;
    CKERROR CkError = CK_OK;
    int BehaviorResult = CKBR_OK;
    std::string Message;
    Diagnostic Details;

    explicit operator bool() const noexcept { return Code == Error::None; }
};

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_STATUS_H
