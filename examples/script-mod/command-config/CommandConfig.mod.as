[bml.mod id="example.command.config"
         name="Command and Config Example"
         version="1.0.0"
         author="BML+"
         description="Registers a command backed by persistent configuration"
         bml="0.3.13"]
class CommandConfigExample {
    private BML::ConfigProperty@ enabledProperty;
    private bool enabled = true;

    void OnLoad(const BML::ModContext &in ctx) {
        BML::Config@ config = ctx.BorrowConfig();
        if (config !is null) {
            config.SetCategoryComment("Example", "Command and config example");
            @enabledProperty = config.GetProperty("Example", "Enabled");
            if (enabledProperty !is null) {
                enabledProperty.SetDefaultBoolean(true);
                enabledProperty.SetComment("Feature state changed by /examplefeature.");
                enabled = enabledProperty.GetBoolean(true);
            }
        }

        BML::CommandDefinition definition;
        definition.Name = "examplefeature";
        definition.Description = "Show or toggle the example feature";
        definition.Usage = "examplefeature [status|toggle]";
        definition.Category = "Examples";
        definition.Enabled = true;

        BML::CommandCallback@ execute = BML::CommandCallback(this.OnCommand);
        BML::CommandCompletionCallback@ complete =
            BML::CommandCompletionCallback(this.CompleteCommand);
        BML::CommandRef@ command = ctx.RegisterCommand(definition, execute, complete);
        if (command is null || !command.IsValid)
            ctx.LogWarn("Could not register /examplefeature");
    }

    void OnModifyConfig(const BML::ModContext &in ctx,
                        const BML::ConfigEvent &in event) {
        if (event.Category != "Example" || event.Key != "Enabled")
            return;

        BML::ConfigProperty@ property = event.BorrowProperty();
        if (property !is null)
            enabled = property.GetBoolean(enabled);
    }

    private void OnCommand(const BML::ModContext &in ctx,
                           const BML::CommandEvent &in event) {
        string action = event.ArgCount == 0 ? "status" : event.GetArg(0);
        if (action == "toggle") {
            enabled = !enabled;
            if (enabledProperty !is null && enabledProperty.IsValid)
                enabledProperty.SetBoolean(enabled);
        } else if (action != "status") {
            BML::UI::AddMessage("Usage: /examplefeature [status|toggle]");
            return;
        }

        BML::UI::AddMessage("Example feature: " + BoolText(enabled));
        ctx.LogInfo("Example feature enabled=" + BoolText(enabled));
    }

    private void CompleteCommand(const BML::ModContext &in ctx,
                                 const BML::CommandEvent &in event,
                                 BML::CommandCompletion &inout completions) {
        completions.Add("status");
        completions.Add("toggle");
    }

    private string BoolText(bool value) {
        return value ? "true" : "false";
    }
}
