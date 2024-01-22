#include "BML/Gui/Text.h"

#include "ModManager.h"

using namespace BGui;

const char *g_TextFont = nullptr;
const char *g_AvailFonts[] = {"Microsoft YaHei UI", "Microsoft YaHei"};

Text::Text(const char *name) : Element(name) {
    CKContext *context = BML_GetCKContext();
    m_Sprite = (CKSpriteText *)context->CreateObject(CKCID_SPRITETEXT, (CKSTRING) name);
    m_Sprite->ModifyObjectFlags(CK_OBJECT_NOTTOBELISTEDANDSAVED, 0);
    context->GetCurrentLevel()->AddObject(m_Sprite);
    m_Sprite->SetHomogeneousCoordinates();
    m_Sprite->EnableClipToCamera(false);
    m_Sprite->EnableRatioOffset(false);
    m_Sprite->SetZOrder(20);
    m_Sprite->SetTextColor(0xffffffff);
    m_Sprite->SetAlign(CKSPRITETEXT_ALIGNMENT(CKSPRITETEXT_VCENTER | CKSPRITETEXT_LEFT));
    m_Sprite->SetFont((CKSTRING) g_TextFont, context->GetPlayerRenderContext()->GetHeight() / 85, 400);
}

Text::~Text() {
    CKContext *context = BML_GetCKContext();
    if (context)
        context->DestroyObject(CKOBJID(m_Sprite));
}

void Text::UpdateFont() {
    CKContext *context = BML_GetCKContext();
    m_Sprite->SetFont((CKSTRING) g_TextFont, context->GetPlayerRenderContext()->GetHeight() / 85, 400);
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
    auto *rc = BML_GetRenderContext();
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
    m_Sprite->SetFont((CKSTRING) FontName, FontSize, Weight, italic, underline);
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