#include "BML/Gui/Text.h"

#include "Loader/ModContext.h"
#include "UI/Gui/LegacyTextFont.h"

using namespace BGui;

Text::Text(const char *name) : Element(name) {
    m_Sprite = (CKSpriteText *)m_Context->CreateObject(CKCID_SPRITETEXT, (CKSTRING) name);
    m_Sprite->ModifyObjectFlags(CK_OBJECT_NOTTOBELISTEDANDSAVED, 0);
    m_Context->GetCurrentLevel()->AddObject(m_Sprite);
    m_Sprite->SetHomogeneousCoordinates();
    m_Sprite->EnableClipToCamera(false);
    m_Sprite->EnableRatioOffset(false);
    m_Sprite->SetZOrder(20);
    m_Sprite->SetTextColor(0xffffffff);
    m_Sprite->SetAlign(CKSPRITETEXT_ALIGNMENT(CKSPRITETEXT_VCENTER | CKSPRITETEXT_LEFT));
    InitializeLegacyTextFont(m_Sprite, m_Context->GetPlayerRenderContext()->GetHeight());
}

Text::~Text() {
    ForgetLegacyTextFont(m_Sprite);
    if (m_Context && m_Sprite)
        m_Context->DestroyObject(CKOBJID(m_Sprite));
}

void Text::UpdateFont() {
    if (m_Context && m_Context->GetPlayerRenderContext()) {
        RefreshLegacyTextFont(m_Sprite, m_Context->GetPlayerRenderContext()->GetHeight());
    }
}

Vx2DVector Text::GetPosition() {
    Vx2DVector res;
    m_Sprite->GetPosition(res, true);
    return res;
}

void Text::SetPosition(Vx2DVector pos) {
    m_Sprite->SetPosition(pos, true);
}

Vx2DVector Text::GetSize() {
    Vx2DVector res;
    m_Sprite->GetSize(res, true);
    return res;
}

void Text::SetSize(Vx2DVector size) {
    m_Sprite->ReleaseAllSlots();
    auto *rc = m_Context->GetPlayerRenderContext();
    m_Sprite->Create((int)(rc->GetWidth() * size.x), (int)(rc->GetHeight() * size.y), 32);
    m_Sprite->SetSize(size, true);
}

int Text::GetZOrder() {
    return m_Sprite->GetZOrder();
}

void Text::SetZOrder(int z) {
    m_Sprite->SetZOrder(z);
}

bool Text::IsVisible() {
    return m_Sprite->IsVisible();
}

void Text::SetVisible(bool visible) {
    m_Sprite->Show(visible ? CKSHOW : CKHIDE);
}

const char *Text::GetText() {
    return m_Sprite->GetText();
}

void Text::SetText(const char *text) {
    m_Sprite->SetText((CKSTRING) text);
}

void Text::SetFont(const char *FontName, int FontSize, int Weight, CKBOOL italic, CKBOOL underline) {
    SetLegacyTextFont(m_Sprite, FontName, FontSize, Weight, italic != FALSE, underline != FALSE);
}

void Text::SetAlignment(CKSPRITETEXT_ALIGNMENT align) {
    m_Sprite->SetAlign(align);
}

CKDWORD Text::GetTextColor() {
    return m_Sprite->GetTextColor();
}

void Text::SetTextColor(CKDWORD color) {
    m_Sprite->SetTextColor(color);
}
