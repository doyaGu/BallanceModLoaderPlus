#include "Gui/Element.h"

#include "ModLoader.h"

using namespace BGui;

Element::Element(const char *name) {
    m_2dentity = (CK2dEntity *) ModLoader::GetInstance().GetCKContext()
        ->CreateObject(CKCID_2DENTITY, TOCKSTRING(name));
    ModLoader::GetInstance().GetCKContext()->GetCurrentLevel()->AddObject(m_2dentity);
    m_2dentity->SetHomogeneousCoordinates();
    m_2dentity->EnableClipToCamera(false);
    m_2dentity->EnableRatioOffset(false);
    m_2dentity->SetZOrder(20);
}

Element::~Element() {
    ModLoader::GetInstance().GetCKContext()->DestroyObject(CKOBJID(m_2dentity));
}

Vx2DVector Element::GetPosition() {
    Vx2DVector res;
    m_2dentity->GetPosition(res, true);
    return res;
}

void Element::SetPosition(Vx2DVector pos) {
    m_2dentity->SetPosition(pos, true);
}

Vx2DVector Element::GetSize() {
    Vx2DVector res;
    m_2dentity->GetSize(res, true);
    return res;
}

void Element::SetSize(Vx2DVector size) {
    m_2dentity->SetSize(size, true);
}

int Element::GetZOrder() {
    return m_2dentity->GetZOrder();
}

void Element::SetZOrder(int z) {
    m_2dentity->SetZOrder(z);
}

bool Element::IsVisible() {
    return m_2dentity->IsVisible();
}

void Element::SetVisible(bool visible) {
    m_2dentity->Show(visible ? CKSHOW : CKHIDE);
}