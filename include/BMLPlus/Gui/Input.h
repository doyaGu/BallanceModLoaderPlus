#ifndef BML_GUI_INPUT_H
#define BML_GUI_INPUT_H

#include <functional>
#include <string>

#include "Defines.h"

#include "Gui/Label.h"

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
