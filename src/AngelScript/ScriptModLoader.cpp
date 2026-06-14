#include "ScriptModLoader.h"

#include <vector>

#include "ScriptModDefinitionBuilder.h"
#include "Utils/StringUtils.h"

namespace BML {

ScriptModDefinition ScriptModLoader::MakePlaceholderDefinition(const ScriptModLoadCandidate &candidate) {
    ScriptModDefinition placeholder;
    const std::string syntheticId = candidate.SyntheticId.empty()
                                        ? MakeSyntheticScriptModId(candidate.RootDirectory)
                                        : candidate.SyntheticId;
    placeholder.Id = syntheticId;
    placeholder.Name = syntheticId;
    placeholder.Author = "";
    placeholder.Description = "";
    placeholder.Version = "0.0.0";
    placeholder.Enabled = true;
    return placeholder;
}

ScriptModLoadResult ScriptModLoader::Load(ModContext *owner,
                                          CKContext *context,
                                          const ScriptModLoadCandidate &candidate) const {
    ScriptModLoadResult result;

    const ScriptModDefinition placeholder = MakePlaceholderDefinition(candidate);
    ScriptModDefinition definition = placeholder;
    ScriptModRuntime runtime(MakeSyntheticScriptModuleName(candidate.RootDirectory));
    ScriptDiagnostic failure;
    ScriptModEntry entry = MakeScriptModEntry(candidate, candidate.EntryPaths.empty() ? std::wstring() : candidate.EntryPaths.front());

    if (candidate.EntryPaths.empty()) {
        failure = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry, "No *.mod.as entry file found.");
        failure.EntryPath = utils::Utf16ToUtf8(candidate.SourcePath.empty() ? candidate.RootDirectory : candidate.SourcePath);
        result.Failed = true;
    } else if (candidate.EntryPaths.size() > 1) {
        failure = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry, "Multiple *.mod.as entry files found.");
        failure.EntryPath = utils::Utf16ToUtf8(candidate.SourcePath.empty() ? candidate.RootDirectory : candidate.SourcePath);
        result.Failed = true;
    } else {
        ScriptModDefinitionBuilder builder;
        if (!builder.Build(context, entry, runtime, definition, failure)) {
            if (runtime.IsModuleLoaded()) {
                ScriptDiagnostic releaseDiagnostic;
                runtime.Release(context, &releaseDiagnostic);
            }
            definition = placeholder;
            definition.Entry = entry.EntryFilename;
            result.Failed = true;
        }
    }

    result.Definition = definition;
    result.Mod = std::make_unique<ScriptMod>(owner, definition, entry, std::move(runtime));
    if (result.Failed)
        result.Mod->SetLoadFailure(failure);
    return result;
}

} // namespace BML
