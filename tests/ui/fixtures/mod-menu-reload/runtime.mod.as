[bml.mod id="bml.ui.mod-menu-reload" name="Mod Menu Reload Fixture" version="1.0.0" author="BML" bml="0.4.0" reload="auto"]
class ModMenuReloadFixture {
  void OnLoad(const BML::ModContext &in ctx) {
    BML::Config@ config = ctx.Config();
    if (config is null)
      return;
    BML::ConfigPropertyDefinition definition;
    definition.Category = "General";
    definition.Key = "Enabled";
    definition.DefaultValue = BML::ConfigValue(false);
    BML::ConfigProperty@ enabled;
    config.DeclareProperty(definition, enabled);
  }
}
