#ifndef BML_GUI_BUTTON_H
#define BML_GUI_BUTTON_H

#include <functional>

#include "Defines.h"

#include "Gui/Label.h"

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
