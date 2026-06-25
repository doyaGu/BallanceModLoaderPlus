#ifndef BML_SCRIPTLIBRARYVALIDATOR_H
#define BML_SCRIPTLIBRARYVALIDATOR_H

#include <string>
#include <vector>

#include "ScriptLibraryRegistry.h"

namespace BML {

bool ValidateScriptLibraryPackage(const ScriptLibraryRegistry &registry,
                                  const ScriptLibraryPackage &package,
                                  std::vector<std::string> &lines);

} // namespace BML

#endif
