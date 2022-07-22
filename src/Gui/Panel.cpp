#include "Gui/Panel.h"

#include "ModLoader.h"

using namespace BGui;

Panel::Panel(const char *name) : Element(name) {
    m_material = (CKMaterial *)ModLoader::GetInstance().GetCKContext()
        ->CreateObject(CKCID_MATERIAL, TOCKSTRING((std::string(name) + "_Mat").c_str()));
    ModLoader::GetInstance().GetCKContext()->GetCurrentLevel()->AddObject(m_material);
    m_material->EnableAlphaBlend();
    m_material->SetSourceBlend(VXBLEND_SRCALPHA);
    m_material->SetDestBlend(VXBLEND_INVSRCALPHA);
    m_2dentity->SetMaterial(m_material);
    m_2dentity->SetZOrder(0);
}

Panel::~Panel() {
    if (!ModLoader::GetInstance().IsReset()) {
        CKContext *context = ModLoader::GetInstance().GetCKContext();
        if (context)
            context->DestroyObject(CKOBJID(m_material));
    }
}

VxColor Panel::GetColor() {
    return m_material->GetDiffuse();
}

void Panel::SetColor(VxColor color) {
    m_material->SetDiffuse(color);
}