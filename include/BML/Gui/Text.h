// Text drawn by a CKSpriteText rather than by the game's 2D Text block, which is the way
// to put a line of a Mod's own on screen in a font the game does not ship. The font is
// picked once for the whole loader, from Microsoft YaHei UI or Microsoft YaHei if either
// is installed, and its size follows the window height, so Gui::Process refreshes it
// through UpdateFont when the resolution changes. SetFont takes a font of the Mod's
// choosing instead, by the name the system knows it under.
//
// Unlike Label this is not styled after the game's menu at all: colour and alignment are
// the sprite's own, and the sprite is marked not to be saved with the level.
//
// It inherits Element privately, so a Text is not a BGui::Element to a Mod and the
// position, size, Z order, and visibility functions are the ones declared here.
#ifndef BML_GUI_TEXT_H
#define BML_GUI_TEXT_H

#include "BML/Defines.h"

#include "BML/Gui/Element.h"

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
        CKSpriteText *m_Sprite;
    };
}

#endif // BML_GUI_TEXT_H
