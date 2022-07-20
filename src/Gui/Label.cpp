#include "Gui/Label.h"

#include "ModLoader.h"
#include "ScriptHelper.h"

using namespace BGui;

Label::Label(const char *name) : Element(name) {
    m_text2d = ExecuteBB::Create2DText(m_2dentity);
}

Label::~Label() {
    ModLoader::GetInstance().GetCKContext()->DestroyObject(CKOBJID(m_text2d));
}

const char *Label::GetText() {
    return ScriptHelper::GetParamString(m_text2d->GetInputParameter(1)->GetRealSource());
}

void Label::SetText(const char *text) {
    ScriptHelper::SetParamString(m_text2d->GetInputParameter(1)->GetRealSource(), text);
}

ExecuteBB::FontType Label::GetFont() {
    return ExecuteBB::GetFontType(
        ScriptHelper::GetParamValue<int>(m_text2d->GetInputParameter(0)->GetRealSource()));
}

void Label::SetFont(ExecuteBB::FontType font) {
    ScriptHelper::SetParamValue(m_text2d->GetInputParameter(0)->GetRealSource(), ExecuteBB::GetFont(font));
}

void Label::SetAlignment(int align) {
    ScriptHelper::SetParamValue(m_text2d->GetInputParameter(2)->GetRealSource(), align);
}

int Label::GetTextFlags() {
    return ScriptHelper::GetParamValue<int>(m_text2d->GetLocalParameter(0));
}

void Label::SetTextFlags(int flags) {
    ScriptHelper::SetParamValue(m_text2d->GetLocalParameter(0), flags);
}

void Label::SetOffset(Vx2DVector offset) {
    ScriptHelper::SetParamValue(m_text2d->GetInputParameter(4)->GetRealSource(), offset);
}

void Label::Process() {
    m_text2d->ActivateInput(0);
    m_text2d->Execute(0);
}