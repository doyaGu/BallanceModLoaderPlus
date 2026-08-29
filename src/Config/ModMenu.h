#ifndef BML_MODMENU_H
#define BML_MODMENU_H

#include <string>
#include <unordered_map>

#include "BML/Bui.h"

#include "Config/Config.h"

class ModMenuState {
public:
    IMod *GetCurrentMod() const { return m_CurrentMod; }
    void SelectMod(IMod *mod) { m_CurrentMod = mod; }

    Category *GetCurrentCategory() const { return m_CurrentCategory; }
    void SelectCategory(Category *category) { m_CurrentCategory = category; }

    Config *GetConfig(IMod *mod) const;

private:
    IMod *m_CurrentMod = nullptr;
    Category *m_CurrentCategory = nullptr;
};

class ModMenu {
public:
    ModMenu();
    void Init();

    bool Open(const std::string &id) { return m_Routes.Open(id); }
    bool Close() { return m_Routes.Close(); }
    bool Render() { return m_Routes.Render(); }

private:
    // Routes is declared last so its Pages are destroyed before their state.
    ModMenuState m_State;
    Bui::Menu m_Routes;
};

class ModListPage : public Bui::Page {
public:
    explicit ModListPage(ModMenuState &state) : m_State(state) {}

    Bui::PageAction OnFrame() override;

private:
    ModMenuState &m_State;
    Bui::Pagination m_Pagination;
};

class ModPage : public Bui::Page {
public:
    explicit ModPage(ModMenuState &state) : m_State(state) {}

    Bui::PageAction OnFrame() override;

protected:
    static void ShowCommentBox(Category *category);

    ModMenuState &m_State;
    Bui::Pagination m_Pagination;
    Config *m_Config = nullptr;
    char m_TextBuf[1024] = {};
};

class ModOptionPage : public Bui::Page {
public:
    explicit ModOptionPage(ModMenuState &state) : m_State(state) {}

    void OnEnter(Bui::PageEnterReason reason) override;
    Bui::PageAction OnFrame() override;
    void OnLeave(Bui::PageLeaveReason reason) override;

protected:
    struct PendingPropertyState {
        IProperty::PropertyType type = IProperty::NONE;
        Property::Value original;
        Property::Value current;
    };

    static constexpr int PROPERTY_SLOTS = 4;
    PendingPropertyState &GetOrCreatePendingState(Property *property);
    bool DrawEditor(Property *property, IProperty::PropertyType type, Property::Value &value);
    void SaveChanges();
    void RevertChanges();
    bool HasPendingChanges() const;

    static void ShowCommentBox(const Property *property);

    ModMenuState &m_State;
    Bui::Pagination m_Pagination;
    Category *m_Category = nullptr;
    Property *m_KeyCaptureProperty = nullptr;
    bool m_HasPendingChanges = false;
    std::unordered_map<Property *, PendingPropertyState> m_PendingValues;
};

#endif // BML_MODMENU_H
