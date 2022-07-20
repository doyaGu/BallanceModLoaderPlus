#ifndef BML_GUI_TEXT_H
#define BML_GUI_TEXT_H

#include "Defines.h"

#include "Gui/Element.h"

namespace BGui {
    class BML_EXPORT Text : private Element {
        friend class Gui;

    public:
        explicit Text(const char *name);
        ~Text() override;

        Vx2DVector GetPosition() override;
        void SetPosition(Vx2DVector pos) override;

        Vx2DVector GetSize() override;
        void SetSize(Vx2DVector size) override;

        int GetZOrder() override;
        void SetZOrder(int z) override;

        bool IsVisible() override;
        void SetVisible(bool visible) override;

        const char *GetText();
        void SetText(const char *text);

        void SetFont(const char *FontName, int FontSize, int Weight, CKBOOL italic, CKBOOL underline);

        void SetAlignment(CKSPRITETEXT_ALIGNMENT align);

        CKDWORD GetTextColor();
        void SetTextColor(CKDWORD color);

        void UpdateFont();

    protected:
        CKSpriteText *m_sprite;
    };
}

#endif // BML_GUI_TEXT_H
