#ifndef BML_BEHAVIOR_SENDMESSAGE_H
#define BML_BEHAVIOR_SENDMESSAGE_H

#include "Behavior/Runtime.h"

namespace BML::Behavior::SendMessage {

Spec Make(const char *message, CKBeObject *destination);

} // namespace BML::Behavior::SendMessage

#endif // BML_BEHAVIOR_SENDMESSAGE_H
