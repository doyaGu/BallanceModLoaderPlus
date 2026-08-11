// One screen's worth of BGui elements and the input that drives them. See BML/Gui.h for
// what BGui is and when to use it instead of Bui.
//
// The Add functions build an element, put it on the screen at a position given in
// fractions of the window, 0 to 1, and hand back a pointer the Gui keeps owning: the
// destructor deletes every element it made, so hold those pointers no longer than the
// Gui itself and do not delete one. The name goes to the CK object, and nothing
// requires it to be unique. AddYesNoButton and AddKeyButton each build two elements and
// return both.
//
// Process is the whole of the machinery and a Mod calls it once a frame while its screen
// is up. It re-executes each element's text block, notices a change of resolution and
// runs OnScreenModeChanged, reads the keyboard buffer and the mouse out of InputHook,
// and from that calls OnCharTyped, OnMouseDown, OnMouseMove, and OnMouseWheel, which is
// where a button's callback is run from. Those five are virtual so a derived screen can
// take them over; call the base if the elements should still react.
//
// SetCanBeBlocked decides which read Process uses. On, which is the default, it reads
// through the loader's input blocks, so this screen goes quiet while something else has
// the keyboard, InputHook::AcquireBlock included. Off, it reads the raw device and sees
// keys the game and the loader are trying to keep to themselves.
//
// Escape is wired to whatever AddBackButton made, so a screen with a back button closes
// on Escape through that button's own callback. A click that landed on a button or an
// input also sends the game's Menu_Click sound.
//
// SetVisible hides or shows every element at once, which is how a screen goes away
// without being destroyed. The elements stay in the level either way.
#ifndef BML_GUI_GUI_H
#define BML_GUI_GUI_H

#include <functional>
#include <vector>

#include "CKDefines.h"

#include "BML/Defines.h"
#include "BML/ExecuteBB.h"

#include "BML/Gui/Element.h"
#include "BML/Gui/Text.h"
#include "BML/Gui/Panel.h"
#include "BML/Gui/Label.h"
#include "BML/Gui/Button.h"
#include "BML/Gui/Input.h"
#include "BML/Gui/KeyInput.h"

namespace BGui {
    class BML_EXPORT Gui {
    public:
        Gui();
        ~Gui();

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
