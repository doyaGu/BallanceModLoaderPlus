#ifndef BML_MODMENU_H
#define BML_MODMENU_H

#include <string>
#include <unordered_map>
#include <array>

#include "bml_menu.hpp"

#include "Config.h"
#include "MenuRegistry.h"

class ModMenu;

class ModMenu : public Menu::Menu {
public:
    void Init();
    bool IsOpen() const { return m_CurrentPage != nullptr; }

    LoadedMod *GetCurrentMod() const { return m_CurrentMod; }
    void SetCurrentMod(LoadedMod *mod) { m_CurrentMod = mod; }

    Category *GetCurrentCategory() const { return m_CurrentCategory; }
    void SetCurrentCategory(Category *category) { m_CurrentCategory = category; }

    void OnOpen() override;
    void OnClose() override;

    static Config *GetConfig(LoadedMod *mod);

private:
    LoadedMod *m_CurrentMod = nullptr;
    Category *m_CurrentCategory = nullptr;
};

class ModListPage : public Menu::Page {
public:
    explicit ModListPage() : Menu::Page("Mod List") {}

    void OnPostBegin() override;
    void OnDraw() override;
};

class ModPage : public Menu::Page {
public:
    explicit ModPage() : Menu::Page("Mod Page") {}

    void OnPostBegin() override;
    void OnDraw() override;

protected:
    static void ShowCommentBox(Category *category);

    Config *m_Config = nullptr;
    char m_TextBuf[1024] = {};
};

class ModOptionPage : public Menu::Page {
public:
    explicit ModOptionPage() : Menu::Page("Mod Options") {}

    void OnPostBegin() override;
    void OnDraw() override;
    void OnPreEnd() override;
    bool OnOpen() override;
    void OnClose() override;
    void OnPageChanged(int newPage, int oldPage) override;

protected:
    struct PropertyState {
        Property *property = nullptr;
        Property::PropertyType type = Property::INTEGER;
        std::string stringValue;
        std::string originalString;
        bool boolValue = false;
        bool originalBool = false;
        int intValue = 0;
        int originalInt = 0;
        float floatValue = 0.0f;
        float originalFloat = 0.0f;
        ImGuiKeyChord keyChord = 0;
        ImGuiKeyChord originalKeyChord = 0;
        bool dirty = false;
    };

    void BindPageStates(int pageIndex);
    void SaveChanges();
    void RevertChanges();
    bool HasPendingChanges() const;
    void UpdateStateDirty(PropertyState *state);

    static void ShowCommentBox(const Property *property);

    Category *m_Category = nullptr;
    bool m_HasPendingChanges = false;

    std::unordered_map<Property *, PropertyState> m_PropertyStates;
    std::array<PropertyState *, 4> m_CurrentStates = {};

    static constexpr size_t BUFFER_SIZE = 4096;
    char m_StringBuffers[4][BUFFER_SIZE] = {};
    bool m_KeyToggled[4] = {};
};

#endif // BML_MODMENU_H

