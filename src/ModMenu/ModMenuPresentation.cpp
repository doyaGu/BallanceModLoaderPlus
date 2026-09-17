#include "ModMenu/ModMenuPresentation.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "BML/Bui.h"

#include "ModMenu/ModMenuModel.h"
#include "UI/BuiInternal.h"

namespace {
    constexpr int PageSize = 4;
    constexpr float ListX = 0.35f;
    constexpr float ListY = 0.24f;
    constexpr float ListSpacing = 0.14f;
    constexpr float DetailsX = 0.4031f;
    constexpr float DetailsY = 0.50f;
    constexpr float DetailsSpacing = 0.06f;
    constexpr float TextPanelScrollbarScale = 0.45f;
    constexpr float MinimumTextPanelScrollbarSize = 6.0f;
    constexpr ImVec4 ScrollbarGrabColor = {224.0f / 255.0f, 169.0f / 255.0f, 113.0f / 255.0f, 195.0f / 255.0f};
    constexpr ImVec4 ScrollbarHoverColor = {235.0f / 255.0f, 190.0f / 255.0f, 122.0f / 255.0f, 210.0f / 255.0f};
    constexpr ImVec4 ScrollbarActiveColor = {190.0f / 255.0f, 128.0f / 255.0f, 52.0f / 255.0f, 225.0f / 255.0f};
    constexpr ImGuiWindowFlags TextPanelFlags = ImGuiWindowFlags_NoSavedSettings |
                                                ImGuiWindowFlags_NoNavInputs |
                                                ImGuiWindowFlags_NoNavFocus;

    struct TextPanelLayout {
        float x;
        float y;
        float width;
        float height;
    };

    struct ViewportLayout {
        void Update() {
            const ImGuiViewport *viewport = ImGui::GetMainViewport();
            position = viewport->Pos;
            size = viewport->Size;
        }

        ImVec2 Position(float x, float y) const {
            return {position.x + size.x * x, position.y + size.y * y};
        }

        ImVec2 Size(float width, float height) const {
            return {size.x * width, size.y * height};
        }

        void SetCursor(float x, float y) const {
            ImGui::SetCursorScreenPos(Position(x, y));
        }

        ImVec2 position;
        ImVec2 size;
    };

    constexpr TextPanelLayout ModInformationPanel = {0.31f, 0.10f, 0.38f, 0.35f};
    constexpr TextPanelLayout DetailsCommentPanel = {0.725f, 0.40f, 0.25f, 0.20f};
    constexpr TextPanelLayout SettingCommentPanel = {0.725f, 0.35f, 0.25f, 0.30f};

    static_assert(ModInformationPanel.y + ModInformationPanel.height < DetailsY,
                  "Mod information must not overlap the details action list");

