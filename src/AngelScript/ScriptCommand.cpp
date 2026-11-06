#include "AngelScript/ScriptCommand.h"

#include "Loader/ModContext.h"
#include "Console/Shell/ShellIo.h"
#if BML_ENABLE_ANGELSCRIPT
#include "AngelScript/ScriptDevToolsService.h"
#endif

void ScriptCommand::Execute(ModContext *bml, const std::vector<std::string> &args) {
#if BML_ENABLE_ANGELSCRIPT
    auto *context = bml;
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

const std::vector<std::string>
ScriptCommand::GetTabCompletion(ModContext *bml, const std::vector<std::string> &args) {
#if BML_ENABLE_ANGELSCRIPT
    auto *context = bml;
    if (context && context->GetScriptDevTools())
        return context->GetScriptDevTools()->CompleteCommand(args);
#endif
    if (args.size() == 2)
        return {"status", "list", "info", "diag", "logs"};
    return {};
}
