#include "BML/Gui/Label.h"

#include "BML/ScriptHelper.h"
#include "Loader/ModContext.h"
#include "Virtools/BallanceBehaviorPresets.h"

using namespace BGui;

Label::Label(const char *name) : Element(name) {
    ModContext *context = BML_GetModContext();
    BML::Virtools::Presets::Text2DOptions definition;
    definition.Target = m_2dEntity;
    definition.FontIndex = context->GetGameFonts().Resolve(BML::GameFont::None);
    BML::Virtools::GraphBlockResult created = context->GetBehaviorRuntime().AddToGraph(
        context->GetScriptByName("Level_Init"), BML::Virtools::Presets::Text2D(definition));
    m_Text2d = created ? created.Behavior : nullptr;
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