    class TextPanelStyleScope final {
    public:
        TextPanelStyleScope() {
            const float scrollbarSize = std::max(
                MinimumTextPanelScrollbarSize, ImGui::GetFontSize() * TextPanelScrollbarScale);
            ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, scrollbarSize);
            ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 0.0f);
            ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ScrollbarGrabColor);
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ScrollbarHoverColor);
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ScrollbarActiveColor);
        }

        ~TextPanelStyleScope() {
            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar(2);
        }

        TextPanelStyleScope(const TextPanelStyleScope &) = delete;
        TextPanelStyleScope &operator=(const TextPanelStyleScope &) = delete;
    };

    const char *FailureText(const ModMenuModel &model) {
        if (!model.GetNotice().empty())
            return model.GetNotice().c_str();

        switch (model.GetSession().GetStatus()) {
        case ModMenuSessionStatus::Conflict:
            return "A setting changed outside this menu.";
        case ModMenuSessionStatus::Stale:
            return "The settings structure changed.";
        case ModMenuSessionStatus::OwnerGone:
            return "This Mod was unloaded or replaced.";
        case ModMenuSessionStatus::AwaitingDocument:
            return "Loading Mod settings.";
        case ModMenuSessionStatus::Ready:
        case ModMenuSessionStatus::NoSelection:
        default:
            return "";
        }
    }

    void DrawTextPanel(const ViewportLayout &viewport, const char *id,
                       const TextPanelLayout &layout,
                       const char *title, const char *text, bool resetScroll = false) {
        const ImVec2 size = viewport.Size(layout.width, layout.height);

        TextPanelStyleScope textPanelStyle;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Bui::GetMenuColor());
        viewport.SetCursor(layout.x, layout.y);
        if (resetScroll)
            ImGui::SetNextWindowScroll(ImVec2(-1.0f, 0.0f));
        if (ImGui::BeginChild(id, size, ImGuiChildFlags_None, TextPanelFlags)) {
            const float textWidth = ImGui::GetContentRegionAvail().x;
            Bui::WrappedText(title, textWidth);
            if (text && text[0] != '\0')
                Bui::WrappedText(text, textWidth);
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    struct CommentPanelState {
        void Clear() {
            source = std::monostate{};
            title.clear();
            text.clear();
            resetScroll = false;
        }

        void ObserveSetting(const ModMenuSettingKey &key, std::string_view nextTitle,
                            std::string_view nextText) {
            const auto *setting = std::get_if<ModMenuSettingKey>(&source);
            if (setting && *setting == key)
                return;

            source = key;
            title.assign(nextTitle);
            text.assign(nextText);
            resetScroll = true;
        }

        void ObserveDetailsAction(const ModMenuCategoryKey &nextKey,
                                  std::string_view nextTitle,
                                  std::string_view nextText) {
            const auto *key = std::get_if<ModMenuCategoryKey>(&source);
            if (key && *key == nextKey)
                return;

            source = nextKey;
            title.assign(nextTitle);
            text.assign(nextText);
            resetScroll = true;
        }

        void ObserveDetailsAction(const ModMenuPageKey &nextKey,
                                  std::string_view nextTitle,
                                  std::string_view nextText) {
            const auto *key = std::get_if<ModMenuPageKey>(&source);
            if (key && *key == nextKey)
                return;

            source = nextKey;
            title.assign(nextTitle);
            text.assign(nextText);
            resetScroll = true;
        }

        bool IsSetting(const ModMenuSettingKey &key) const {
            const auto *setting = std::get_if<ModMenuSettingKey>(&source);
            return setting && *setting == key;
        }

        bool IsDetailsAction(const ModMenuCategoryKey &key) const {
            const auto *category = std::get_if<ModMenuCategoryKey>(&source);
            return category && *category == key;
        }

        bool IsDetailsAction(const ModMenuPageKey &key) const {
            const auto *page = std::get_if<ModMenuPageKey>(&source);
            return page && *page == key;
        }

        void Draw(const ViewportLayout &viewport, const char *id,
                  const TextPanelLayout &layout) {
            if (std::holds_alternative<std::monostate>(source))
                return;

            DrawTextPanel(viewport, id, layout, title.c_str(), text.c_str(), resetScroll);
            resetScroll = false;
        }

        std::variant<std::monostate, ModMenuCategoryKey, ModMenuPageKey,
                     ModMenuSettingKey> source;
        std::string title;
        std::string text;
        bool resetScroll = false;
    };
}

struct ModMenuPresentation::State {
    void BeginFrame() {
        viewport.Update();
    }

    void Reset() {
        modPagination = {};
        detailsPagination = {};
        settingPagination = {};
        observedOwner.reset();
        observedSchemaRevision = 0;
        observedPageRevision = 0;
        selectedCategory.clear();
        authorLine.clear();
        versionLine.clear();
        statusLine.clear();
        keyCapture.reset();
        resetInformationScroll = true;
        comment.Clear();
    }

    void ObserveDocument(const ModMenuDocument &document) {
        const bool ownerChanged = !observedOwner || *observedOwner != document.owner;
        const bool documentChanged = ownerChanged ||
            observedSchemaRevision != document.schemaRevision ||
            observedPageRevision != document.pageRevision;
        if (!documentChanged)
            return;

        observedOwner = document.owner;
        observedSchemaRevision = document.schemaRevision;
        observedPageRevision = document.pageRevision;
        if (ownerChanged) {
            authorLine = document.author.empty() ? std::string() : "By " + document.author;
            versionLine = document.version.empty() ? std::string() : "v" + document.version;
            statusLine = document.status.empty() ? std::string() : "Script mod: " + document.status;
            detailsPagination = {};
            settingPagination = {};
            selectedCategory.clear();
            resetInformationScroll = true;
        }

        keyCapture.reset();
        comment.Clear();
    }

