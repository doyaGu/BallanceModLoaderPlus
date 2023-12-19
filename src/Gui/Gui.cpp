#include "BML/Gui/Gui.h"

#include <utility>

#include "BML/InputHook.h"
#include "ModLoader.h"

using namespace BGui;

extern const char *g_TextFont;
extern const char *g_AvailFonts[2];
extern CKMaterial *g_Up;
extern CKMaterial *g_Over;
extern CKMaterial *g_Inactive;
extern CKMaterial *g_Caret;
extern CKMaterial *g_Highlight;

CKMaterial *g_Field = nullptr;
CKGroup *g_AllSound = nullptr;

Gui::Gui() {
    CKRenderContext *rc = ModLoader::GetInstance().GetRenderContext();
    m_Width = rc->GetWidth();
    m_Height = rc->GetHeight();
}

void Gui::OnCharTyped(CKDWORD key) {
    if (key == CKKEY_ESCAPE)
        if (m_Back)
            m_Back->InvokeCallback();
    if (m_Focus && m_Focus->IsVisible())
        m_Focus->OnCharTyped(key);
}

void Gui::OnMouseDown(float x, float y, CK_MOUSEBUTTON key) {
    if (key == CK_MOUSEBUTTON_LEFT) {
        bool success = false;
        for (Button *button: m_Buttons) {
            if (Intersect(x, y, button)) {
                button->InvokeCallback();
                success = true;
            }
        }
        if (m_Buttons.empty() && m_Inputs.size() == 1)
            SetFocus(m_Inputs[0]);
        else {
            SetFocus(nullptr);
            for (Input *input: m_Inputs) {
                if (Intersect(x, y, input)) {
                    SetFocus(input);
                    success = true;
                }
            }
        }

        if (success) {
            CKMessageManager *mm = ModLoader::GetInstance().GetMessageManager();
            CKMessageType msg = mm->AddMessageType("Menu_Click");
            mm->SendMessageSingle(msg, g_AllSound);
        }
    }
}

void Gui::OnMouseWheel(float w) {}

void Gui::OnMouseMove(float x, float y, float lx, float ly) {}

bool Gui::Intersect(float x, float y, Element *element) {
    Vx2DVector pos = element->GetPosition(), size = element->GetSize();
    return element->IsVisible() && x >= pos.x && x <= pos.x + size.x && y >= pos.y && y <= pos.y + size.y;
}

Button *Gui::AddNormalButton(const char *name, const char *text, float yPos, float xPos,
                             std::function<void()> callback) {
    auto *button = new Button(name);
    button->SetText(text);
    button->SetPosition(Vx2DVector(xPos, yPos));
    button->SetCallback(std::move(callback));
    button->SetType(BUTTON_NORMAL);
    m_Elements.push_back(button);
    m_Buttons.push_back(button);
    return button;
}

Button *Gui::AddBackButton(const char *name, const char *text, float yPos, float xPos,
                           std::function<void()> callback) {
    auto *button = new Button(name);
    button->SetText(text);
    button->SetPosition(Vx2DVector(xPos, yPos));
    button->SetCallback(std::move(callback));
    button->SetType(BUTTON_BACK);
    m_Elements.push_back(button);
    m_Buttons.push_back(button);
    m_Back = button;
    return button;
}

Button *Gui::AddSettingButton(const char *name, const char *text, float yPos, float xPos,
                              std::function<void()> callback) {
    auto *button = new Button(name);
    button->SetText(text);
    button->SetPosition(Vx2DVector(xPos, yPos));
    button->SetCallback(std::move(callback));
    button->SetType(BUTTON_SETTING);
    m_Elements.push_back(button);
    m_Buttons.push_back(button);
    return button;
}

Button *Gui::AddLevelButton(const char *name, const char *text, float yPos, float xPos,
                            std::function<void()> callback) {
    auto *button = new Button(name);
    button->SetText(text);
    button->SetPosition(Vx2DVector(xPos, yPos));
    button->SetCallback(std::move(callback));
    button->SetType(BUTTON_LEVEL);
    m_Elements.push_back(button);
    m_Buttons.push_back(button);
    return button;
}

Button *Gui::AddSmallButton(const char *name, const char *text, float yPos, float xPos,
                            std::function<void()> callback) {
    auto *button = new Button(name);
    button->SetText(text);
    button->SetPosition(Vx2DVector(xPos, yPos));
    button->SetCallback(std::move(callback));
    button->SetType(BUTTON_SMALL);
    m_Elements.push_back(button);
    m_Buttons.push_back(button);
    return button;
}

