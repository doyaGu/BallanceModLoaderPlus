// Text in one of the game's own fonts, drawn the way the game draws its menu text: the
// element owns a 2D Text building block added to the game's Level_Init script, and every
// function here writes a parameter of that block. Process executes it, which is what
// actually puts the text on screen, so a label only shows while Gui::Process keeps being
// called.
//
// That block is also why a Label cannot be built before the game's Level_Init script
// exists. The fonts are the game's, chosen from BML::GameFont, and the flags from
// SetTextFlags are the block's own, BML_GUI_TEXT_SCREEN and the rest declared in BML/Gui.h.
// SetOffset shifts the text inside the element, which is how the text on a button is
// moved off the left edge.
#ifndef BML_UI_GUI_HOST_LABEL_H
#define BML_UI_GUI_HOST_LABEL_H

#include "BML/Defines.h"
#include "BML/Gui.h"
#include "UI/GameFontCatalog.h"

#include "UI/Gui/Element.h"

namespace BML::Api {
class GuiApi;
}

namespace BGuiHost {
class Label : public Element {
    friend class Gui;
    friend class BML::Api::GuiApi;

  public:
    explicit Label(const char *name);
    ~Label() override;

    virtual const char *GetText();
    virtual void SetText(const char *text);

    BML::GameFont GetFont();
    void SetFont(BML::GameFont font);

    void SetAlignment(int align);

    int GetTextFlags();
    void SetTextFlags(int flags);

    void SetOffset(Vx2DVector offset);

    void Process() override;

  protected:
    CKBehavior *m_Text2d = nullptr;
};
} // namespace BGuiHost

#endif // BML_UI_GUI_HOST_LABEL_H
