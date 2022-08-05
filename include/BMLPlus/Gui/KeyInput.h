#ifndef BML_GUI_KEYINPUT_H
#define BML_GUI_KEYINPUT_H

#include <functional>

#include "Defines.h"

#include "Gui/Input.h"

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