    void ObserveCategory(const ModMenuDocument &document, std::string_view category) {
        ObserveDocument(document);
        if (selectedCategory == category)
            return;

        selectedCategory.assign(category);
        settingPagination = {};
        keyCapture.reset();
        comment.Clear();
    }

    bool DrawModList(ModMenuModel &model) {
        const std::vector<ModMenuModSummary> &mods = model.GetMods();
        modPagination.Update(static_cast<int>(mods.size()), PageSize);
        if (mods.empty()) {
            viewport.SetCursor(ListX, ListY);
            Bui::WrappedText("No Mods are currently available.",
                             Bui::GetButtonSize(Bui::BUTTON_MAIN).x);
            return false;
        }

        if (modPagination.CanPrevious() && Bui::NavLeft())
            modPagination.Previous();
        if (modPagination.CanNext() && Bui::NavRight())
            modPagination.Next();

        const int first = modPagination.GetFirstItem();
        for (int row = 0; row < PageSize; ++row) {
            const int index = first + row;
            if (index >= static_cast<int>(mods.size()))
                break;

            const ModMenuModSummary &mod = mods[static_cast<std::size_t>(index)];
            viewport.SetCursor(ListX, ListY + ListSpacing * static_cast<float>(row));
            ImGui::PushID(mod.owner.id.c_str());
            const bool pressed = Bui::MainButton(mod.name.c_str());
            ImGui::PopID();
            if (pressed && model.SelectMod(mod.owner)) {
                observedOwner.reset();
                return true;
            }
        }
        return false;
    }

    void DrawModInformation(const ModMenuDocument &document) {
        const ImVec2 size = viewport.Size(ModInformationPanel.width,
                                          ModInformationPanel.height);

        TextPanelStyleScope textPanelStyle;
        viewport.SetCursor(ModInformationPanel.x, ModInformationPanel.y);
        if (resetInformationScroll)
            ImGui::SetNextWindowScroll(ImVec2(-1.0f, 0.0f));
        const bool drawContents = ImGui::BeginChild(
            "ModInformation", size, ImGuiChildFlags_None,
            TextPanelFlags | ImGuiWindowFlags_NoBackground);
        resetInformationScroll = false;

        if (drawContents) {
            const float textWidth = ImGui::GetContentRegionAvail().x;
            Bui::WrappedText(document.name.c_str(), textWidth, 0.0f, 1.2f);
            Bui::WrappedText(authorLine.c_str(), textWidth);
            Bui::WrappedText(versionLine.c_str(), textWidth);
            Bui::WrappedText(statusLine.c_str(), textWidth);
            if (!document.diagnostic.empty()) {
                Bui::WrappedText("Diagnostic:", textWidth);
                Bui::WrappedText(document.diagnostic.c_str(), textWidth);
            }
            if (!document.description.empty()) {
                ImGui::NewLine();
                Bui::WrappedText(document.description.c_str(), textWidth);
            }
        }
        ImGui::EndChild();
    }

