#ifndef BML_MODMENU_H
#define BML_MODMENU_H

#include <vector>
#include <memory>

#include "BML/Bui.h"

#include "Config.h"

class ModMenu;

class ModMenuPage : public Bui::Page {
public:
    ModMenuPage(ModMenu *menu, std::string name);
    ~ModMenuPage() override;

    void OnClose() override;

protected:
    ModMenu *m_Menu;
};

class ModMenu : public Bui::Menu {
public:
    void Init();
    void Shutdown();

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
    std::vector<std::unique_ptr<ModMenuPage>> m_Pages;
};

class ModListPage : public ModMenuPage {
public:
    explicit ModListPage(ModMenu *menu) : ModMenuPage(menu, "Mod List") {}

    void OnAfterBegin() override;
    void OnDraw() override;
};

class ModPage : public ModMenuPage {
public:
    explicit ModPage(ModMenu *menu) : ModMenuPage(menu, "Mod Page") {}

    void OnAfterBegin() override;
    void OnDraw() override;

protected:
    static void ShowCommentBox(Category *category);

    Config *m_Config = nullptr;
    char m_TextBuf[1024] = {};
};

class ModOptionPage : public ModMenuPage {
public:
    explicit ModOptionPage(ModMenu *menu) : ModMenuPage(menu, "Mod Options") {
        // Initialize buffers safely
        for (int i = 0; i < 4; ++i) {
            memset(m_Buffers[i], 0, sizeof(m_Buffers[i]));
            m_BufferHashes[i] = 0;
            m_KeyToggled[i] = false;
            m_KeyChord[i] = ImGuiKey_None;
            m_IntFlags[i] = 0;
            m_FloatFlags[i] = 0;
            m_IntValues[i] = 0;
            m_FloatValues[i] = 0.0f;
        }
    }

    void OnAfterBegin() override;
    void OnDraw() override;
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
