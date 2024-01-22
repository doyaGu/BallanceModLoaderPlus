#include "BML/Gui/Button.h"

using namespace BGui;

CKMaterial *g_Up = nullptr;
CKMaterial *g_Over = nullptr;
CKMaterial *g_Inactive = nullptr;

Button::Button(const char *name) : Label(name), m_Type(BUTTON_NORMAL) {
    m_2dEntity->UseSourceRect();
}

ButtonType Button::GetType() {
    return m_Type;
}

void Button::SetType(ButtonType type) {
    m_Type = type;
    m_2dEntity->SetMaterial(g_Up);

    VxRect rect;
    switch (type) {
        case BUTTON_NORMAL:
            SetFont(ExecuteBB::GAMEFONT_01);
            SetSize(Vx2DVector(0.3f, 0.0938f));
            rect = VxRect(0.0f, 0.51372f, 1.0f, 0.7451f);
            m_2dEntity->SetSourceRect(rect);
            break;
        case BUTTON_BACK:
            SetFont(ExecuteBB::GAMEFONT_01);
            SetSize(Vx2DVector(0.1875f, 0.0938f));
            rect = VxRect(0.2392f, 0.75294f, 0.8666f, 0.98431f);
            m_2dEntity->SetSourceRect(rect);
            break;
        case BUTTON_SETTING:
            SetFont(ExecuteBB::GAMEFONT_01);
            SetSize(Vx2DVector(0.3f, 0.1f));
            rect = VxRect(0.0f, 0.0f, 1.0f, 0.24706f);
            m_2dEntity->SetSourceRect(rect);
            break;
        case BUTTON_LEVEL:
            SetFont(ExecuteBB::GAMEFONT_03);
            SetSize(Vx2DVector(0.1938f, 0.05f));
            rect = VxRect(0.0f, 0.247f, 0.647f, 0.36863f);
            m_2dEntity->SetSourceRect(rect);
            break;
        case BUTTON_KEY:
            SetFont(ExecuteBB::GAMEFONT_03);
            SetSize(Vx2DVector(0.3f, 0.0396f));
            rect = VxRect(0.0f, 0.40785f, 1.0f, 0.51f);
            m_2dEntity->SetSourceRect(rect);
            break;
        case BUTTON_KEYSEL:
            SetFont(ExecuteBB::GAMEFONT_03);
            SetSize(Vx2DVector(0.1450f, 0.0396f));
            rect = VxRect(0.005f, 0.3804f, 0.4353f, 0.4549f);
            m_2dEntity->SetSourceRect(rect);
            break;
        case BUTTON_SMALL:
            SetFont(ExecuteBB::GAMEFONT_03);
            SetSize(Vx2DVector(0.0700f, 0.0354f));
            rect = VxRect(0.0f, 0.82353f, 0.226f, 0.9098f);
            m_2dEntity->SetSourceRect(rect);
            break;
        case BUTTON_LEFT:
            SetSize(Vx2DVector(0.0363f, 0.0517f));
            rect = VxRect(0.6392f, 0.24706f, 0.78823f, 0.40392f);
            m_2dEntity->SetSourceRect(rect);
            break;
        case BUTTON_RIGHT:
            SetSize(Vx2DVector(0.0363f, 0.0517f));
            rect = VxRect(0.7921f, 0.24706f, 0.9412f, 0.40392f);
            m_2dEntity->SetSourceRect(rect);
            break;
        case BUTTON_PLUS:
            SetSize(Vx2DVector(0.0200f, 0.0267f));
            rect = VxRect(0.88627f, 0.8902f, 0.96863f, 0.97255f);
            m_2dEntity->SetSourceRect(rect);
            break;
        case BUTTON_MINUS:
            SetSize(Vx2DVector(0.0200f, 0.0267f));
            rect = VxRect(0.88627f, 0.77804f, 0.96863f, 0.8594f);
            m_2dEntity->SetSourceRect(rect);
            break;
    }
}

bool Button::IsActive() {
    return m_Active;
}

void Button::SetActive(bool active) {
    m_Active = active;
    m_2dEntity->SetMaterial(active ? g_Up : g_Inactive);
}

void Button::InvokeCallback() {
    m_Callback();
}

void Button::SetCallback(std::function<void()> callback) {
    m_Callback = std::move(callback);
}

void Button::OnMouseEnter() {
    if (m_Active || m_Type == BUTTON_SMALL)
        m_2dEntity->SetMaterial(g_Over);
}

void Button::OnMouseLeave() {
    if (m_Active || m_Type == BUTTON_SMALL)
        m_2dEntity->SetMaterial(m_Active ? g_Up : g_Inactive);
}