    ModMenuRouteAction DrawDetailsActions(ModMenuModel &model,
                                          const ModMenuDocument &document) {
        const std::size_t actionCount = document.detailsActions.size();
        detailsPagination.Update(static_cast<int>(actionCount), PageSize);
        if (actionCount == 0) {
            comment.Clear();
            viewport.SetCursor(DetailsX, DetailsY);
            Bui::WrappedText("No configurable options.",
                             Bui::GetButtonSize(Bui::BUTTON_LEVEL).x);
            return ModMenuRouteAction::None;
        }

        const int first = detailsPagination.GetFirstItem();
        bool commentOnPage = false;
        for (int row = 0; row < PageSize; ++row) {
            const int index = first + row;
            if (index >= static_cast<int>(actionCount))
                break;

            const ModMenuDetailsActionDocument &detailsAction =
                document.detailsActions[static_cast<std::size_t>(index)];
            const auto *category = std::get_if<ModMenuCategoryDocument>(&detailsAction);
            const auto *page = std::get_if<ModMenuPageInfo>(&detailsAction);
            if (!category && !page)
                continue;
            const std::string &id = category ? category->key.id : page->key.id;
            const std::string &label = category ? category->label : page->label;
            const std::string &description = category ? category->description
                                                      : page->description;
            viewport.SetCursor(DetailsX, DetailsY + DetailsSpacing * static_cast<float>(row));
            ImGui::PushID(static_cast<int>(detailsAction.index()));
            ImGui::PushID(id.c_str());
            bool selected = true;
            const bool pressed = Bui::LevelButton(label.c_str(), &selected);
            if (ImGui::IsItemHovered()) {
                if (category)
                    comment.ObserveDetailsAction(category->key, label, description);
                else
                    comment.ObserveDetailsAction(page->key, label, description);
            }
            if ((category && comment.IsDetailsAction(category->key)) ||
                (page && comment.IsDetailsAction(page->key))) {
                commentOnPage = true;
            }
            ImGui::PopID();
            ImGui::PopID();

            if (!pressed)
                continue;
            if (category) {
                if (!model.SelectDetailsAction(category->key))
                    continue;
                ObserveCategory(document, category->key.id);
                return ModMenuRouteAction::OpenSettings;
            }
            if (!model.SelectDetailsAction(page->key))
                continue;
            comment.Clear();
            return ModMenuRouteAction::OpenPage;
        }

        bool pageChanged = false;
        if (detailsPagination.CanPrevious() && Bui::NavLeft(0.35f, 0.59f)) {
            detailsPagination.Previous();
            pageChanged = true;
        }
        if (detailsPagination.CanNext() && Bui::NavRight(0.6138f, 0.59f)) {
            detailsPagination.Next();
            pageChanged = true;
        }

        if (pageChanged || !commentOnPage)
            comment.Clear();
        comment.Draw(viewport, "ModComment", DetailsCommentPanel);
        return ModMenuRouteAction::None;
    }

    void DrawKeySetting(ModMenuModel &model, const ModMenuSettingKey &key,
                        const char *label, int source) {
        bool capturing = keyCapture && *keyCapture == key;
        ImGuiKeyChord chord = Bui::CKKeyToImGuiKey(static_cast<CKKEYBOARD>(source));
        if (Bui::KeyButton(label, &capturing, &chord) && !capturing) {
            chord &= ~ImGuiMod_Mask_;
            model.EditSetting(
                key, static_cast<int>(Bui::ImGuiKeyToCKKey(static_cast<ImGuiKey>(chord))));
        }

        if (capturing)
            keyCapture = key;
        else if (keyCapture && *keyCapture == key)
            keyCapture.reset();
    }

    void DrawSetting(ModMenuModel &model, const ModMenuSettingDocument &setting) {
        const ModMenuSettingValue *source = model.GetSession().GetValue(setting.key);
        if (!source) {
            ImGui::Dummy(Bui::GetButtonSize(Bui::BUTTON_OPTION));
            return;
        }

        switch (setting.type) {
        case IProperty::STRING: {
            std::string next = std::get<std::string>(*source);
            if (Bui::InputTextButton(setting.label.c_str(), &next))
                model.EditSetting(setting.key, std::move(next));
            break;
        }
        case IProperty::BOOLEAN: {
            bool next = std::get<bool>(*source);
            const bool previous = next;
            Bui::YesNoButton(setting.label.c_str(), &next);
            if (next != previous)
                model.EditSetting(setting.key, next);
            break;
        }
        case IProperty::INTEGER: {
            int next = std::get<int>(*source);
            if (Bui::InputIntButton(setting.label.c_str(), &next))
                model.EditSetting(setting.key, next);
            break;
        }
        case IProperty::KEY:
            DrawKeySetting(model, setting.key, setting.label.c_str(), std::get<int>(*source));
            break;
        case IProperty::FLOAT: {
            float next = std::get<float>(*source);
            if (Bui::InputFloatButton(setting.label.c_str(), &next))
                model.EditSetting(setting.key, next);
            break;
        }
        case IProperty::NONE:
        default:
            ImGui::Dummy(Bui::GetButtonSize(Bui::BUTTON_OPTION));
            break;
        }
    }

