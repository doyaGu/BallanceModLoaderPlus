#include "BML/Gui/Input.h"

#include "BML/InputHook.h"
#include "BML/ScriptHelper.h"
#include "ModManager.h"

using namespace BGui;

CKMaterial *g_Caret = nullptr;

Input::Input(const char *name) : Label(name) {
    m_2dEntity->UseSourceRect();
    ScriptHelper::SetParamObject(m_Text2d->GetInputParameter(8)->GetRealSource(), ::g_Caret);
    ScriptHelper::SetParamString(m_Text2d->GetInputParameter(1)->GetRealSource(), "\b");
}

void Input::InvokeCallback(CKDWORD key) {
    m_Callback(key);
}

void Input::SetCallback(std::function<void(CKDWORD)> callback) {
    m_Callback = std::move(callback);
}

void Input::OnCharTyped(CKDWORD key) {
    bool changed = false;

    switch (key) {
        case CKKEY_BACK:
            if (m_Caret > 0) {
                m_Text.erase(--m_Caret);
                changed = true;
            }
            break;
        case CKKEY_DELETE:
            if (m_Caret < m_Text.size()) {
                m_Text.erase(m_Caret);
                changed = true;
            }
            break;
        case CKKEY_LEFT:
            if (m_Caret > 0) {
                m_Caret--;
                changed = true;
            }
            break;
        case CKKEY_RIGHT:
            if (m_Caret < m_Text.size()) {
                m_Caret++;
                changed = true;
            }
            break;
        case CKKEY_HOME:
            if (m_Caret > 0) {
                m_Caret = 0;
                changed = true;
            }
            break;
        case CKKEY_END:
            if (m_Caret < m_Text.size()) {
                m_Caret = m_Text.size();
                changed = true;
            }
            break;
        case CKKEY_ESCAPE:
        case CKKEY_TAB:
        case CKKEY_RETURN:
        case CKKEY_UP:
        case CKKEY_DOWN:
            InvokeCallback(key);
            break;
        default:
            char c = VxScanCodeToAscii(key, BML_GetInputHook()->GetKeyboardState());
            if (c) {
                m_Text.insert(m_Caret++, 1, c);
                changed = true;
            }
    }

    if (changed) {
        InvokeCallback(key);
        std::string str = m_Text;
        str.insert(m_Caret, 1, '\b');
        ScriptHelper::SetParamString(m_Text2d->GetInputParameter(1)->GetRealSource(), str.c_str());
    }
}

const char *Input::GetText() {
    return m_Text.c_str();
}

void Input::SetText(const char *text) {
    m_Text = text;
    m_Caret = m_Text.size();
    ScriptHelper::SetParamString(m_Text2d->GetInputParameter(1)->GetRealSource(), (m_Text + '\b').c_str());
}

void Input::GetFocus() {
    SetTextFlags(GetTextFlags() | TEXT_SHOWCARET);
}

void Input::LoseFocus() {
    SetTextFlags(GetTextFlags() & ~TEXT_SHOWCARET);
}
