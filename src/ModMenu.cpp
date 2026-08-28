#include "ModMenu.h"

#include "BuiInternal.h"

#include <algorithm>
#include <set>
#include <vector>

#include "BML/InputHook.h"

#include "BuiInternal.h"
#include "ModContext.h"
#include "PathUtils.h"
#include "StringUtils.h"

#if BML_ENABLE_ANGELSCRIPT
#include "ScriptMod.h"
#endif

namespace {
    std::vector<std::string> g_FontFilenames = {"unifont.otf"};

    bool IsKnownFontFilename(const char *value) {
        if (!value || value[0] == '\0')
            return false;

        return std::find_if(g_FontFilenames.begin(), g_FontFilenames.end(),
                            [value](const std::string &choice) {
                                return utils::CStringEqual(value, choice.c_str());
                            }) != g_FontFilenames.end();
    }

    std::vector<const char *> BuildFontFilenameItems(const char *currentValue, std::string &customItem) {
        std::vector<const char *> items;
        items.reserve(g_FontFilenames.size() + 1);

        for (const auto &choice : g_FontFilenames) {
            items.push_back(choice.c_str());
        }

        if (currentValue && currentValue[0] != '\0' && !IsKnownFontFilename(currentValue)) {
            customItem = currentValue;
            items.push_back(customItem.c_str());
        }

        return items;
    }

    int FindFontFilenameItem(const char *value, const std::vector<const char *> &items) {
        if (!value)
            value = "";

        for (size_t i = 0; i < items.size(); ++i) {
            if (utils::CStringEqual(value, items[i]))
                return static_cast<int>(i);
        }

        return 0;
    }

    bool IsBmlFontFilenameProperty(IMod *mod, Category *category, const Property *property) {
        if (!mod || !category || !property)
            return false;

        if (!utils::CStringEqual(mod->GetID(), "BML") || !utils::CStringEqual(category->GetName(), "GUI"))
            return false;

        return utils::CStringEqual(property->GetName(), "FontFilename") ||
               utils::CStringEqual(property->GetName(), "SecondaryFontFilename");
    }

    const char *GetModDisplayName(IMod *mod) {
        if (!mod)
            return "";

        const char *name = mod->GetName();
        if (name && name[0] != '\0')
            return name;

        const char *id = mod->GetID();
        return id ? id : "";
    }

    void RefreshFontList() {
        g_FontFilenames.clear();
        std::string fontsDir = std::string(BML_GetModContext()->GetDirectoryUtf8(BML_DIR_LOADER)) + "\\Fonts";
        std::vector<std::string> files = utils::ListFilesUtf8(fontsDir, "*");

        struct CaseInsensitiveLess {
            bool operator()(const std::string &a, const std::string &b) const {
                return utils::CompareString(a, b) < 0;
            }
        };
        std::set<std::string, CaseInsensitiveLess> sortedFonts;

        for (const auto &file : files) {
            if (utils::EndsWith(file, ".ttf", false) || utils::EndsWith(file, ".otf", false)) {
                sortedFonts.insert(file);
            }
        }

        if (sortedFonts.empty()) {
            sortedFonts.insert("unifont.otf");
        }

        g_FontFilenames.assign(sortedFonts.begin(), sortedFonts.end());
    }

#if BML_ENABLE_ANGELSCRIPT
    const BML::ScriptMod *AsScriptMod(const IMod *mod) {
        return dynamic_cast<const BML::ScriptMod *>(mod);
    }

    const char *GetScriptModStateLabel(const BML::ScriptMod *mod) {
        if (!mod)
            return "";
        if (mod->IsFailed())
            return "failed";
        return mod->IsLoaded() ? "loaded" : "registered";
    }
#endif
}

void ModMenu::Init() {
    CreatePage<ModListPage>();
    CreatePage<ModPage>();
    CreatePage<ModOptionPage>();
}

void ModMenu::OnOpen() {
    Bui::BlockKeyboardInput(this);
}