Button *Gui::AddLeftButton(const char *name, float yPos, float xPos, std::function<void()> callback) {
    auto *button = new Button(name);
    button->SetPosition(Vx2DVector(xPos, yPos));
    button->SetCallback(std::move(callback));
    button->SetType(BUTTON_LEFT);
    m_Elements.push_back(button);
    m_Buttons.push_back(button);
    return button;
}

Button *Gui::AddRightButton(const char *name, float yPos, float xPos, std::function<void()> callback) {
    auto *button = new Button(name);
    button->SetPosition(Vx2DVector(xPos, yPos));
    button->SetCallback(std::move(callback));
    button->SetType(BUTTON_RIGHT);
    m_Elements.push_back(button);
    m_Buttons.push_back(button);
    return button;
}

Button *Gui::AddPlusButton(const char *name, float yPos, float xPos, std::function<void()> callback) {
    auto *button = new Button(name);
    button->SetPosition(Vx2DVector(xPos, yPos));
    button->SetCallback(std::move(callback));
    button->SetType(BUTTON_PLUS);
    m_Elements.push_back(button);
    m_Buttons.push_back(button);
    return button;
}

Button *Gui::AddMinusButton(const char *name, float yPos, float xPos, std::function<void()> callback) {
    auto *button = new Button(name);
    button->SetPosition(Vx2DVector(xPos, yPos));
    button->SetCallback(std::move(callback));
    button->SetType(BUTTON_MINUS);
    m_Elements.push_back(button);
    m_Buttons.push_back(button);
    return button;
}

Button *Gui::AddKeyBgButton(const char *name, float yPos, float xPos) {
    auto *button = new Button(name);
    button->SetPosition(Vx2DVector(xPos, yPos));
    button->SetType(BUTTON_KEY);
    m_Elements.push_back(button);
    m_Buttons.push_back(button);
    return button;
}

Panel *Gui::AddPanel(const char *name, VxColor color, float xPos, float yPos, float xSize, float ySize) {
    auto *panel = new Panel(name);
    panel->SetColor(color);
    panel->SetPosition(Vx2DVector(xPos, yPos));
    panel->SetSize(Vx2DVector(xSize, ySize));
    m_Elements.push_back(panel);
    return panel;
}

Label *Gui::AddTextLabel(const char *name, const char *text, ExecuteBB::FontType font, float xPos, float yPos,
                         float xSize, float ySize) {
    auto *label = new Label(name);
    label->SetText(text);
    label->SetFont(font);
    label->SetPosition(Vx2DVector(xPos, yPos));
    label->SetSize(Vx2DVector(xSize, ySize));
    m_Elements.push_back(label);
    return label;
}

Text *Gui::AddText(const char *name, const char *text, float xPos, float yPos, float xSize, float ySize) {
    auto *txt = new Text(name);
    txt->SetText(text);
    txt->SetPosition(Vx2DVector(xPos, yPos));
    txt->SetSize(Vx2DVector(xSize, ySize));
    m_Elements.push_back(txt);
    m_Texts.push_back(txt);
    return txt;
}

Input *Gui::AddTextInput(const char *name, ExecuteBB::FontType font, float xPos, float yPos,
                         float xSize, float ySize, std::function<void(CKDWORD)> callback) {
    auto *input = new Input(name);
    input->SetFont(font);
    input->SetPosition(Vx2DVector(xPos, yPos));
    input->SetSize(Vx2DVector(xSize, ySize));
    input->SetCallback(std::move(callback));
    m_Elements.push_back(input);
    m_Inputs.push_back(input);
    if (!m_Focus)
        SetFocus(input);
    return input;
}

std::pair<Button *, KeyInput *> Gui::AddKeyButton(const char *name, const char *text, float yPos, float xPos,
                                                  std::function<void(CKDWORD)> callback) {
    Button *bg = AddKeyBgButton(name, yPos, xPos);
    bg->SetText(text);
    bg->SetAlignment(ALIGN_LEFT);
    bg->SetZOrder(15);
    bg->SetOffset(Vx2DVector(ModLoader::GetInstance().GetRenderContext()->GetWidth() * 0.03f, 0.0f));
    bg->SetCallback([]() {});
    m_Elements.push_back(bg);

    auto *button = new KeyInput(name);
    button->SetFont(ExecuteBB::GAMEFONT_03);
    button->SetPosition(Vx2DVector(xPos + 0.155f, yPos));
    button->SetSize(Vx2DVector(0.1450f, 0.0396f));
    button->SetCallback(std::move(callback));
    button->SetZOrder(25);
    m_Elements.push_back(button);
    m_Inputs.push_back(button);
    return {bg, button};
}

