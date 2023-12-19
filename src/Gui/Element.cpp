#include "BML/Gui/Element.h"

#include "ModLoader.h"

using namespace BGui;

Element::Element(const char *name) {
    m_2dEntity = (CK2dEntity *) ModLoader::GetInstance().GetCKContext()
        ->CreateObject(CKCID_2DENTITY, TOCKSTRING(name));
    ModLoader::GetInstance().GetCKContext()->GetCurrentLevel()->AddObject(m_2dEntity);
    m_2dEntity->SetHomogeneousCoordinates();
    m_2dEntity->EnableClipToCamera(false);
    m_2dEntity->EnableRatioOffset(false);
    m_2dEntity->SetZOrder(20);
}

Element::~Element() {
    CKContext *context = ModLoader::GetInstance().GetCKContext();
    if (context)
        context->DestroyObject(CKOBJID(m_2dEntity));
}

Vx2DVector Element::GetPosition() {
    Vx2DVector res;
    m_2dEntity->GetPosition(res, true);
    return res;
}

void Element::SetPosition(Vx2DVector pos) {
    m_2dEntity->SetPosition(pos, true);
}

Vx2DVector Element::GetSize() {
    Vx2DVector res;
    m_2dEntity->GetSize(res, true);
    return res;
}

void Element::SetSize(Vx2DVector size) {
    m_2dEntity->SetSize(size, true);
}

int Element::GetZOrder() {
    return m_2dEntity->GetZOrder();
}

void Element::SetZOrder(int z) {
    m_2dEntity->SetZOrder(z);
}

bool Element::IsVisible() {
    return m_2dEntity->IsVisible();
}

void Element::SetVisible(bool visible) {
    m_2dEntity->Show(visible ? CKSHOW : CKHIDE);
}