void ModMenu::OnClose() {
    Bui::TransitionToScriptAndUnblock("Menu_Options", this);
}

Config *ModMenu::GetConfig(IMod *mod) {
    return BML_GetModContext()->GetConfig(mod);
}

void ModListPage::OnPostBegin() {
    Bui::Title(m_Title.c_str());

    const int count = BML_GetModContext()->GetModCount();
    SetPageCount(Bui::CalcPageCount(count, 4));

    if (Bui::CanPrevPage(m_PageIndex) && Bui::NavLeft()) PrevPage();
    if (Bui::CanNextPage(m_PageIndex, count, 4) && Bui::NavRight()) NextPage();
}

void ModListPage::OnDraw() {
    const int n = GetPage() * 4;

    Bui::Entries([&](size_t index) {
        IMod *mod = BML_GetModContext()->GetMod(static_cast<int>(n + index));
        if (!mod)
            return false;

        ImGui::PushID(mod);
        const bool clicked = Bui::MainButton(GetModDisplayName(mod));
        ImGui::PopID();

        if (clicked) {
            Menu()->SetCurrentMod(mod);
            Menu()->OpenPage("Mod Page");
        }
        return true;
    }, 0.35f, 0.24f, 0.14f, 4);
}

void ModPage::OnPostBegin() {
    const auto menuPos = Bui::GetMenuPos();
    const auto menuSize = Bui::GetMenuSize();

    const float titleX = menuPos.x * 1.03f;
    const float titleWidth = menuSize.x * 0.95f;

    ImGui::Dummy(Bui::CoordToPixel(ImVec2(1.0f, 0.1f)));

    auto *mod = Menu()->GetCurrentMod();

    Bui::WrappedText(mod->GetName(), titleWidth, titleX, 1.2f);

    snprintf(m_TextBuf, sizeof(m_TextBuf), "By %s", mod->GetAuthor());
    Bui::WrappedText(m_TextBuf, titleWidth, titleX);

    snprintf(m_TextBuf, sizeof(m_TextBuf), "v%s", mod->GetVersion());
    Bui::WrappedText(m_TextBuf, titleWidth, titleX);

#if BML_ENABLE_ANGELSCRIPT
    if (const auto *scriptMod = AsScriptMod(mod)) {
        snprintf(m_TextBuf,
                 sizeof(m_TextBuf),
                 "Script mod: %s",
                 GetScriptModStateLabel(scriptMod));
        Bui::WrappedText(m_TextBuf, titleWidth, titleX);

        if (scriptMod->IsFailed() || !scriptMod->GetLastDiagnostic().empty()) {
            Bui::WrappedText("Diagnostic:", titleWidth, titleX);
            Bui::WrappedText(scriptMod->GetLastDiagnostic().c_str(), titleWidth, titleX);
        }
    }
#endif

    ImGui::NewLine();

    Bui::WrappedText(mod->GetDescription(), titleWidth, titleX);

    m_Config = ModMenu::GetConfig(mod);
    if (!m_Config)
        return;

    int count = (int) m_Config->GetCategoryCount();
    SetPageCount(Bui::CalcPageCount(count, 4));
}

void ModPage::OnDraw() {
    if (!m_Config)
        return;

    bool v = true;
    const int n = GetPage() * 4;

    Bui::Entries([&](size_t index) {
        Category *category = m_Config->GetCategory(static_cast<int>(n + index));
        if (!category)
            return false;

        const char *name = category->GetName();
        if (!name)
            return true;

        ImGui::PushID(category);
        const bool clicked = Bui::LevelButton(name, &v);
        ImGui::PopID();

        if (clicked) {
            Menu()->SetCurrentCategory(category);
            Menu()->OpenPage("Mod Options");
        }

        if (ImGui::IsItemHovered()) {
            ShowCommentBox(category);
        }
        return true;
    }, 0.4031f, 0.5f, 0.06f, 4);

    const int totalCategories = m_Config ? (int)m_Config->GetCategoryCount() : 0;

    if (Bui::CanPrevPage(m_PageIndex) &&
        Bui::NavLeft(0.35f, 0.59f)) {
        PrevPage();
    }

    if (Bui::CanNextPage(m_PageIndex, totalCategories, 4) &&
        Bui::NavRight(0.6138f, 0.59f)) {
        NextPage();
    }
}

