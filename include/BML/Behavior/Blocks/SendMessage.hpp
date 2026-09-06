// The Send Message Building Block. Its Message pin is a CKPGUID_MESSAGE,
// not a string: the text travels as the Message type's string form, which
// registers the message name.
#ifndef BML_BEHAVIOR_BLOCKS_SENDMESSAGE_HPP
#define BML_BEHAVIOR_BLOCKS_SENDMESSAGE_HPP

#include <string>
#include <string_view>

#include "CKAll.h"
#include "BML/Behavior/Detail/Blocks.hpp"
#include "BML/Guids/Logics.h"

namespace BML::Behavior::Blocks {
namespace Send {

struct Options {
    [[nodiscard]] static CKGUID Prototype() noexcept {
        return VT_LOGICS_SENDMESSAGE;
    }

    std::string Message;
    CKBeObject *Destination = nullptr;

private:
    template <class Definition>
    void Configure(Definition &block) const {
        block.Pin(0, CKPGUID_MESSAGE, std::string_view(Message));
        block.ObjectPin(1, CKPGUID_BEOBJECT, Destination);
    }

    friend class BML::Behavior::Detail::BlockAccess;
};

// "Send Message" declares its Message pin as CKPGUID_MESSAGE, which is not
// derived from CKPGUID_STRING. The text travels as the Message type's string
// form, which registers the message name.
inline Result<Block> Make(const Session &session, const Options &options) {
    BML::Behavior::Detail::Definition block(session, Options::Prototype());
    BML::Behavior::Detail::BlockAccess::Configure(options, block);
    return std::move(block).Build();
}

} // namespace Send
} // namespace BML::Behavior::Blocks

#endif // BML_BEHAVIOR_BLOCKS_SENDMESSAGE_HPP
