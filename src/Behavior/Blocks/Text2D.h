#ifndef BML_BEHAVIOR_TEXT2D_H
#define BML_BEHAVIOR_TEXT2D_H

#include <string>

#include "Behavior/Runtime.h"

namespace BML::Behavior::Text2D {

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

Spec Make(const Options &options);

// Adds a 2D Text Block to a live graph and hands back the Block itself, which
// is what a caller that keeps drawing the text holds on to.
CKBehavior *Add(Runtime &runtime, CKBehavior *graph, const Options &options);

// The Slots of a live 2D Text Block, addressed by role. Which parameter of the
// retail prototype carries the text is this module's business, so a caller that
// already holds the Block writes what it means instead of counting parameters.
// The view owns nothing and keeps no state, so it is built for one statement and
// dropped; the Block stays the caller's.
class View final {
public:
    View(CKContext *context, CKBehavior *block) noexcept
        : m_Context(context), m_Block(block) {}

    [[nodiscard]] explicit operator bool() const noexcept {
        return m_Context != nullptr && m_Block != nullptr;
    }

    Status SetFont(int index);
    Status SetText(const char *text);
    Status SetAlignment(int alignment);
    Status SetOffset(const Vx2DVector &offset);
    Status SetCaretMaterial(CKMaterial *material);
    // The text flags are a Setting. The retail Block reads them while it draws
    // rather than rebuilding its layout from them, so this writes the Setting
    // in place and runs no settings stage.
    Status SetFlags(int flags);

    [[nodiscard]] int Font() const;
    [[nodiscard]] const char *Text() const;
    [[nodiscard]] int Flags() const;

    // Activates the Block's In and executes it. The graph that holds a parked
    // 2D Text Block never activates it, so drawing is the caller's to do.
    void Draw();

private:
    [[nodiscard]] CKParameter *Find(const Slot &slot) const;
    Status Write(const Slot &slot, const Parameter::Binding &value);

    CKContext *m_Context = nullptr;
    CKBehavior *m_Block = nullptr;
};

} // namespace BML::Behavior::Text2D

#endif // BML_BEHAVIOR_TEXT2D_H