void ModPage::ShowCommentBox(Category *category) {
    if (!category)
        return;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, Bui::GetMenuColor());

    const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
    const ImVec2 commentBoxPos(vpSize.x * 0.725f, vpSize.y * 0.4f);
    const ImVec2 commentBoxSize(vpSize.x * 0.25f, vpSize.y * 0.2f);
    ImGui::SetCursorScreenPos(commentBoxPos);
    ImGui::BeginChild("ModComment", commentBoxSize);

    const char *name = category->GetName();
    if (name[0] == '@')
        ++name;
    Bui::WrappedText(name, commentBoxSize.x);
    Bui::WrappedText(category->GetComment(), commentBoxSize.x);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void ModOptionPage::OnPostBegin() {
    Bui::Title(m_Name.c_str(), 0.13f, 1.5f,  m_HasPendingChanges ? IM_COL32(255, 255, 128, 255) : IM_COL32_WHITE);

    // Navigation
    const int totalProps = m_Category ? (int)m_Category->GetPropertyCount() : 0;
    if (Bui::CanPrevPage(m_PageIndex) && Bui::NavLeft()) PrevPage();
    if (Bui::CanNextPage(m_PageIndex, totalProps, PROPERTY_SLOTS) && Bui::NavRight()) NextPage();

    // Update pending changes status
    m_HasPendingChanges = HasPendingChanges();
}

void ModOptionPage::OnDraw() {
    if (!m_Category)
        return;

    const int n = GetPage() * PROPERTY_SLOTS;

    Bui::Entries([&](size_t index) {
        Property *property = m_Category->GetProperty(static_cast<int>(n + index));
        if (!property)
            return false;

        const char *name = property->GetName();
        if (!name || name[0] == '\0')
            return true;

        ImGui::PushID(property);
        PendingPropertyState &state = GetOrCreatePendingState(property);
        DrawEditor(property, state.current);

        ImGui::PopID();

        if (ImGui::IsItemHovered()) {
            ShowCommentBox(property);
        }

        return true;
    }, 0.35f, 0.24f, 0.14f, PROPERTY_SLOTS);

    m_HasPendingChanges = HasPendingChanges();
}

void ModOptionPage::OnPreEnd() {
    // Show save/revert buttons if there are pending changes
    if (m_HasPendingChanges) {
        const float x = Bui::GetButtonSizeInCoord(Bui::BUTTON_SMALL).x;
        Bui::At(0.5f - (x + 0.04f), 0.85f, [&]() {
            if (Bui::SmallButton("Save")) {
                SaveChanges();
            }
        });

        Bui::At(0.54f, 0.85f, [&]() {
            if (Bui::SmallButton("Revert")) {
                RevertChanges();
            }
        });
    } else {
        if (Bui::NavBack()) {
            if (auto *menu = Menu()) {
                menu->OpenPrevPage();
            } else {
                Close();
            }
        }
    }
}

bool ModOptionPage::OnOpen() {
    RefreshFontList();

    m_Category = Menu()->GetCurrentCategory();
    if (!m_Category)
        return false;

    int count = static_cast<int>(m_Category->GetPropertyCount());
    SetPageCount(Bui::CalcPageCount(count, PROPERTY_SLOTS));

    m_PendingValues.clear();
    m_KeyCaptureProperty = nullptr;
    m_HasPendingChanges = false;
    return true;
}

void ModOptionPage::OnClose() {
    m_PendingValues.clear();
    m_KeyCaptureProperty = nullptr;
    m_HasPendingChanges = false;
}

