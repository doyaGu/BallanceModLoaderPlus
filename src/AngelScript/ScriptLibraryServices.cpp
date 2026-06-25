#include "ScriptLibraryServices.h"

#include "BML/Defines.h"
#include "ModContext.h"
#include "Utils/PathUtils.h"
#include "Utils/StringUtils.h"

namespace BML {

std::wstring GetScriptLibraryRootDirectory(ModContext *context) {
    if (!context)
        return {};
    const wchar_t *loaderDir = context->GetDirectory(BML_DIR_LOADER);
    if (!loaderDir || !*loaderDir)
        return {};
    return utils::CombinePathW(loaderDir, L"ScriptLibs");
}

std::string ScriptLibraryPackageKey(const std::string &id, const std::string &version) {
    return id + "@" + version;
}

ScriptLibraryRegistry MakeInstalledScriptLibraryRegistry(ModContext *context,
                                                         ScriptDiagnostic &diagnostic) {
    ScriptLibraryRegistry registry;
    diagnostic = ScriptDiagnostic();
    const std::wstring root = GetScriptLibraryRootDirectory(context);
    if (root.empty())
        return registry;

    registry.SetRootDirectory(root);
    std::string registryDiagnostic;
    if (!registry.Scan(registryDiagnostic)) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry, registryDiagnostic);
        diagnostic.EntryPath = utils::Utf16ToUtf8(registry.GetRootDirectory());
    }
    return registry;
}

} // namespace BML
