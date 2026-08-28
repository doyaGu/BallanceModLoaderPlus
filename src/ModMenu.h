#ifndef BML_MODMENU_H
#define BML_MODMENU_H

#include <string>
#include <unordered_map>

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

class ModListPage : public Bui::TypedPage<ModMenu> {
public:
    explicit ModListPage() : Bui::TypedPage<ModMenu>("Mod List") {}

    void OnPostBegin() override;
    void OnDraw() override;
};

class ModPage : public Bui::TypedPage<ModMenu> {
public:
    explicit ModPage() : Bui::TypedPage<ModMenu>("Mod Page") {}

    void OnPostBegin() override;
    void OnDraw() override;

protected:
    static void ShowCommentBox(Category *category);

    Config *m_Config = nullptr;
    char m_TextBuf[1024] = {};
};

class ModOptionPage : public Bui::TypedPage<ModMenu> {
public:
    explicit ModOptionPage() : Bui::TypedPage<ModMenu>("Mod Options") {}

    void OnPostBegin() override;
    void OnDraw() override;
    void OnPreEnd() override;
    bool OnOpen() override;
    void OnClose() override;

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

    Category *m_Category = nullptr;
    Property *m_KeyCaptureProperty = nullptr;
    bool m_HasPendingChanges = false;
    std::unordered_map<Property *, PendingPropertyState> m_PendingValues;
};

#endif // BML_MODMENU_H
