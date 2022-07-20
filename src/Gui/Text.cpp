#include "Gui/Text.h"

#include "ModLoader.h"

using namespace BGui;

const char *g_text_font = nullptr;
const char *g_avail_fonts[] = {"Microsoft YaHei UI", "Microsoft YaHei"};

Text::Text(const char *name) : Element(name) {
    CKContext *ctx = ModLoader::GetInstance().GetCKContext();
    m_sprite = (CKSpriteText *)ctx->CreateObject(CKCID_SPRITETEXT, TOCKSTRING(name));
    m_sprite->ModifyObjectFlags(CK_OBJECT_NOTTOBELISTEDANDSAVED, 0);
    ctx->GetCurrentLevel()->AddObject(m_sprite);
    m_sprite->SetHomogeneousCoordinates();
    m_sprite->EnableClipToCamera(false);
    m_sprite->EnableRatioOffset(false);
    m_sprite->SetZOrder(20);
    m_sprite->SetTextColor(0xffffffff);
    m_sprite->SetAlign(CKSPRITETEXT_ALIGNMENT(CKSPRITETEXT_VCENTER | CKSPRITETEXT_LEFT));
    m_sprite->SetFont(TOCKSTRING(g_text_font), ctx->GetPlayerRenderContext()->GetHeight() / 85, 400);
}

Text::~Text() {
    ModLoader::GetInstance().GetCKContext()->DestroyObject(CKOBJID(m_sprite));
}

void Text::UpdateFont() {
    CKContext *ctx = ModLoader::GetInstance().GetCKContext();
    m_sprite->SetFont(TOCKSTRING(g_text_font), ctx->GetPlayerRenderContext()->GetHeight() / 85, 400);
}

Vx2DVector Text::GetPosition() {
    Vx2DVector res;
    m_sprite->GetPosition(res, true);
    return res;
}

void Text::SetPosition(Vx2DVector pos) {
    m_sprite->SetPosition(pos, true);
}

Vx2DVector Text::GetSize() {
    Vx2DVector res;
    m_sprite->GetSize(res, true);
    return res;
}

void Text::SetSize(Vx2DVector size) {
    m_sprite->ReleaseAllSlots();
    m_sprite->Create(int(ModLoader::GetInstance().GetRenderContext()->GetWidth() * size.x),
                     int(ModLoader::GetInstance().GetRenderContext()->GetHeight() * size.y), 32);
    m_sprite->SetSize(size, true);
}

int Text::GetZOrder() {
    return m_sprite->GetZOrder();
}

void Text::SetZOrder(int z) {
    m_sprite->SetZOrder(z);
}

bool Text::IsVisible() {
    return m_sprite->IsVisible();
}

void Text::SetVisible(bool visible) {
    m_sprite->Show(visible ? CKSHOW : CKHIDE);
}

const char *Text::GetText() {
    return m_sprite->GetText();
}

void Text::SetText(const char *text) {
    m_sprite->SetText(TOCKSTRING(text));
}

void Text::SetFont(const char *FontName, int FontSize, int Weight, CKBOOL italic, CKBOOL underline) {
    m_sprite->SetFont(TOCKSTRING(FontName), FontSize, Weight, italic, underline);
}

void Text::SetAlignment(CKSPRITETEXT_ALIGNMENT align) {
    m_sprite->SetAlign(align);
}

CKDWORD Text::GetTextColor() {
    return m_sprite->GetTextColor();
}

void Text::SetTextColor(CKDWORD color) {
    m_sprite->SetTextColor(color);
}