#ifndef BML_GUI_GUI_H
#define BML_GUI_GUI_H

#include <functional>
#include <vector>

#include "Defines.h"
#include "ExecuteBB.h"

#include "Gui/Element.h"
#include "Gui/Text.h"
#include "Gui/Panel.h"
#include "Gui/Label.h"
#include "Gui/Button.h"
#include "Gui/Input.h"
#include "Gui/KeyInput.h"

namespace BGui {
    class BML_EXPORT Gui {
    public:
        Gui();

        Button *AddNormalButton(const char *name, const char *text, float yPos, float xPos = 0.35f,
                                std::function<void()> callback = []() {});
        Button *AddBackButton(const char *name, const char *text = "Back", float yPos = 0.85f, float xPos = 0.4031f,
                              std::function<void()> callback = []() {});
        Button *AddSettingButton(const char *name, const char *text, float yPos, float xPos = 0.35f,
                                 std::function<void()> callback = []() {});
        Button *AddLevelButton(const char *name, const char *text, float yPos, float xPos = 0.4031f,
                               std::function<void()> callback = []() {});
        Button *AddSmallButton(const char *name, const char *text, float yPos, float xPos,
                               std::function<void()> callback = []() {});
        Button *AddLeftButton(const char *name, float yPos, float xPos, std::function<void()> callback = []() {});
        Button *AddRightButton(const char *name, float yPos, float xPos, std::function<void()> callback = []() {});
        Button *AddPlusButton(const char *name, float yPos, float xPos, std::function<void()> callback = []() {});
        Button *AddMinusButton(const char *name, float yPos, float xPos, std::function<void()> callback = []() {});
        Button *AddKeyBgButton(const char *name, float yPos, float xPos);

        Panel *AddPanel(const char *name, VxColor color, float xPos, float yPos, float xSize, float ySize);

        Label *AddTextLabel(const char *name, const char *text, ExecuteBB::FontType font, float xPos, float yPos,
                            float xSize, float ySize);
        Text *AddText(const char *name, const char *text, float xPos, float yPos, float xSize, float ySize);
        Input *AddTextInput(const char *name, ExecuteBB::FontType font, float xPos, float yPos, float xSize, float ySize,
                            std::function<void(CKDWORD)> callback = [](CKDWORD) {});

        std::pair<Button *, KeyInput *> AddKeyButton(const char *name, const char *text, float yPos, float xPos = 0.35f,
                                                     std::function<void(CKDWORD)> callback = [](CKDWORD) {});
        std::pair<Button *, Button *> AddYesNoButton(const char *name, float yPos, float x1Pos, float x2Pos,
                                                     std::function<void(bool)> callback = [](bool) {});

        virtual void OnCharTyped(CKDWORD key);
        virtual void OnMouseDown(float x, float y, CK_MOUSEBUTTON key);
        virtual void OnMouseWheel(float w);
        virtual void OnMouseMove(float x, float y, float lx, float ly);
        virtual void OnScreenModeChanged();

        virtual void Process();

        virtual void SetVisible(bool visible);

        bool CanBeBlocked();
        void SetCanBeBlocked(bool block);

        void SetFocus(Input *input);

        bool Intersect(float x, float y, Element *element);

        static void InitMaterials();

    private:
        std::vector<Element *> m_Elements;
        std::vector<Button *> m_Buttons;
        std::vector<Input *> m_Inputs;
        std::vector<Text *> m_Texts;
        Input *m_Focus = nullptr;
        Button *m_Back = nullptr;
        bool m_Block = true;
        int m_Width = 0;
        int m_Height = 0;
        Vx2DVector m_OldMousePos;
    };
}

#endif // BML_GUI_GUI_H
