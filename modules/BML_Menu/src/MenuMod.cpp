#define BML_LOADER_IMPLEMENTATION
#define BML_MENU_PROVIDER_IMPLEMENTATION
#include "bml_module.hpp"

#include "bml_menu.hpp"
#include "bml_interface.h"

namespace {

bool ServiceInitTextures(CKContext *context) {
    return Menu::InitTextures(context);
}

bool ServiceInitMaterials(CKContext *context) {
    return Menu::InitMaterials(context);
}

bool ServiceInitSounds(CKContext *context) {
    return Menu::InitSounds(context);
}

CKTexture *ServiceLoadTexture(CKContext *context, const char *id, const char *filename, int slot) {
    return Menu::LoadTexture(context, id, filename, slot);
}

void ServicePlayMenuClickSound() {
    Menu::PlayMenuClickSound();
}

ImVec2 ServiceGetMenuPos() {
    return Menu::GetMenuPos();
}

ImVec2 ServiceGetMenuSize() {
    return Menu::GetMenuSize();
}

ImVec4 ServiceGetMenuColor() {
    return Menu::GetMenuColor();
}

ImVec2 ServiceGetButtonSize(Menu::ButtonType type) {
    return Menu::GetButtonSize(type);
}

float ServiceGetButtonIndent(Menu::ButtonType type) {
    return Menu::GetButtonIndent(type);
}

ImVec2 ServiceGetButtonSizeInCoord(Menu::ButtonType type) {
    return Menu::GetButtonSizeInCoord(type);
}

float ServiceGetButtonIndentInCoord(Menu::ButtonType type) {
    return Menu::GetButtonIndentInCoord(type);
}

ImGuiKey ServiceCKKeyToImGuiKey(CKKEYBOARD key) {
    return Menu::CKKeyToImGuiKey(key);
}

CKKEYBOARD ServiceImGuiKeyToCKKey(ImGuiKey key) {
    return Menu::ImGuiKeyToCKKey(key);
}

bool ServiceKeyChordToString(ImGuiKeyChord keyChord, char *buf, size_t size) {
    return Menu::KeyChordToString(keyChord, buf, size);
}

bool ServiceSetKeyChordFromIO(ImGuiKeyChord *keyChord) {
    return Menu::SetKeyChordFromIO(keyChord);
}

void ServiceAddButtonImage(
    ImDrawList *drawList,
    const ImVec2 &pos,
    Menu::ButtonType type,
    int state,
    const char *text,
    const ImVec2 *textAlign) {
    if (textAlign) {
        Menu::AddButtonImage(drawList, pos, type, state, text, *textAlign);
    } else if (text) {
        Menu::AddButtonImage(drawList, pos, type, state, text);
    } else {
        Menu::AddButtonImage(drawList, pos, type, state);
    }
}

bool ServiceMainButton(const char *label, ImGuiButtonFlags flags) {
    return Menu::MainButton(label, flags);
}

bool ServiceOkButton(const char *label, ImGuiButtonFlags flags) {
    return Menu::OkButton(label, flags);
}

bool ServiceBackButton(const char *label, ImGuiButtonFlags flags) {
    return Menu::BackButton(label, flags);
}

bool ServiceOptionButton(const char *label, ImGuiButtonFlags flags) {
    return Menu::OptionButton(label, flags);
}

bool ServiceLevelButton(const char *label, bool *v, ImGuiButtonFlags flags) {
    return Menu::LevelButton(label, v, flags);
}

bool ServiceSmallButton(const char *label, bool *v, ImGuiButtonFlags flags) {
    return Menu::SmallButton(label, v, flags);
}

bool ServiceLeftButton(const char *label, ImGuiButtonFlags flags) {
    return Menu::LeftButton(label, flags);
}

bool ServiceRightButton(const char *label, ImGuiButtonFlags flags) {
    return Menu::RightButton(label, flags);
}

bool ServicePlusButton(const char *label, ImGuiButtonFlags flags) {
    return Menu::PlusButton(label, flags);
}

bool ServiceMinusButton(const char *label, ImGuiButtonFlags flags) {
    return Menu::MinusButton(label, flags);
}

bool ServiceKeyButton(
    const char *label,
    bool *toggled,
    ImGuiKeyChord *keyChord) {
    return Menu::KeyButton(label, toggled, keyChord);
}

bool ServiceYesNoButton(const char *label, bool *v) {
    return Menu::YesNoButton(label, v);
}

bool ServiceRadioButton(
    const char *label,
    int *currentItem,
    const char *const items[],
    int itemsCount) {
    return Menu::RadioButton(label, currentItem, items, itemsCount);
}

bool ServiceInputTextButton(
    const char *label,
    char *buf,
    size_t bufSize,
    ImGuiInputTextFlags flags,
    ImGuiInputTextCallback callback,
    void *userData) {
    return Menu::InputTextButton(label, buf, bufSize, flags, callback, userData);
}

bool ServiceInputFloatButton(
    const char *label,
    float *v,
    float step,
    float stepFast,
    const char *format,
    ImGuiInputTextFlags flags) {
    return Menu::InputFloatButton(label, v, step, stepFast, format, flags);
}

bool ServiceInputIntButton(
    const char *label,
    int *v,
    int step,
    int stepFast,
    ImGuiInputTextFlags flags) {
    return Menu::InputIntButton(label, v, step, stepFast, flags);
}

void ServiceWrappedText(
    const char *text,
    float width,
    float baseX,
    float scale) {
    Menu::WrappedText(text, width, baseX, scale);
}

bool ServiceNavLeft(float x, float y) {
    return Menu::NavLeft(x, y);
}

bool ServiceNavRight(float x, float y) {
    return Menu::NavRight(x, y);
}

bool ServiceNavBack(float x, float y) {
    return Menu::NavBack(x, y);
}

void ServiceTitle(const char *text, float y, float scale, ImU32 color) {
    Menu::Title(text, y, scale, color);
}

bool ServiceSearchBar(
    char *buffer,
    size_t bufferSize,
    float x,
    float y,
    float width) {
    return Menu::SearchBar(buffer, bufferSize, x, y, width);
}

const BML_MenuResourceApi g_MenuResourceApi = {
    sizeof(BML_MenuResourceApi),
    ServiceInitTextures,
    ServiceInitMaterials,
    ServiceInitSounds,
    ServiceLoadTexture,
    ServicePlayMenuClickSound,
};

const BML_MenuLayoutApi g_MenuLayoutApi = {
    sizeof(BML_MenuLayoutApi),
    ServiceGetMenuPos,
    ServiceGetMenuSize,
    ServiceGetMenuColor,
    ServiceGetButtonSize,
    ServiceGetButtonIndent,
    ServiceGetButtonSizeInCoord,
    ServiceGetButtonIndentInCoord,
};

const BML_MenuInputApi g_MenuInputApi = {
    sizeof(BML_MenuInputApi),
    ServiceCKKeyToImGuiKey,
    ServiceImGuiKeyToCKKey,
    ServiceKeyChordToString,
    ServiceSetKeyChordFromIO,
};

const BML_MenuDrawApi g_MenuDrawApi = {
    sizeof(BML_MenuDrawApi),
    ServiceAddButtonImage,
};

const BML_MenuWidgetApi g_MenuWidgetApi = {
    sizeof(BML_MenuWidgetApi),
    ServiceMainButton,
    ServiceOkButton,
    ServiceBackButton,
    ServiceOptionButton,
    ServiceLevelButton,
    ServiceSmallButton,
    ServiceLeftButton,
    ServiceRightButton,
    ServicePlusButton,
    ServiceMinusButton,
    ServiceKeyButton,
    ServiceYesNoButton,
    ServiceRadioButton,
    ServiceInputTextButton,
    ServiceInputFloatButton,
    ServiceInputIntButton,
    ServiceSearchBar,
};

const BML_MenuTextApi g_MenuTextApi = {
    sizeof(BML_MenuTextApi),
    ServiceWrappedText,
    ServiceTitle,
};

const BML_MenuNavApi g_MenuNavApi = {
    sizeof(BML_MenuNavApi),
    ServiceNavLeft,
    ServiceNavRight,
    ServiceNavBack,
};

const BML_MenuApi g_MenuApi = {
    BML_IFACE_HEADER(BML_MenuApi, BML_MENU_INTERFACE_ID, 1, 0),
    &g_MenuResourceApi,
    &g_MenuLayoutApi,
    &g_MenuInputApi,
    &g_MenuDrawApi,
    &g_MenuWidgetApi,
    &g_MenuTextApi,
    &g_MenuNavApi,
};
} // namespace

class MenuMod : public bml::Module {
public:
    BML_Result OnAttach() override {
        m_MenuApi = Publish(
            BML_MENU_INTERFACE_ID,
            &g_MenuApi,
            1,
            0,
            0,
            BML_INTERFACE_FLAG_IMMUTABLE);
        return m_MenuApi ? BML_RESULT_OK : BML_RESULT_FAIL;
    }

    BML_Result OnPrepareDetach() override {
        Menu::CleanupResources(nullptr);
        return m_MenuApi.Reset();
    }

private:
    bml::PublishedInterface m_MenuApi;
};

BML_DEFINE_MODULE(MenuMod)
