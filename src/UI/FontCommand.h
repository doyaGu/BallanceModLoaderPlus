#ifndef BML_UI_FONT_COMMAND_H
#define BML_UI_FONT_COMMAND_H

#include <string>
#include <vector>

#include "BML/ICommand.h"
#include "UI/FontRuntime.h"

class IProperty;

struct FontCommandContext {
    using Profile = BML::UI::FontProfile;

    BML::UI::FontRuntime *Runtime = nullptr;
    IProperty *PrimaryFace = nullptr;
    IProperty *ReferenceSize = nullptr;
    IProperty *FallbackFaces = nullptr;
    IProperty *FallbackReferenceSize = nullptr;
    IProperty *UseWindowsFallbacks = nullptr;

    bool IsComplete() const noexcept;
    Profile ReadProfile() const;
    std::vector<std::string> ReadFallbackFaces() const;
    void WriteFallbackFaces(const std::vector<std::string> &faces) const;
};

class FontCommand final : public ICommand {
public:
    explicit FontCommand(FontCommandContext context);

    std::string GetName() override { return "font"; }
    std::string GetAlias() override { return ""; }
    std::string GetDescription() override {
        return "Inspect and configure the built-in UI font runtime.";
    }
    bool IsCheat() override { return false; }

    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override;

private:
    void ExecutePrimary(IBML &bml, const std::vector<std::string> &args);
    void ExecuteSize(IBML &bml, const std::vector<std::string> &args);
    void ExecuteFallback(IBML &bml, const std::vector<std::string> &args);
    void ExecuteSystemFallbacks(IBML &bml, const std::vector<std::string> &args);
    void ShowStatus(IBML &bml, bool includeHint) const;
    void ShowSources(IBML &bml) const;
    void ShowCatalog(IBML &bml);
    void ShowSample(IBML &bml) const;
    void CheckText(IBML &bml, const std::string &text) const;
    void ShowFallbacks(IBML &bml) const;
    void ShowHelp(IBML &bml) const;
    bool EnsureAvailable(IBML &bml) const;
    void ScheduleConfiguredProfile(IBML &bml, const std::string &message);

    FontCommandContext m_Context;
    std::vector<std::string> m_KnownFaces;
};

#endif // BML_UI_FONT_COMMAND_H
