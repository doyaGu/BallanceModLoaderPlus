// The 2D Text Building Block: fonts, alignment, margins, and the caret.
// The retail parameter names are spelled here, so a Mod states what it
// means instead of counting slots.
#ifndef BML_BEHAVIOR_BLOCKS_TEXT2D_HPP
#define BML_BEHAVIOR_BLOCKS_TEXT2D_HPP

#include <string>
#include <string_view>

#include "CKAll.h"
#include "BML/Behavior/Detail/Blocks.hpp"
#include "BML/Guids/Interface.h"

namespace BML::Behavior::Blocks {
namespace Text2D {

struct Options {
    [[nodiscard]] static CKGUID Prototype() noexcept {
        return VT_INTERFACE_2DTEXT;
    }

    CK2dEntity *Target = nullptr;
    int FontIndex = 0;
    std::string Text;
    int Alignment = 0;
    VxRect Margin{2.0f, 2.0f, 2.0f, 2.0f};
    Vx2DVector Offset{0.0f, 0.0f};
    Vx2DVector ParagraphIndentation{0.0f, 0.0f};
    CKMaterial *BackgroundMaterial = nullptr;
    float CaretSize = 0.1f;
    CKMaterial *CaretMaterial = nullptr;
    int Flags = 1;

private:
    template <class Definition>
    void Configure(Definition &block) const {
        block.Target(CKPGUID_2DENTITY, Target);
        block.Pin(0, CKPGUID_FONT, FontIndex);
        block.Pin(1, CKPGUID_STRING, std::string_view(Text));
        block.Pin(2, CKPGUID_ALIGNMENT, Alignment);
        block.Pin(3, CKPGUID_RECT, Margin);
        block.Pin(4, CKPGUID_2DVECTOR, Offset);
        block.Pin(5, CKPGUID_2DVECTOR, ParagraphIndentation);
        block.ObjectPin(6, CKPGUID_MATERIAL, BackgroundMaterial);
        block.Pin(7, CKPGUID_PERCENTAGE, CaretSize);
        block.ObjectPin(8, CKPGUID_MATERIAL, CaretMaterial);
        block.Setting(0, CKPGUID_TEXTPROPERTIES, Flags);
    }

    friend class BML::Behavior::Detail::BlockAccess;
};

inline Result<Block> Make(const Session &session, const Options &options) {
    BML::Behavior::Detail::Definition block(session, Options::Prototype());
    BML::Behavior::Detail::BlockAccess::Configure(options, block);
    return std::move(block).Build();
}

} // namespace Text2D
} // namespace BML::Behavior::Blocks

#endif // BML_BEHAVIOR_BLOCKS_TEXT2D_HPP
