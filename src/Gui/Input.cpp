#include "Gui/Input.h"

#include "ModLoader.h"
#include "InputHook.h"
#include "ScriptHelper.h"

using namespace BGui;

CKMaterial *m_caret = nullptr;

Input::Input(const char *name) : Label(name) {
    m_2dentity->UseSourceRect();
    ScriptHelper::SetParamObject(m_text2d->GetInputParameter(8)->GetRealSource(), ::m_caret);
    ScriptHelper::SetParamString(m_text2d->GetInputParameter(1)->GetRealSource(), "\b");
}

void Input::InvokeCallback(CKDWORD key) {
    m_callback(key);
}

void Input::SetCallback(std::function<void(CKDWORD)> callback) {
    m_callback = std::move(callback);
}

void Input::OnCharTyped(CKDWORD key) {
    bool changed = false;

    switch (key) {
        case CKKEY_BACK:
            if (m_caret > 0) {
                m_text.erase(--m_caret);
                changed = true;
            }
            break;
        case CKKEY_DELETE:
            if (m_caret < m_text.size()) {
                m_text.erase(m_caret);
                changed = true;
            }
            break;
        case CKKEY_LEFT:
            if (m_caret > 0) {
                m_caret--;
                changed = true;
            }
            break;
        case CKKEY_RIGHT:
            if (m_caret < m_text.size()) {
                m_caret++;
                changed = true;
            }
            break;
        case CKKEY_HOME:
            if (m_caret > 0) {
                m_caret = 0;
                changed = true;
            }
            break;
        case CKKEY_END:
            if (m_caret < m_text.size()) {
                m_caret = m_text.size();
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
            char c = VxScanCodeToAscii(key, ModLoader::GetInstance().GetInputManager()->GetKeyboardState());
            if (c) {
                m_text.insert(m_caret++, 1, c);
                changed = true;
            }
    }

    if (changed) {
        InvokeCallback(key);
        std::string str = m_text;
        str.insert(m_caret, 1, '\b');
        ScriptHelper::SetParamString(m_text2d->GetInputParameter(1)->GetRealSource(), str.c_str());
    }
}

const char *Input::GetText() {
    return m_text.c_str();
}

void Input::SetText(const char *text) {
    m_text = text;
    m_caret = m_text.size();
    ScriptHelper::SetParamString(m_text2d->GetInputParameter(1)->GetRealSource(), (m_text + '\b').c_str());
}

void Input::GetFocus() {
    SetTextFlags(GetTextFlags() | TEXT_SHOWCARET);
}

void Input::LoseFocus() {
    SetTextFlags(GetTextFlags() & ~TEXT_SHOWCARET);
}
