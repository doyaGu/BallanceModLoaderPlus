[bml.mod id="example.command.config"
         name="Command and Config Example"
         version="1.0.0"
         author="BML+"
         description="Registers a command backed by persistent configuration"
         bml="0.4.0"]
class CommandConfigExample {
    private BML::ConfigProperty@ enabledProperty;
    private bool enabled = true;

    void OnLoad(const BML::ModContext &in ctx) {
        BML::Config@ config = ctx.Config();
        if (config !is null) {
            config.SetCategoryComment("Example", "Command and config example");
            BML::ConfigPropertyDefinition property;
            property.Category = "Example";
            property.Key = "Enabled";
            property.DefaultValue = BML::ConfigValue(true);
            property.Comment = "Feature state changed by /examplefeature.";
            if (config.DeclareProperty(property, enabledProperty) == BML::ERROR_OK)
                enabled = enabledProperty.GetBoolean(true);
        }

        BML::CommandDefinition definition;
        definition.Name = "examplefeature";
        definition.Description = "Show or toggle the example feature";
        definition.Usage = "examplefeature [status|toggle]";
        definition.Category = "Examples";
        definition.Enabled = true;

        BML::CommandExecute@ execute = BML::CommandExecute(this.OnCommand);
        BML::CommandComplete@ complete =
            BML::CommandComplete(this.CompleteCommand);
        BML::CommandRef@ command;
        int status = ctx.Commands().Register(definition, execute, command, complete);
        if (status != BML::ERROR_OK)
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

    private int OnCommand(const BML::ModContext &in ctx,
                           const BML::CommandInvocation &in event) {
        string action = "status";
        if (event.ArgumentCount != 0) event.ReadArgument(0, action);
        if (action == "toggle") {
            enabled = !enabled;
            if (enabledProperty !is null && enabledProperty.IsValid())
                enabledProperty.Set(BML::ConfigValue(enabled));
        } else if (action != "status") {
            BML::UI::AddMessage("Usage: /examplefeature [status|toggle]");
            return 1;
        }

        BML::UI::AddMessage("Example feature: " + BoolText(enabled));
        ctx.LogInfo("Example feature enabled=" + BoolText(enabled));
        return 0;
    }

    private int CompleteCommand(const BML::ModContext &in ctx,
                                 const BML::CommandCompletionRequest &in event,
                                 const BML::CommandCompletion &in completions) {
        completions.Add("status");
        return completions.Add("toggle");
    }

    private string BoolText(bool value) {
        return value ? "true" : "false";
    }
}
