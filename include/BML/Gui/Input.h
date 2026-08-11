// A single-line text field in the game's own style. The text lives here and goes to the
// underlying 2D Text block with a backspace character standing where the caret is, which is
// how the game's field draws a caret, so the string this holds and the string the block is
// given are not quite the same and GetText is the one to read.
//
// One field in a Gui has focus and only that one is typed into. Gui::SetFocus moves it, the
// first field added takes it, a click puts it on the field clicked, and a Gui with no
// buttons and exactly one field keeps it there whatever is clicked. Focus shows and hides
// the caret, which is all GetFocus and LoseFocus do here.
//
// OnCharTyped is the whole editor: Backspace and Delete either side of the caret, Left,
// Right, Home, and End to move it, and anything else translated through the current keyboard
// state, so the layout and the shift keys apply. That translation gives one ASCII byte, so
// what an IME would produce never arrives, and a key it cannot translate is dropped.
//
// Escape, Tab, Return, Up, and Down are not text and are not inserted. They go to the
// callback and nowhere else, which is where a Mod decides that Return means accept or that
// Tab moves on. The callback also runs after every edit, with the scan code of the key that
// caused it, so it fires per keystroke rather than once at the end, and it is called without
// checking that one was set. SetText replaces the text and leaves the caret at the end.
#ifndef BML_GUI_INPUT_H
#define BML_GUI_INPUT_H

#include <functional>
#include <string>

#include "BML/Defines.h"

#include "BML/Gui/Label.h"

namespace BGui {
    class BML_EXPORT Input : public Label {
        friend class Gui;
    public:
        explicit Input(const char *name);

        void InvokeCallback(CKDWORD);
        void SetCallback(std::function<void(CKDWORD)> callback);

        virtual void OnCharTyped(CKDWORD key);

        const char *GetText() override;
        void SetText(const char *text) override;

        virtual void GetFocus();
        virtual void LoseFocus();

    protected:
        std::string m_Text;
        unsigned int m_Caret = 0;
        std::function<void(CKDWORD)> m_Callback;
    };
}

#endif // BML_GUI_INPUT_H
