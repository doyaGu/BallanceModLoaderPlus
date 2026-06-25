#ifndef BML_SCRIPTLIBRARYSERVICES_H
#define BML_SCRIPTLIBRARYSERVICES_H

#include <string>

#include "ScriptDiagnostic.h"
#include "ScriptLibraryRegistry.h"

class ModContext;

namespace BML {

std::wstring GetScriptLibraryRootDirectory(ModContext *context);
std::string ScriptLibraryPackageKey(const std::string &id, const std::string &version);
ScriptLibraryRegistry MakeInstalledScriptLibraryRegistry(ModContext *context,
                                                         ScriptDiagnostic &diagnostic);

} // namespace BML

#endif
