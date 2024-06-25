#include "BML/Gui/KeyInput.h"

#include "BML/InputHook.h"
#include "ModManager.h"

using namespace BGui;

CKMaterial *g_Highlight = nullptr;

KeyInput::KeyInput(const char *name) : Input(name), m_Key() {
    m_2dEntity->UseSourceRect();
    VxRect rect(0.005f, 0.3804f, 0.4353f, 0.4549f);
    m_2dEntity->SetSourceRect(rect);
}

void KeyInput::OnCharTyped(CKDWORD key) {
    SetKey(static_cast<CKKEYBOARD>(key));
    InvokeCallback(key);
}

CKKEYBOARD KeyInput::GetKey() {
    return m_Key;
}

void KeyInput::SetKey(CKKEYBOARD key) {
    m_Key = key;
    char name[0x100];
    BML_GetModManager()->GetInputManager()->GetKeyName(key, name);
    SetText(name);
}

void KeyInput::GetFocus() {
    m_2dEntity->SetMaterial(g_Highlight);
}

void KeyInput::LoseFocus() {
    m_2dEntity->SetMaterial(nullptr);
}