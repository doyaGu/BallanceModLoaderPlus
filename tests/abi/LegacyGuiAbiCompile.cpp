#include "BML/Gui.h"

#include <cstddef>
#include <functional>
#include <string>

namespace {

// The published 0.3.13 storage layout, independent of the current headers.
// Use the member types rather than byte counts so both CRT configurations
// check the same ABI without assuming std::string/std::function sizes.
struct LegacyElement {
    virtual ~LegacyElement() = default;
    CK2dEntity *Entity;
};

struct LegacyLabel : LegacyElement {
    CKBehavior *Text;
};

struct LegacyPanel : LegacyElement {
    CKMaterial *Material;
};

class LegacyText : private LegacyElement {
public:
    CKSpriteText *Sprite;
};

struct LegacyButton : LegacyLabel {
    BGui::ButtonType Type;
    bool Active;
    std::function<void()> Callback;
};

struct LegacyInput : LegacyLabel {
    std::string Text;
    unsigned int Caret;
    std::function<void(CKDWORD)> Callback;
};

struct LegacyKeyInput : LegacyInput {
    CKKEYBOARD Key;
    std::function<void()> KeyCallback;
};

class ElementFields : public BGui::Element {
public:
    using Element::m_2dEntity;
};

class LabelFields : public BGui::Label {
public:
    using Label::m_Text2d;
};

class PanelFields : public BGui::Panel {
public:
    using Panel::m_Material;
};

class TextFields : public BGui::Text {
public:
    using Text::m_Sprite;
};

class ButtonFields : public BGui::Button {
public:
    using Button::m_Type;
    using Button::m_Active;
    using Button::m_Callback;
};

class InputFields : public BGui::Input {
public:
    using Input::m_Text;
    using Input::m_Caret;
    using Input::m_Callback;
};

class KeyInputFields : public BGui::KeyInput {
public:
    using KeyInput::m_Key;
    using KeyInput::m_KeyCallback;
};

#define CHECK_LAYOUT(Type, Legacy) \
    static_assert(sizeof(BGui::Type) == sizeof(Legacy), #Type " size changed"); \
    static_assert(alignof(BGui::Type) == alignof(Legacy), #Type " alignment changed")

#define CHECK_MEMBER(Type, Member, Legacy, Field) \
    static_assert(offsetof(Type, Member) == offsetof(Legacy, Field), #Type "::" #Member " offset changed")

CHECK_LAYOUT(Element, LegacyElement);
CHECK_LAYOUT(Label, LegacyLabel);
CHECK_LAYOUT(Panel, LegacyPanel);
CHECK_LAYOUT(Text, LegacyText);
CHECK_LAYOUT(Button, LegacyButton);
CHECK_LAYOUT(Input, LegacyInput);
CHECK_LAYOUT(KeyInput, LegacyKeyInput);

CHECK_MEMBER(ElementFields, m_2dEntity, LegacyElement, Entity);
CHECK_MEMBER(LabelFields, m_Text2d, LegacyLabel, Text);
CHECK_MEMBER(PanelFields, m_Material, LegacyPanel, Material);
CHECK_MEMBER(TextFields, m_Sprite, LegacyText, Sprite);
CHECK_MEMBER(ButtonFields, m_Type, LegacyButton, Type);
CHECK_MEMBER(ButtonFields, m_Active, LegacyButton, Active);
CHECK_MEMBER(ButtonFields, m_Callback, LegacyButton, Callback);
CHECK_MEMBER(InputFields, m_Text, LegacyInput, Text);
CHECK_MEMBER(InputFields, m_Caret, LegacyInput, Caret);
CHECK_MEMBER(InputFields, m_Callback, LegacyInput, Callback);
CHECK_MEMBER(KeyInputFields, m_Key, LegacyKeyInput, Key);
CHECK_MEMBER(KeyInputFields, m_KeyCallback, LegacyKeyInput, KeyCallback);

#undef CHECK_MEMBER
#undef CHECK_LAYOUT

} // namespace
