// The Send Message Building Block. Its Message pin is a CKPGUID_MESSAGE,
// not a string: the text travels as the Message type's string form, which
// registers the message name.
#ifndef BML_BEHAVIOR_BLOCKS_SENDMESSAGE_HPP
#define BML_BEHAVIOR_BLOCKS_SENDMESSAGE_HPP

#include <string>
#include <string_view>

#include "CKAll.h"
#ifdef BML_BEHAVIOR_INTERNAL
#include "Behavior/Blocks/Definition.h"
#else
#include "BML/Behavior/Detail/Blocks.hpp"
#endif
#include "BML/Guids/Logics.h"

namespace BML::Behavior::Blocks {
namespace SendMessage {

struct Options {
    std::string Message;
    CKBeObject *Destination = nullptr;
};

namespace Detail {
template <class Definition>
void Define(Definition &block, const Options &options) {
    block.Pin(0, CKPGUID_MESSAGE, std::string_view(options.Message));
    block.ObjectPin(1, CKPGUID_BEOBJECT, options.Destination);
}
} // namespace Detail

// "Send Message" declares its Message pin as CKPGUID_MESSAGE, which is not
// derived from CKPGUID_STRING. The text travels as the Message type's string
// form, which registers the message name.
#ifdef BML_BEHAVIOR_INTERNAL
inline Spec Make(const Options &options) {
    Blocks::Detail::Definition block(VT_LOGICS_SENDMESSAGE);
    Detail::Define(block, options);
    return std::move(block).Build();
}
#else
inline Result<Block> Make(const Session &session, const Options &options) {
    BML::Behavior::Detail::Definition block(session, VT_LOGICS_SENDMESSAGE);
    Detail::Define(block, options);
    return std::move(block).Build();
}
#endif


} // namespace SendMessage
} // namespace BML::Behavior::Blocks

#endif // BML_BEHAVIOR_BLOCKS_SENDMESSAGE_HPP
