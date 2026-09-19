#include "BML/Gui/Panel.h"

#include "Loader/ModContext.h"

using namespace BGui;

Panel::Panel(const char *name) : Element(name) {
    m_Material = (CKMaterial *)m_Context->CreateObject(CKCID_MATERIAL, (CKSTRING) ((std::string(name) + "_Mat").c_str()));
    m_Context->GetCurrentLevel()->AddObject(m_Material);
    m_Material->EnableAlphaBlend();
    m_Material->SetSourceBlend(VXBLEND_SRCALPHA);
    m_Material->SetDestBlend(VXBLEND_INVSRCALPHA);
    m_2dEntity->SetMaterial(m_Material);
    m_2dEntity->SetZOrder(0);
}

Panel::~Panel() {
    if (m_Context && m_Material)
        m_Context->DestroyObject(CKOBJID(m_Material));
}

VxColor Panel::GetColor() {
    return m_Material->GetDiffuse();
}

void Panel::SetColor(VxColor color) {
    m_Material->SetDiffuse(color);
}
