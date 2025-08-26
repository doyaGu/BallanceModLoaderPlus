#ifndef BML_MODMENU_H
#define BML_MODMENU_H

#include <vector>
#include <memory>

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

    void OnAfterBegin() override;
    void OnDraw() override;
};

class ModPage : public Bui::Page {
public:
    explicit ModPage() : Bui::Page("Mod Page") {}

    void OnAfterBegin() override;
    void OnDraw() override;

protected:
    static void ShowCommentBox(Category *category);

    Config *m_Config = nullptr;
    char m_TextBuf[1024] = {};
};

class ModOptionPage : public Bui::Page {
public:
    explicit ModOptionPage() : Bui::Page("Mod Options") {}

    void OnDraw() override;
    bool OnOpen() override;
    void OnClose() override;
    void OnPageChanged(int newPage, int oldPage) override;

protected:
    void FlushBuffers();

    static void ShowCommentBox(const Property *property);

    Category *m_Category = nullptr;

    static constexpr size_t BUFFER_SIZE = 4096;
    char m_Buffers[4][BUFFER_SIZE] = {};
    size_t m_BufferHashes[4] = {};
    bool m_KeyToggled[4] = {};
    ImGuiKeyChord m_KeyChord[4] = {};
    std::uint8_t m_IntFlags[4] = {};
    std::uint8_t m_FloatFlags[4] = {};
    int m_IntValues[4] = {};
    float m_FloatValues[4] = {};
};

#endif // BML_MODMENU_H
