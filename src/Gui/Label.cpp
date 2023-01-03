#include "Gui/Label.h"

#include "ModLoader.h"
#include "ScriptHelper.h"

namespace ExecuteBB {
    int GetFont(FontType type);
    FontType GetFontType(int font);
}

using namespace BGui;

Label::Label(const char *name) : Element(name) {
    m_Text2d = ExecuteBB::Create2DText(ModLoader::GetInstance().GetScriptByName("Level_Init"), m_2dEntity);
}

Label::~Label() {
    if (!ModLoader::GetInstance().IsReset()) {
        CKContext *context = ModLoader::GetInstance().GetCKContext();
        if (context)
            context->DestroyObject(CKOBJID(m_Text2d));
    }
}

const char *Label::GetText() {
    return ScriptHelper::GetParamString(m_Text2d->GetInputParameter(1)->GetRealSource());
}

void Label::SetText(const char *text) {
    ScriptHelper::SetParamString(m_Text2d->GetInputParameter(1)->GetRealSource(), text);
}

ExecuteBB::FontType Label::GetFont() {
    return ExecuteBB::GetFontType(
        ScriptHelper::GetParamValue<int>(m_Text2d->GetInputParameter(0)->GetRealSource()));
}

void Label::SetFont(ExecuteBB::FontType font) {
    ScriptHelper::SetParamValue(m_Text2d->GetInputParameter(0)->GetRealSource(), ExecuteBB::GetFont(font));
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