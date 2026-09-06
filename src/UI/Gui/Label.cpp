#include "BML/Gui/Label.h"

#include "Loader/ModContext.h"
#include "Behavior/Text2DView.h"

using namespace BGui;

namespace {
// Every function below writes or reads one Slot of the same Block, so each one
// opens a view on it and lets the Text2D module say which Slot that is.
BML::Behavior::Internal::Text2DView::View Text(CKBehavior *block) {
    return {BML_GetCKContext(), block};
}
} // namespace

Label::Label(const char *name) : Element(name) {
    ModContext *context = BML_GetModContext();
    m_Text2d = BML::Behavior::Internal::Text2DView::Add(
        context->Behaviors(), context->GetScriptByName("Level_Init"),
        {m_2dEntity, context->GetGameFonts().Resolve(BML::GameFont::None)});
}

Label::~Label() {
    CKContext *context = BML_GetCKContext();
    if (context && m_Text2d)
        context->DestroyObject(CKOBJID(m_Text2d));
}

const char *Label::GetText() {
    return Text(m_Text2d).Text();
}

void Label::SetText(const char *text) {
    Text(m_Text2d).SetText(text);
}

ExecuteBB::FontType Label::GetFont() {
    const BML::GameFontCatalog &fonts = BML_GetModContext()->GetGameFonts();
    return static_cast<ExecuteBB::FontType>(
        fonts.Identify(Text(m_Text2d).Font()));
}

void Label::SetFont(ExecuteBB::FontType font) {
    const BML::GameFontCatalog &fonts = BML_GetModContext()->GetGameFonts();
    Text(m_Text2d).SetFont(fonts.Resolve(static_cast<BML::GameFont>(font)));
}

void Label::SetAlignment(int align) {
    Text(m_Text2d).SetAlignment(align);
}

int Label::GetTextFlags() {
    return Text(m_Text2d).Flags();
}

void Label::SetTextFlags(int flags) {
    Text(m_Text2d).SetFlags(flags);
}

void Label::SetOffset(Vx2DVector offset) {
    Text(m_Text2d).SetOffset(offset);
}

void Label::Process() {
    Text(m_Text2d).Draw();
}