std::pair<Button *, Button *> Gui::AddYesNoButton(const char *name, float yPos, float x1Pos, float x2Pos, std::function<void(bool)> callback) {
    Button *yes = AddSmallButton(name, "Yes", yPos, x1Pos);
    Button *no = AddSmallButton(name, "No", yPos, x2Pos);
    yes->SetCallback([callback, yes, no]() {
        callback(true);
        yes->SetActive(true);
        no->SetActive(false);
    });
    no->SetCallback([callback, yes, no]() {
        callback(false);
        yes->SetActive(false);
        no->SetActive(true);
    });
    m_Elements.push_back(yes);
    m_Elements.push_back(no);
    return {yes, no};
}

void Gui::Process() {
    CKRenderContext *rc = ModLoader::GetInstance().GetRenderContext();
    if (rc->GetWidth() != m_Width || rc->GetHeight() != m_Height) {
        m_Width = rc->GetWidth();
        m_Height = rc->GetHeight();
        OnScreenModeChanged();
    }

    for (Element *element: m_Elements)
        element->Process();

    InputHook *input = ModLoader::GetInstance().GetInputManager();
    int cnt = (m_Block ? input->GetNumberOfKeyInBuffer() : input->oGetNumberOfKeyInBuffer());
    for (int i = 0; i < cnt; i++) {
        CKDWORD key;
        if ((m_Block ? input->GetKeyFromBuffer(i, key) : input->oGetKeyFromBuffer(i, key)) == KEY_PRESSED)
            OnCharTyped(key);
    }

    Vx2DVector mousePos, lastPos;
    input->GetMousePosition(mousePos, false);
    for (CK_MOUSEBUTTON button = CK_MOUSEBUTTON_LEFT; button < CK_MOUSEBUTTON_4;
         (*reinterpret_cast<int *>(&button))++) {
        if (m_Block ? input->IsMouseClicked(button) : input->oIsMouseClicked(button))
            OnMouseDown(mousePos.x / rc->GetWidth(), mousePos.y / rc->GetHeight(), button);
    }

    for (Button *button: m_Buttons)
        button->OnMouseLeave();
    for (Button *button: m_Buttons)
        if (Intersect(mousePos.x / rc->GetWidth(), mousePos.y / rc->GetHeight(), button))
            button->OnMouseEnter();

    VxVector relPos;
    input->GetMouseRelativePosition(relPos);
    input->GetLastMousePosition(lastPos);
    if (relPos.x != 0 && relPos.y != 0)
        OnMouseMove(mousePos.x / rc->GetWidth(), mousePos.y / rc->GetHeight(), lastPos.x / rc->GetWidth(),
                    lastPos.y / rc->GetHeight());
    if (relPos.z != 0)
        OnMouseWheel(relPos.z);
}

void Gui::SetVisible(bool visible) {
    for (Element *element: m_Elements)
        element->SetVisible(visible);
}

bool Gui::CanBeBlocked() {
    return m_Block;
}

void Gui::SetCanBeBlocked(bool block) {
    m_Block = block;
}

void Gui::SetFocus(Input *input) {
    if (m_Focus)
        m_Focus->LoseFocus();
    m_Focus = input;
    if (input)
        input->GetFocus();
}

void Gui::InitMaterials() {
    g_Up = ModLoader::GetInstance().GetMaterialByName("M_Button_Up");
    g_Inactive = ModLoader::GetInstance().GetMaterialByName("M_Button_Inactive");
    g_Over = ModLoader::GetInstance().GetMaterialByName("M_Button_Over");
    g_Field = ModLoader::GetInstance().GetMaterialByName("M_EntryBG");
    g_Caret = ModLoader::GetInstance().GetMaterialByName("M_Caret");
    g_Highlight = ModLoader::GetInstance().GetMaterialByName("M_Keys_Highlight");
    g_AllSound = ModLoader::GetInstance().GetGroupByName("All_Sound");

    CKParameterManager *pm = ModLoader::GetInstance().GetParameterManager();
    CKEnumStruct *data = pm->GetEnumDescByType(pm->ParameterGuidToType(CKPGUID_FONTNAME));
    for (const char *avail_font: g_AvailFonts) {
        for (int i = 0; i < data->GetNumEnums(); i++) {
            const char *fontName = data->GetEnumDescription(i);
            if (!strcmp(fontName, avail_font)) {
                g_TextFont = avail_font;
                break;
            }
        }
        if (g_TextFont)
            break;
    }
    if (!g_TextFont)
        g_TextFont = "";
}

void Gui::OnScreenModeChanged() {
    for (Text *txt: m_Texts)
        txt->UpdateFont();
}