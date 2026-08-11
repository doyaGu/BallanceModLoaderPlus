// Text in one of the game's own fonts, drawn the way the game draws its menu text: the
// element owns a 2D Text building block added to the game's Level_Init script, and every
// function here writes a parameter of that block. Process executes it, which is what
// actually puts the text on screen, so a label only shows while Gui::Process keeps being
// called.
//
// That block is also why a Label cannot be built before the game's Level_Init script
// exists. The fonts are the game's, chosen from ExecuteBB::FontType, and the flags from
// SetTextFlags are the block's own, TEXT_SCREEN and the rest declared in ExecuteBB.h.
// SetOffset shifts the text inside the element, which is how the text on a button is
// moved off the left edge.
#ifndef BML_GUI_LABEL_H
#define BML_GUI_LABEL_H

#include "BML/Defines.h"
#include "BML/ExecuteBB.h"

#include "BML/Gui/Element.h"

namespace BGui {
    class BML_EXPORT Label : public Element {
        friend class Gui;

    public:
        explicit Label(const char *name);
        ~Label() override;

        virtual const char *GetText();
        virtual void SetText(const char *text);

        ExecuteBB::FontType GetFont();
        void SetFont(ExecuteBB::FontType font);

        void SetAlignment(int align);

        int GetTextFlags();
        void SetTextFlags(int flags);

        void SetOffset(Vx2DVector offset);

        void Process() override;

    protected:
        CKBehavior *m_Text2d;
    };
}

#endif // BML_GUI_LABEL_H
