// One of the game's own menu buttons, which is a Label drawing a piece of the menu's button
// texture behind its text. The type picks which piece: SetType sets the source rectangle,
// the size, and the font all together, so it undoes a SetSize made before it and has to be
// called first. The arrow and plus/minus types carry no text.
//
// The three materials are the game's, M_Button_Up, M_Button_Over, and M_Button_Inactive,
// which Gui::InitMaterials fetches when the menu loads, so a button made before that draws
// with no texture at all.
//
// SetActive(false) only swaps in the inactive material. It does not stop anything: a click
// inside the rectangle still runs the callback, so a button that should do nothing while
// inactive checks IsActive itself, at the top of its own callback. SetType puts the plain
// material back and so undoes the inactive look as well.
//
// A callback is not optional. Gui::Process calls it on a click inside the rectangle, and on
// Escape for whichever button was added as the back one, without checking whether one was
// set, so a Button::SetCallback that never happened is a crash rather than a button that
// does nothing.
//
// OnMouseEnter and OnMouseLeave are the hover look and Gui::Process drives them once a
// frame, leaving every button and then entering the one under the mouse. Nothing calls them
// from the engine, and BUTTON_SMALL is the one type that lights up even while inactive,
// which is how the Yes/No pair still answers the mouse on the side that is not chosen.
#ifndef BML_GUI_BUTTON_H
#define BML_GUI_BUTTON_H

#include <functional>

#include "BML/Defines.h"

#include "BML/Gui/Label.h"

namespace BGui {
    enum ButtonType {
        BUTTON_NORMAL,
        BUTTON_BACK,
        BUTTON_SETTING,
        BUTTON_LEVEL,
        BUTTON_KEY,
        BUTTON_KEYSEL,
        BUTTON_SMALL,
        BUTTON_LEFT,
        BUTTON_RIGHT,
        BUTTON_PLUS,
        BUTTON_MINUS,
    };

    class BML_EXPORT Button : public Label {
        friend class Gui;

    public:
        explicit Button(const char *name);

        ButtonType GetType();
        void SetType(ButtonType type);

        bool IsActive();
        void SetActive(bool active);

        void InvokeCallback();
        void SetCallback(std::function<void()> callback);

        void OnMouseEnter();
        void OnMouseLeave();

    protected:
        ButtonType m_Type;
        bool m_Active = true;
        std::function<void()> m_Callback;
    };
}

#endif // BML_GUI_BUTTON_H
