// A rectangle of flat colour, for the background a screen is drawn over. It creates a
// material of its own next to the entity, alpha blended, so the alpha of the colour is
// the transparency, and sits at Z order 0 to stay behind the other elements.
#ifndef BML_GUI_PANEL_H
#define BML_GUI_PANEL_H

#include "BML/Defines.h"

#include "BML/Gui/Element.h"

namespace BGui {
    class BML_EXPORT Panel : public Element {
        friend class Gui;

    public:
        explicit Panel(const char *name);
        ~Panel() override;

        VxColor GetColor();
        void SetColor(VxColor color);

    protected:
        CKMaterial *m_Material;
    };
}

#endif // BML_GUI_PANEL_H
