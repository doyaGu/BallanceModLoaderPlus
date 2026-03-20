#ifndef BML_SCRIPTING_RUNTIME_PROVIDER_H
#define BML_SCRIPTING_RUNTIME_PROVIDER_H

#include "bml_module_runtime.h"
#include "bml_export.h"

namespace BML::Scripting {

class ScriptInstanceManager;

const BML_ModuleRuntimeProvider *GetRuntimeProvider();

void SetInstanceManager(ScriptInstanceManager *manager);

BML_Result RegisterProvider(PFN_BML_GetProcAddress get_proc, const char *owner_id);
void UnregisterProvider(PFN_BML_GetProcAddress get_proc);

} // namespace BML::Scripting

#endif // BML_SCRIPTING_RUNTIME_PROVIDER_H
