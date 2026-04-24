#ifndef BML_MODMENU_H
#define BML_MODMENU_H

#include <cstdint>
#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

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
    struct PendingPropertyState {
        IProperty::PropertyType type = IProperty::NONE;
        std::string originalString;
        std::string currentString;
        int originalInt = 0;
        int currentInt = 0;
        float originalFloat = 0.0f;
        float currentFloat = 0.0f;
        bool originalBool = false;
        bool currentBool = false;
        ImGuiKeyChord originalKeyChord = static_cast<ImGuiKeyChord>(0);
        ImGuiKeyChord currentKeyChord = static_cast<ImGuiKeyChord>(0);
    };

    static constexpr int PROPERTY_SLOTS = 4;
    void FlushBuffers();
    int GetPropertyStartIndex() const;
    Property *GetVisibleProperty(int index) const;
    const PendingPropertyState *FindPendingState(Property *property) const;
    PendingPropertyState &GetOrCreatePendingState(Property *property, IProperty::PropertyType type);
    void SyncVisiblePageToPending();
    void LoadOriginalValues();
    void SaveChanges();
    void RevertChanges();
    bool HasPendingChanges() const;

    static void ShowCommentBox(const Property *property);

    Category *m_Category = nullptr;
    bool m_HasPendingChanges = false;
    std::unordered_map<Property *, PendingPropertyState> m_PendingValues;

    static constexpr size_t BUFFER_SIZE = 4096;

    // Current working values
    char m_Buffers[PROPERTY_SLOTS][BUFFER_SIZE] = {};
    size_t m_BufferHashes[PROPERTY_SLOTS] = {};
    bool m_KeyToggled[PROPERTY_SLOTS] = {};
    ImGuiKeyChord m_KeyChord[PROPERTY_SLOTS] = {};
    std::uint8_t m_IntFlags[PROPERTY_SLOTS] = {};
    std::uint8_t m_FloatFlags[PROPERTY_SLOTS] = {};
    int m_IntValues[PROPERTY_SLOTS] = {};
    float m_FloatValues[PROPERTY_SLOTS] = {};
    bool m_BoolValues[PROPERTY_SLOTS] = {};

    // Original values
    char m_OriginalBuffers[PROPERTY_SLOTS][BUFFER_SIZE] = {};
    int m_OriginalIntValues[PROPERTY_SLOTS] = {};
    float m_OriginalFloatValues[PROPERTY_SLOTS] = {};
    bool m_OriginalBoolValues[PROPERTY_SLOTS] = {};
    ImGuiKeyChord m_OriginalKeyChord[PROPERTY_SLOTS] = {};
};

#endif // BML_MODMENU_H
