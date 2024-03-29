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
