#ifndef BML_UI_FONT_COMMAND_H
#define BML_UI_FONT_COMMAND_H

#include <string>
#include <vector>

#include "Console/CommandRecord.h"
#include "UI/FontRuntime.h"

class Property;

struct FontCommandContext {
    using Profile = BML::UI::FontProfile;

    BML::UI::FontRuntime *Runtime = nullptr;
    Property *PrimaryFace = nullptr;
    Property *ReferenceSize = nullptr;
    Property *FallbackFaces = nullptr;
    Property *FallbackReferenceSize = nullptr;
    Property *UseWindowsFallbacks = nullptr;

    bool IsComplete() const noexcept;
    Profile ReadProfile() const;
    std::vector<std::string> ReadFallbackFaces() const;
    void WriteFallbackFaces(const std::vector<std::string> &faces) const;
};

class FontCommand final {
public:
    explicit FontCommand(FontCommandContext context);

    std::string GetName() { return "font"; }

    std::string GetAlias() { return ""; }

    std::string GetDescription() { return "Inspect and configure the built-in UI font runtime."; }

    bool IsCheat() { return false; }

    void Execute(ModContext *bml, const std::vector<std::string> &args);
    const std::vector<std::string> GetTabCompletion(ModContext *bml,
                                                    const std::vector<std::string> &args);

private:
    void ExecutePrimary(ModContext &bml, const std::vector<std::string> &args);
    void ExecuteSize(ModContext &bml, const std::vector<std::string> &args);
    void ExecuteFallback(ModContext &bml, const std::vector<std::string> &args);
    void ExecuteSystemFallbacks(ModContext &bml, const std::vector<std::string> &args);
    void ShowStatus(ModContext &bml, bool includeHint) const;
    void ShowSources(ModContext &bml) const;
    void ShowCatalog(ModContext &bml);
    void ShowSample(ModContext &bml) const;
    void CheckText(ModContext &bml, const std::string &text) const;
    void ShowFallbacks(ModContext &bml) const;
    void ShowHelp(ModContext &bml) const;
    bool EnsureAvailable(ModContext &bml) const;
    void ScheduleConfiguredProfile(ModContext &bml, const std::string &message);

    FontCommandContext m_Context;
    std::vector<std::string> m_KnownFaces;
};

#endif // BML_UI_FONT_COMMAND_H
