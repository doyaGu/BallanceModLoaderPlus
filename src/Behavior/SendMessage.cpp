#include "Behavior/SendMessage.h"

#include "BML/Guids/Logics.h"

namespace BML::Behavior::SendMessage {

Spec Make(const char *message, CKBeObject *destination) {
    Spec spec(VT_LOGICS_SENDMESSAGE);
    spec.Input(Slot::At(SlotKind::InputParameter, 0, CKPGUID_STRING),
               Value::String(message ? message : ""))
        .Input(Slot::At(SlotKind::InputParameter, 1, CKPGUID_BEOBJECT),
               Value::Object(CKPGUID_BEOBJECT, destination));
    return spec;
}

} // namespace BML::Behavior::SendMessage
