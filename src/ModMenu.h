#ifndef BML_MODMENU_H
#define BML_MODMENU_H

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <array>

#include "BML/Bui.h"

#include "Config.h"

class ModMenu;

class ModMenu : public Bui::Menu {
public:
    void Init();

    IMod *GetCurrentMod() const { return m_CurrentMod; }
    void SetCurrentMod(IMod *mod) { m_CurrentMod = mod; }

    Category *GetCurrentCategory() const { return m_CurrentCategory; }
    void SetCurrentCategory(Category *category) { m_CurrentCategory = category; }

    void OnOpen() override;
    void OnClose() override;

    static Config *GetConfig(IMod *mod);

private:
    IMod *m_CurrentMod = nullptr;
    Category *m_CurrentCategory = nullptr;
};

class ModListPage : public Bui::Page {
public:
    explicit ModListPage() : Bui::Page("Mod List") {}

    void OnPostBegin() override;
    void OnDraw() override;
};

class ModPage : public Bui::Page {
public:
    explicit ModPage() : Bui::Page("Mod Page") {}

    void OnPostBegin() override;
    void OnDraw() override;

protected:
    static void ShowCommentBox(Category *category);

    Config *m_Config = nullptr;
    char m_TextBuf[1024] = {};
};

class ModOptionPage : public Bui::Page {
public:
    explicit ModOptionPage() : Bui::Page("Mod Options") {}

    void OnPostBegin() override;
    void OnDraw() override;
    void OnPreEnd() override;
    bool OnOpen() override;
    void OnClose() override;
    void OnPageChanged(int newPage, int oldPage) override;

protected:
    struct PropertyState {
        Property *property = nullptr;
        IProperty::PropertyType type = IProperty::INTEGER;
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
