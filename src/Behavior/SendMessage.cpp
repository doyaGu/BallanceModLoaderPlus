#include "Behavior/SendMessage.h"

#include "BML/Guids/Logics.h"

namespace BML::Behavior::SendMessage {

Spec Make(const char *message, CKBeObject *destination) {
    Spec spec(VT_LOGICS_SENDMESSAGE);
    // "Send Message" declares its first pin as CKPGUID_MESSAGE, which is not
    // derived from CKPGUID_STRING. The text is written through the Message
    // type's string function, which registers the message name.
    spec.Input(Slot::At(SlotKind::InputParameter, 0, CKPGUID_MESSAGE),
               Value::Text(CKPGUID_MESSAGE, message ? message : ""))
        .Input(Slot::At(SlotKind::InputParameter, 1, CKPGUID_BEOBJECT),
               Parameter::Binding::Object(CKPGUID_BEOBJECT, destination));
    return spec;
}

} // namespace BML::Behavior::SendMessage
