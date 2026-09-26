#ifndef BML_BEHAVIOR_ENGINE_POUT_H
#define BML_BEHAVIOR_ENGINE_POUT_H

#include <cstddef>
#include <functional>

#include "CKAll.h"
#include "Behavior/Core/ObjectRef.h"
#include "Behavior/Execution.h"

namespace BML::Behavior::Internal::Engine {

// The Frame value form of a Pout parameter type and, for numeric forms, its
// native value size. False when the type has no Frame representation.
bool PoutForm(CKContext *context, CKParameter *parameter, PoutKind &kind,
              std::size_t &size);

// Copies the current value of one Pout into a Frame value whose Kind and
// port identity the caller has already set. Object values are retained
// through issue; size is the native size PoutForm reported.
bool ReadPout(CKContext *context, CKParameterOut *parameter, std::size_t size,
              const std::function<ObjectRef(const void *)> &issue,
              Pout &value, ExecutionFault &fault);

} // namespace BML::Behavior::Internal::Engine

#endif // BML_BEHAVIOR_ENGINE_POUT_H