    void DrawSettings(ModMenuModel &model, const ModMenuDocument &document,
                      const ModMenuCategoryDocument &category) {
        ObserveCategory(document, category.key.id);
        settingPagination.Update(static_cast<int>(category.settings.size()), PageSize);
        if (category.settings.empty()) {
            comment.Clear();
            viewport.SetCursor(ListX, ListY);
            Bui::WrappedText("No settings in this category.",
                             Bui::GetButtonSize(Bui::BUTTON_MAIN).x);
            return;
        }

        if (settingPagination.CanPrevious() && Bui::NavLeft()) {
            settingPagination.Previous();
            keyCapture.reset();
            comment.Clear();
        }
        if (settingPagination.CanNext() && Bui::NavRight()) {
            settingPagination.Next();
            keyCapture.reset();
            comment.Clear();
        }

        const int first = settingPagination.GetFirstItem();
        bool commentOnPage = false;
        for (int row = 0; row < PageSize; ++row) {
            const int index = first + row;
            if (index >= static_cast<int>(category.settings.size()))
                break;

            const ModMenuSettingDocument &setting =
                category.settings[static_cast<std::size_t>(index)];
            viewport.SetCursor(ListX, ListY + ListSpacing * static_cast<float>(row));
            ImGui::PushID(setting.key.property.c_str());
            DrawSetting(model, setting);
            if (ImGui::IsItemHovered())
                comment.ObserveSetting(setting.key, setting.label, setting.description);
            if (comment.IsSetting(setting.key))
                commentOnPage = true;
            ImGui::PopID();
        }

        if (!commentOnPage)
            comment.Clear();
        if (FailureText(model)[0] == '\0')
            comment.Draw(viewport, "ModOptionComment", SettingCommentPanel);
    }

    void DrawPendingActions(ModMenuModel &model) {
        const float buttonWidth = Bui::GetButtonSizeInCoord(Bui::BUTTON_SMALL).x;
        viewport.SetCursor(0.5f - (buttonWidth + 0.04f), 0.85f);
        ImGui::BeginDisabled(!model.GetSession().CanApply());
        if (Bui::SmallButton("Save"))
            model.RequestApply();
        ImGui::EndDisabled();

        viewport.SetCursor(0.54f, 0.85f);
        if (Bui::SmallButton("Revert"))
            model.RequestRevert();

        const char *failure = FailureText(model);
        if (failure[0] != '\0') {
            DrawTextPanel(viewport, "ModOptionError", SettingCommentPanel,
                          "Unable to save", failure);
        }
    }

    Bui::Pagination modPagination;
    Bui::Pagination detailsPagination;
    Bui::Pagination settingPagination;
    std::optional<ModMenuOwner> observedOwner;
    std::uint64_t observedSchemaRevision = 0;
    std::uint64_t observedPageRevision = 0;
    std::string selectedCategory;
    std::string authorLine;
    std::string versionLine;
    std::string statusLine;
    std::optional<ModMenuSettingKey> keyCapture;
    bool resetInformationScroll = true;
    CommentPanelState comment;
    ViewportLayout viewport;
};

ModMenuPresentation::ModMenuPresentation() : m_State(std::make_unique<State>()) {}

ModMenuPresentation::~ModMenuPresentation() = default;

void ModMenuPresentation::Reset() {
    m_State->Reset();
}

ModMenuRouteAction ModMenuPresentation::DrawModListPage(ModMenuModel &model) {
    m_State->BeginFrame();
    Bui::Title("Mod List");
    if (m_State->DrawModList(model))
        return ModMenuRouteAction::OpenDetails;
    return Bui::NavBack() ? ModMenuRouteAction::Back : ModMenuRouteAction::None;
}

