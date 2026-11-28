[bml.mod id="SdlEventProbe" name="Event and dependency regression" version="1.0.0"]
[bml.require id="SdlRuntimeProbe" version="1.0.0"]
class ScriptEventProbe {
    uint commands = 0;
    bool reported = false;
    void OnGameEvent(const BML::ModContext &in ctx, BML::GameEvent event) {
        if (event == BML::GAME_EVENT_POST_START_MENU) Mark(ctx, "bml_probe_combined_event");
    }
    void OnPostStartMenu(const BML::ModContext &in ctx) { throw("combined game event was double-delivered"); }
    void OnPreCommandExecute(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
        throw("combined command event was double-delivered");
    }
    void OnPostCommandExecute(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
        throw("combined command event was double-delivered");
    }
    void OnCommandEvent(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {
        if (event.Name == "bml_probe_command" && event.IsPost && ++commands == 2)
            Mark(ctx, "bml_probe_combined_command");
    }
    void OnProcess(const BML::ModContext &in ctx) {
        if (reported) return;
        BML::ModRef@ provider; BML::ModRef@ owner;
        BML::ModInfo providerInfo; BML::ModInfo ownerInfo;
        if (ctx.FindMod("SdlRuntimeProbe", provider) != BML::ERROR_OK ||
            provider.Read(providerInfo) != BML::ERROR_OK || providerInfo.State != BML::MOD_STATE_FAILED) return;
        bool satisfied = true;
        if (ctx.Owner(owner) != BML::ERROR_OK || owner.Read(ownerInfo) != BML::ERROR_OK ||
            ownerInfo.State != BML::MOD_STATE_LOADED || owner.CheckDependencies(satisfied) != BML::ERROR_OK || satisfied)
            throw("instant dependency query was replaced by consumer state");
        reported = true;
        Mark(ctx, "bml_probe_dependency_failed");
    }
    void Mark(const BML::ModContext &in ctx, const string &in name) {
        BML::CommandDefinition definition; definition.Name = name;
        BML::CommandRef@ command;
        if (ctx.Commands().Register(definition, BML::CommandExecute(this.Marker), command) != BML::ERROR_OK)
            throw("event marker registration failed");
    }
    int Marker(const BML::ModContext &in ctx, const BML::CommandInvocation &in call) { return 0; }
}
