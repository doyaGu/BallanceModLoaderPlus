#include "Gui/KeyInput.h"

#include "ModLoader.h"
#include "InputHook.h"

using namespace BGui;

CKMaterial *m_highlight = nullptr;

KeyInput::KeyInput(const char *name) : Input(name), m_key() {
    m_2dentity->UseSourceRect();
    VxRect rect(0.005f, 0.3804f, 0.4353f, 0.4549f);
    m_2dentity->SetSourceRect(rect);
}

void KeyInput::OnCharTyped(CKDWORD key) {
    SetKey(static_cast<CKKEYBOARD>(key));
    InvokeCallback(key);
}

CKKEYBOARD KeyInput::GetKey() {
    return m_key;
}

void KeyInput::SetKey(CKKEYBOARD key) {
    m_key = key;
    char name[0x100];
    ModLoader::GetInstance().GetInputManager()->GetKeyName(key, name);
    SetText(name);
}

void KeyInput::GetFocus() {
    m_2dentity->SetMaterial(m_highlight);
}

void KeyInput::LoseFocus() {
    m_2dentity->SetMaterial(nullptr);
}