ModMenuRouteAction ModMenuPresentation::DrawDetailsPage(ModMenuModel &model) {
    m_State->BeginFrame();
    model.SynchronizeSelected();
    const ModMenuDocument *document = model.GetSession().GetDocument();
    if (!document) {
        Bui::Title("Mod unavailable");
        const char *failure = FailureText(model);
        if (failure[0] != '\0') {
            DrawTextPanel(m_State->viewport, "ModError", DetailsCommentPanel,
                          "Mod unavailable", failure);
        }
        return Bui::NavBack() ? ModMenuRouteAction::Back : ModMenuRouteAction::None;
    }

    m_State->ObserveDocument(*document);
    m_State->DrawModInformation(*document);
    const ModMenuRouteAction action = m_State->DrawDetailsActions(model, *document);
    if (action != ModMenuRouteAction::None)
        return action;
    return Bui::NavBack() ? ModMenuRouteAction::Back : ModMenuRouteAction::None;
}

ModMenuRouteAction ModMenuPresentation::DrawPage(ModMenuModel &model, bool entered) {
    m_State->BeginFrame();
    if (!entered) {
        Bui::Title("Page unavailable");
        m_State->viewport.SetCursor(ListX, ListY);
        Bui::WrappedText("The Mod could not open this page.",
                         Bui::GetButtonSize(Bui::BUTTON_MAIN).x);
        return Bui::NavBack() ? ModMenuRouteAction::Back : ModMenuRouteAction::None;
    }

    model.SynchronizeSelected();
    const ModMenuDocument *document = model.GetSession().GetDocument();
    const ModMenuDetailsActionDocument *selected = model.GetSelectedDetailsAction();
    const ModMenuPageInfo *page = selected ? std::get_if<ModMenuPageInfo>(selected) : nullptr;
    if (!document || !page) {
        Bui::Title("Page unavailable");
        return Bui::NavBack() ? ModMenuRouteAction::Back : ModMenuRouteAction::None;
    }

    BML_ModMenuPageAction action = BML_MOD_MENU_PAGE_NONE;
    ImGui::PushID(document->owner.id.c_str());
    ImGui::PushID(page->key.id.c_str());
    const int result = model.DrawPage(action);
    ImGui::PopID();
    ImGui::PopID();

    if (result != BML_OK) {
        Bui::Title("Page unavailable");
        m_State->viewport.SetCursor(ListX, ListY);
        Bui::WrappedText("The Mod could not draw this page.",
                         Bui::GetButtonSize(Bui::BUTTON_MAIN).x);
        return Bui::NavBack() ? ModMenuRouteAction::Back : ModMenuRouteAction::None;
    }
    if (action == BML_MOD_MENU_PAGE_BACK)
        return ModMenuRouteAction::Back;
    if (action == BML_MOD_MENU_PAGE_CLOSE)
        return ModMenuRouteAction::Close;
    return ModMenuRouteAction::None;
}

ModMenuRouteAction ModMenuPresentation::DrawSettingsPage(ModMenuModel &model) {
    m_State->BeginFrame();
    model.SynchronizeSelected();
    const ModMenuDocument *document = model.GetSession().GetDocument();
    const ModMenuDetailsActionDocument *action = model.GetSelectedDetailsAction();
    const ModMenuCategoryDocument *category = action
        ? std::get_if<ModMenuCategoryDocument>(action)
        : nullptr;
    const bool dirtyAtFrameStart = model.GetSession().IsDirty();
    Bui::Title("Mod Options", 0.13f, 1.5f,
               dirtyAtFrameStart ? IM_COL32(255, 255, 128, 255) : IM_COL32_WHITE);

    if (document && action && category)
        m_State->DrawSettings(model, *document, *category);

    if (model.GetSession().IsDirty()) {
        m_State->DrawPendingActions(model);
        return ModMenuRouteAction::None;
    }

    return Bui::NavBack() ? ModMenuRouteAction::Back : ModMenuRouteAction::None;
}
