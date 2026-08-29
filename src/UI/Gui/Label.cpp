#include "BML/Gui/Label.h"

#include "BML/ScriptHelper.h"
#include "Loader/ModContext.h"
#include "Virtools/BehaviorGraphRecipes.h"

using namespace BGui;

Label::Label(const char *name) : Element(name) {
    ModContext *context = BML_GetModContext();
    BML::BehaviorGraphRecipes::Text2DDefinition definition;
    definition.Target = m_2dEntity;
    definition.FontIndex = context->GetGameFonts().Resolve(BML::GameFont::None);
    m_Text2d = BML::BehaviorGraphRecipes::Add2DText(context->GetScriptByName("Level_Init"), definition);
}

Label::~Label() {
    CKContext *context = BML_GetCKContext();
    if (context && m_Text2d)
        context->DestroyObject(CKOBJID(m_Text2d));
}

const char *Label::GetText() {
    return ScriptHelper::GetParamString(m_Text2d->GetInputParameter(1)->GetRealSource());
}

void Label::SetText(const char *text) {
    ScriptHelper::SetParamString(m_Text2d->GetInputParameter(1)->GetRealSource(), text);
}

ExecuteBB::FontType Label::GetFont() {
    ModContext *context = BML_GetModContext();
    const int font = ScriptHelper::GetParamValue<int>(m_Text2d->GetInputParameter(0)->GetRealSource());
    return static_cast<ExecuteBB::FontType>(context->GetGameFonts().Identify(font));
}

void Label::SetFont(ExecuteBB::FontType font) {
    ModContext *context = BML_GetModContext();
    const BML::GameFont gameFont = static_cast<BML::GameFont>(font);
    ScriptHelper::SetParamValue(m_Text2d->GetInputParameter(0)->GetRealSource(),
                                context->GetGameFonts().Resolve(gameFont));
}

void Label::SetAlignment(int align) {
    ScriptHelper::SetParamValue(m_Text2d->GetInputParameter(2)->GetRealSource(), align);
}

int Label::GetTextFlags() {
    return ScriptHelper::GetParamValue<int>(m_Text2d->GetLocalParameter(0));
}

void Label::SetTextFlags(int flags) {
    ScriptHelper::SetParamValue(m_Text2d->GetLocalParameter(0), flags);
}

void Label::SetOffset(Vx2DVector offset) {
    ScriptHelper::SetParamValue(m_Text2d->GetInputParameter(4)->GetRealSource(), offset);
}

void Label::Process() {
    m_Text2d->ActivateInput(0);
    m_Text2d->Execute(0);
}
