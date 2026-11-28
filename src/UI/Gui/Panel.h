// A rectangle of flat colour, for the background a screen is drawn over. It creates a
// material of its own next to the entity, alpha blended, so the alpha of the colour is
// the transparency, and sits at Z order 0 to stay behind the other elements.
#ifndef BML_UI_GUI_HOST_PANEL_H
#define BML_UI_GUI_HOST_PANEL_H

#include "BML/Defines.h"

#include "UI/Gui/Element.h"

namespace BML::Api {
class GuiApi;
}

namespace BGuiHost {
class Panel : public Element {
    friend class Gui;
    friend class BML::Api::GuiApi;

  public:
    explicit Panel(const char *name);
    ~Panel() override;

    VxColor GetColor();
    void SetColor(VxColor color);

  protected:
    CKMaterial *m_Material = nullptr;
};
} // namespace BGuiHost

#endif // BML_UI_GUI_HOST_PANEL_H