ModOptionPage::PendingPropertyState &ModOptionPage::GetOrCreatePendingState(Property *property) {
    auto [it, inserted] = m_PendingValues.try_emplace(property);
    if (inserted) {
        it->second.original = property->GetValue();
        it->second.current = it->second.original;
    }
    return it->second;
}

bool ModOptionPage::DrawEditor(Property *property, Property::Value &value) {
    const Property::Value previous = value;

    switch (property->GetType()) {
        case IProperty::STRING: {
            std::string &text = std::get<std::string>(value);
            auto *modMenu = Menu();
            IMod *currentMod = modMenu ? modMenu->GetCurrentMod() : nullptr;

            if (IsBmlFontFilenameProperty(currentMod, m_Category, property)) {
                std::string customFont;
                std::vector<const char *> fontItems = BuildFontFilenameItems(text.c_str(), customFont);
                int currentItem = FindFontFilenameItem(text.c_str(), fontItems);
                if (Bui::RadioButton(property->GetName(), &currentItem, fontItems.data(),
                                     static_cast<int>(fontItems.size()))) {
                    text = fontItems[static_cast<size_t>(currentItem)];
                }
            } else {
                Bui::InputTextButton(property->GetName(), &text);
            }
            break;
        }
        case IProperty::BOOLEAN:
            Bui::YesNoButton(property->GetName(), &std::get<bool>(value));
            break;
        case IProperty::INTEGER:
            Bui::InputIntButton(property->GetName(), &std::get<int>(value));
            break;
        case IProperty::KEY: {
            bool capturing = m_KeyCaptureProperty == property;
            ImGuiKeyChord chord = Bui::CKKeyToImGuiKey(static_cast<CKKEYBOARD>(std::get<int>(value)));
            if (Bui::KeyButton(property->GetName(), &capturing, &chord)) {
                chord &= ~ImGuiMod_Mask_;
                value = static_cast<int>(Bui::ImGuiKeyToCKKey(static_cast<ImGuiKey>(chord)));
            }

            if (capturing) {
                m_KeyCaptureProperty = property;
            } else if (m_KeyCaptureProperty == property) {
                m_KeyCaptureProperty = nullptr;
            }
            break;
        }
        case IProperty::FLOAT:
            Bui::InputFloatButton(property->GetName(), &std::get<float>(value));
            break;
        default:
            ImGui::Dummy(Bui::GetButtonSize(Bui::BUTTON_OPTION));
            break;
    }

    return value != previous;
}

void ModOptionPage::SaveChanges() {
    if (!m_Category)
        return;

    for (const auto &entry : m_PendingValues) {
        Property *property = entry.first;
        const PendingPropertyState &state = entry.second;
        if (!property || state.current == state.original)
            continue;

        property->SetValue(state.current);
    }

    m_PendingValues.clear();
    m_KeyCaptureProperty = nullptr;
    m_HasPendingChanges = false;
}

void ModOptionPage::RevertChanges() {
    if (!m_Category)
        return;

    m_PendingValues.clear();
    m_KeyCaptureProperty = nullptr;
    m_HasPendingChanges = false;
}

bool ModOptionPage::HasPendingChanges() const {
    for (const auto &entry : m_PendingValues) {
        if (entry.second.current != entry.second.original)
            return true;
    }

    return false;
}

void ModOptionPage::ShowCommentBox(const Property *property) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Bui::GetMenuColor());

    const ImVec2 &vpSize = ImGui::GetMainViewport()->Size;
    const ImVec2 commentBoxPos(vpSize.x * 0.725f, vpSize.y * 0.35f);
    const ImVec2 commentBoxSize(vpSize.x * 0.25f, vpSize.y * 0.3f);
    ImGui::SetCursorScreenPos(commentBoxPos);
    ImGui::BeginChild("ModOptionComment", commentBoxSize);

    Bui::WrappedText(property->GetName(), commentBoxSize.x);
    Bui::WrappedText(property->GetComment(), commentBoxSize.x);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}
