// A field holding one key rather than a line of text, for a Mod letting the player rebind
// something. It shows the key's name as the input manager spells it, and SetKey is also how
// the current binding is put in before the player sees it.
//
// While it has focus the next key pressed becomes the binding, whatever key it is. There is
// no filtering and no way to back out: Escape and Return are taken as bindings like anything
// else, and the editing keys of Input do not apply here since every key ends up in SetKey.
// A Mod that wants to refuse a key calls SetKey again with the old one from its callback,
// which is where the new key arrives.
//
// Focus is shown by swapping in the menu's key highlight material, and losing focus clears
// the material rather than putting one back, so this element draws only its text the rest of
// the time. Gui::AddKeyButton is the usual way to get one, and it puts a plain BUTTON_KEY
// behind the pair to carry the label and the field's background; that button gets a callback
// that does nothing, so clicking the label only plays the menu click.
#ifndef BML_GUI_KEYINPUT_H
#define BML_GUI_KEYINPUT_H

#include <functional>

#include "BML/Defines.h"

#include "BML/Gui/Input.h"

namespace BGui {
    class BML_EXPORT KeyInput : public Input {
        friend class Gui;
    public:
        explicit KeyInput(const char *name);

        void OnCharTyped(CKDWORD key) override;

        CKKEYBOARD GetKey();
        void SetKey(CKKEYBOARD key);

        void GetFocus() override;
        void LoseFocus() override;

    protected:
        CKKEYBOARD m_Key;
        std::function<void()> m_KeyCallback;
    };
}

#endif // BML_GUI_KEYINPUT_H
