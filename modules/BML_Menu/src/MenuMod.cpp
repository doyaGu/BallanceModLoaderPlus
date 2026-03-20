#define BML_LOADER_IMPLEMENTATION
#define BML_MENU_PROVIDER_IMPLEMENTATION
#include "bml_module.hpp"

#include "bml_imgui.hpp"
#include "bml_menu.h"
#include "bml_interface.h"

#include "bml_menu.hpp"

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

ImVec2 ServiceGetMenuPos(const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return {};
    bml::imgui::ApiScope _scope(imguiApi);
    return Menu::GetMenuPos();
}

ImVec2 ServiceGetMenuSize(const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return {};
    bml::imgui::ApiScope _scope(imguiApi);
    return Menu::GetMenuSize();
}

ImVec4 ServiceGetMenuColor() {
    return Menu::GetMenuColor();
}

ImVec2 ServiceGetButtonSize(Menu::ButtonType type, const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return {};
    bml::imgui::ApiScope _scope(imguiApi);
    return Menu::GetButtonSize(type);
}

float ServiceGetButtonIndent(Menu::ButtonType type, const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return {};
    bml::imgui::ApiScope _scope(imguiApi);
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

bool ServiceKeyChordToString(ImGuiKeyChord keyChord, char *buf, size_t size, const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return {};
    bml::imgui::ApiScope _scope(imguiApi);
    return Menu::KeyChordToString(keyChord, buf, size);
}

bool ServiceSetKeyChordFromIO(ImGuiKeyChord *keyChord, const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return {};
    bml::imgui::ApiScope _scope(imguiApi);
    return Menu::SetKeyChordFromIO(keyChord);
}

void ServicePlayMenuClickSound() {
    Menu::PlayMenuClickSound();
}

void ServiceAddButtonImageState(
    ImDrawList *drawList,
    const ImVec2 &pos,
    Menu::ButtonType type,
    int state,
    const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return;
    bml::imgui::ApiScope _scope(imguiApi);
    Menu::AddButtonImage(drawList, pos, type, state);
}

void ServiceAddButtonImageSelected(
    ImDrawList *drawList,
    const ImVec2 &pos,
    Menu::ButtonType type,
    bool selected,
    const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return;
    bml::imgui::ApiScope _scope(imguiApi);
    Menu::AddButtonImage(drawList, pos, type, selected);
}

void ServiceAddButtonImageStateText(
    ImDrawList *drawList,
    const ImVec2 &pos,
    Menu::ButtonType type,
    int state,
    const char *text,
    const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return;
    bml::imgui::ApiScope _scope(imguiApi);
    Menu::AddButtonImage(drawList, pos, type, state, text);
}

void ServiceAddButtonImageSelectedText(
    ImDrawList *drawList,
    const ImVec2 &pos,
    Menu::ButtonType type,
    bool selected,
    const char *text,
    const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return;
    bml::imgui::ApiScope _scope(imguiApi);
    Menu::AddButtonImage(drawList, pos, type, selected, text);
}

void ServiceAddButtonImageStateTextAlign(
    ImDrawList *drawList,
    const ImVec2 &pos,
    Menu::ButtonType type,
    int state,
    const char *text,
    const ImVec2 &textAlign,
    const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return;
    bml::imgui::ApiScope _scope(imguiApi);
    Menu::AddButtonImage(drawList, pos, type, state, text, textAlign);
}

void ServiceAddButtonImageSelectedTextAlign(
    ImDrawList *drawList,
    const ImVec2 &pos,
    Menu::ButtonType type,
    bool selected,
    const char *text,
    const ImVec2 &textAlign,
    const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return;
    bml::imgui::ApiScope _scope(imguiApi);
    Menu::AddButtonImage(drawList, pos, type, selected, text, textAlign);
}

bool ServiceMainButton(const char *label, ImGuiButtonFlags flags, const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return {};
    bml::imgui::ApiScope _scope(imguiApi);
    return Menu::MainButton(label, flags);
}

bool ServiceOkButton(const char *label, ImGuiButtonFlags flags, const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return {};
    bml::imgui::ApiScope _scope(imguiApi);
    return Menu::OkButton(label, flags);
}

bool ServiceBackButton(const char *label, ImGuiButtonFlags flags, const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return {};
    bml::imgui::ApiScope _scope(imguiApi);
    return Menu::BackButton(label, flags);
}

bool ServiceOptionButton(const char *label, ImGuiButtonFlags flags, const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return {};
    bml::imgui::ApiScope _scope(imguiApi);
    return Menu::OptionButton(label, flags);
}

bool ServiceLevelButton(const char *label, bool *v, ImGuiButtonFlags flags, const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return {};
    bml::imgui::ApiScope _scope(imguiApi);
    return Menu::LevelButton(label, v, flags);
}

bool ServiceSmallButton(const char *label, bool *v, ImGuiButtonFlags flags, const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return {};
    bml::imgui::ApiScope _scope(imguiApi);
    return Menu::SmallButton(label, v, flags);
}

bool ServiceLeftButton(const char *label, ImGuiButtonFlags flags, const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return {};
    bml::imgui::ApiScope _scope(imguiApi);
    return Menu::LeftButton(label, flags);
}

bool ServiceRightButton(const char *label, ImGuiButtonFlags flags, const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return {};
    bml::imgui::ApiScope _scope(imguiApi);
    return Menu::RightButton(label, flags);
}

bool ServicePlusButton(const char *label, ImGuiButtonFlags flags, const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return {};
    bml::imgui::ApiScope _scope(imguiApi);
    return Menu::PlusButton(label, flags);
}

bool ServiceMinusButton(const char *label, ImGuiButtonFlags flags, const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return {};
    bml::imgui::ApiScope _scope(imguiApi);
    return Menu::MinusButton(label, flags);
}

bool ServiceKeyButton(
    const char *label,
    bool *toggled,
    ImGuiKeyChord *keyChord,
    const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return {};
    bml::imgui::ApiScope _scope(imguiApi);
    return Menu::KeyButton(label, toggled, keyChord);
}

bool ServiceYesNoButton(const char *label, bool *v, const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return {};
    bml::imgui::ApiScope _scope(imguiApi);
    return Menu::YesNoButton(label, v);
}

bool ServiceRadioButton(
    const char *label,
    int *currentItem,
    const char *const items[],
    int itemsCount,
    const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return {};
    bml::imgui::ApiScope _scope(imguiApi);
    return Menu::RadioButton(label, currentItem, items, itemsCount);
}

bool ServiceInputTextButton(
    const char *label,
    char *buf,
    size_t bufSize,
    ImGuiInputTextFlags flags,
    ImGuiInputTextCallback callback,
    void *userData,
    const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return {};
    bml::imgui::ApiScope _scope(imguiApi);
    return Menu::InputTextButton(label, buf, bufSize, flags, callback, userData);
}

bool ServiceInputFloatButton(
    const char *label,
    float *v,
    float step,
    float stepFast,
    const char *format,
    ImGuiInputTextFlags flags,
    const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return {};
    bml::imgui::ApiScope _scope(imguiApi);
    return Menu::InputFloatButton(label, v, step, stepFast, format, flags);
}

bool ServiceInputIntButton(
    const char *label,
    int *v,
    int step,
    int stepFast,
    ImGuiInputTextFlags flags,
    const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return {};
    bml::imgui::ApiScope _scope(imguiApi);
    return Menu::InputIntButton(label, v, step, stepFast, flags);
}

void ServiceWrappedText(
    const char *text,
    float width,
    float baseX,
    float scale,
    const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return;
    bml::imgui::ApiScope _scope(imguiApi);
    Menu::WrappedText(text, width, baseX, scale);
}

bool ServiceNavLeft(float x, float y, const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return {};
    bml::imgui::ApiScope _scope(imguiApi);
    return Menu::NavLeft(x, y);
}

bool ServiceNavRight(float x, float y, const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return {};
    bml::imgui::ApiScope _scope(imguiApi);
    return Menu::NavRight(x, y);
}

bool ServiceNavBack(float x, float y, const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return {};
    bml::imgui::ApiScope _scope(imguiApi);
    return Menu::NavBack(x, y);
}

void ServiceTitle(const char *text, float y, float scale, ImU32 color, const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return;
    bml::imgui::ApiScope _scope(imguiApi);
    Menu::Title(text, y, scale, color);
}

bool ServiceSearchBar(
    char *buffer,
    size_t bufferSize,
    float x,
    float y,
    float width,
    const BML_ImGuiApi *imguiApi) {
    if (!imguiApi) return {};
    bml::imgui::ApiScope _scope(imguiApi);
    return Menu::SearchBar(buffer, bufferSize, x, y, width);
}

const BML_MenuApi g_MenuApi = {
    BML_IFACE_HEADER(BML_MenuApi, BML_MENU_INTERFACE_ID, 1, 0),
    ServiceInitTextures,
    ServiceInitMaterials,
    ServiceInitSounds,
    ServiceLoadTexture,
    ServiceGetMenuPos,
    ServiceGetMenuSize,
    ServiceGetMenuColor,
    ServiceGetButtonSize,
    ServiceGetButtonIndent,
    ServiceGetButtonSizeInCoord,
    ServiceGetButtonIndentInCoord,
    ServiceCKKeyToImGuiKey,
    ServiceImGuiKeyToCKKey,
    ServiceKeyChordToString,
    ServiceSetKeyChordFromIO,
    ServicePlayMenuClickSound,
    ServiceAddButtonImageState,
    ServiceAddButtonImageSelected,
    ServiceAddButtonImageStateText,
    ServiceAddButtonImageSelectedText,
    ServiceAddButtonImageStateTextAlign,
    ServiceAddButtonImageSelectedTextAlign,
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
    ServiceWrappedText,
    ServiceNavLeft,
    ServiceNavRight,
    ServiceNavBack,
    ServiceTitle,
    ServiceSearchBar,
};
} // namespace

class MenuMod : public bml::Module {
public:
    BML_Result OnAttach(bml::ModuleServices &) override {
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
        return m_MenuApi.Reset();
    }

private:
    bml::PublishedInterface m_MenuApi;
};

BML_DEFINE_MODULE(MenuMod)

