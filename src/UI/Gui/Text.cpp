#include "UI/Gui/Text.h"

#include "CKContext.h"
#include "CKLevel.h"
#include "CKRenderContext.h"
#include "CKSpriteText.h"

#include "UI/Gui/LegacyTextFont.h"
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace BGuiHost;

Text::Text(const char *name) : Element(name) {
    CKContext *context = m_2dEntity->GetCKContext();
    m_Sprite = (CKSpriteText *)context->CreateObject(CKCID_SPRITETEXT, (CKSTRING) name);
    if (!m_Sprite) throw std::bad_alloc();
    m_Sprite->ModifyObjectFlags(CK_OBJECT_NOTTOBELISTEDANDSAVED, 0);
    context->GetCurrentLevel()->AddObject(m_Sprite);
    m_Sprite->SetHomogeneousCoordinates();
    m_Sprite->EnableClipToCamera(false);
    m_Sprite->EnableRatioOffset(false);
    m_Sprite->SetZOrder(20);
    m_Sprite->SetTextColor(0xffffffff);
    m_Sprite->SetAlign(CKSPRITETEXT_ALIGNMENT(CKSPRITETEXT_VCENTER | CKSPRITETEXT_LEFT));
    try {
        auto *render = context->GetPlayerRenderContext();
        InitializeLegacyTextFont(m_Sprite, render ? render->GetHeight() : 0);
    } catch (...) {
        ForgetLegacyTextFont(m_Sprite);
        context->DestroyObject(m_Sprite);
        m_Sprite = nullptr;
        throw;
    }
}

Text::~Text() {
    ForgetLegacyTextFont(m_Sprite);
    if (m_Sprite)
        m_Sprite->GetCKContext()->DestroyObject(CKOBJID(m_Sprite));
}

void Text::UpdateFont() {
    CKRenderContext *render = m_Sprite ? m_Sprite->GetCKContext()->GetPlayerRenderContext() : nullptr;
    if (render)
        RefreshLegacyTextFont(m_Sprite, render->GetHeight());
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
    auto *rc = m_Sprite->GetCKContext()->GetPlayerRenderContext();
    if (!rc) throw std::runtime_error("GUI text requires a render context");
    const double width = static_cast<double>(rc->GetWidth()) * size.x;
    const double height = static_cast<double>(rc->GetHeight()) * size.y;
    if (!std::isfinite(width) || !std::isfinite(height) || width < 0 || height < 0 ||
        width > std::numeric_limits<int>::max() || height > std::numeric_limits<int>::max())
        throw std::invalid_argument("GUI text size exceeds the sprite dimensions");
    if (!m_Sprite->Create(std::max(1, static_cast<int>(width)), std::max(1, static_cast<int>(height)), 32))
        throw std::runtime_error("Failed to resize GUI text sprite");
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
