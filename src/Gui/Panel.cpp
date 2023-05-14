#include "Gui/Panel.h"

#include "ModLoader.h"

using namespace BGui;

Panel::Panel(const char *name) : Element(name) {
    m_Material = (CKMaterial *)ModLoader::GetInstance().GetCKContext()
        ->CreateObject(CKCID_MATERIAL, TOCKSTRING((std::string(name) + "_Mat").c_str()));
    ModLoader::GetInstance().GetCKContext()->GetCurrentLevel()->AddObject(m_Material);
    m_Material->EnableAlphaBlend();
    m_Material->SetSourceBlend(VXBLEND_SRCALPHA);
    m_Material->SetDestBlend(VXBLEND_INVSRCALPHA);
    m_2dEntity->SetMaterial(m_Material);
    m_2dEntity->SetZOrder(0);
}

Panel::~Panel() {
    CKContext *context = ModLoader::GetInstance().GetCKContext();
    if (context)
        context->DestroyObject(CKOBJID(m_Material));
}

VxColor Panel::GetColor() {
    return m_Material->GetDiffuse();
}

void Panel::SetColor(VxColor color) {
    m_Material->SetDiffuse(color);
}