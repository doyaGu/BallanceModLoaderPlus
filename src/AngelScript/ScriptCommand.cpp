#include "AngelScript/ScriptCommand.h"

#include "BML/IBML.h"
#include "Console/Shell/ShellIo.h"
#if BML_ENABLE_ANGELSCRIPT
#include "AngelScript/ScriptDevToolsService.h"
#include "Loader/ModContext.h"
#endif

void ScriptCommand::Execute(IBML *bml, const std::vector<std::string> &args) {
#if BML_ENABLE_ANGELSCRIPT
    auto *context = dynamic_cast<ModContext *>(bml);
    if (!context || !context->GetScriptDevTools()) {
        BML::Shell::Fail(bml, "Script developer tools unavailable.");
        return;
    }

    for (const std::string &line : context->GetScriptDevTools()->HandleCommand(args))
        bml->SendIngameMessage(line.c_str());
#else
    if (bml)
        BML::Shell::Fail(bml, "Script developer tools unavailable: AngelScript is disabled in this build.");
#endif
}

const std::vector<std::string> ScriptCommand::GetTabCompletion(IBML *bml, const std::vector<std::string> &args) {
#if BML_ENABLE_ANGELSCRIPT
    auto *context = dynamic_cast<ModContext *>(bml);
    if (context && context->GetScriptDevTools())
        return context->GetScriptDevTools()->CompleteCommand(args);
#endif
    if (args.size() == 2)
        return {"status", "list", "info", "diag", "logs"};
    return {};
}
