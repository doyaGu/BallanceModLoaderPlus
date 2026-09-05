// The 2D Text Building Block: fonts, alignment, margins, and the caret.
// The retail parameter names are spelled here, so a Mod states what it
// means instead of counting slots.
#ifndef BML_BEHAVIOR_BLOCKS_TEXT2D_HPP
#define BML_BEHAVIOR_BLOCKS_TEXT2D_HPP

#include <string>
#include <string_view>

#include "CKAll.h"
#ifdef BML_BEHAVIOR_INTERNAL
#include "Behavior/Blocks/Definition.h"
#else
#include "BML/Behavior/Detail/Blocks.hpp"
#endif
#include "BML/Guids/Interface.h"

namespace BML::Behavior::Blocks {
namespace Text2D {

struct Options {
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
};

namespace Detail {
template <class Definition>
void Define(Definition &block, const Options &options) {
    block.Target(CKPGUID_2DENTITY, options.Target);
    block.Pin(0, CKPGUID_FONT, options.FontIndex);
    block.Pin(1, CKPGUID_STRING, std::string_view(options.Text));
    block.Pin(2, CKPGUID_ALIGNMENT, options.Alignment);
    block.Pin(3, CKPGUID_RECT, options.Margin);
    block.Pin(4, CKPGUID_2DVECTOR, options.Offset);
    block.Pin(5, CKPGUID_2DVECTOR, options.ParagraphIndentation);
    block.ObjectPin(6, CKPGUID_MATERIAL, options.BackgroundMaterial);
    block.Pin(7, CKPGUID_PERCENTAGE, options.CaretSize);
    block.ObjectPin(8, CKPGUID_MATERIAL, options.CaretMaterial);
    block.Setting(0, CKPGUID_TEXTPROPERTIES, options.Flags);
}
} // namespace Detail

#ifdef BML_BEHAVIOR_INTERNAL
inline BlockSpec Make(const Options &options) {
    Blocks::Detail::Definition block(VT_INTERFACE_2DTEXT);
    Detail::Define(block, options);
    return std::move(block).Build();
}
#else
inline Result<Block> Make(const Session &session, const Options &options) {
    BML::Behavior::Detail::Definition block(session, VT_INTERFACE_2DTEXT);
    Detail::Define(block, options);
    return std::move(block).Build();
}
#endif


} // namespace Text2D
} // namespace BML::Behavior::Blocks

#endif // BML_BEHAVIOR_BLOCKS_TEXT2D_HPP
