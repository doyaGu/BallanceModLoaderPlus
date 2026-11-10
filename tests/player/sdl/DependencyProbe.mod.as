[bml.mod id="SdlDependencyProbe" name="Dependency read regression" version="1.0.0"]
[bml.optional id="test.aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" version="1.2.3"]
class DependencyProbe {
    void OnLoad(const BML::ModContext &in ctx) {
        BML::ModRef@ mod;
        BML::ModInfo info;
        BML::ModDependency dependency;
        array<BML::ModInfo> mods;
        bool satisfied = false;
        if (ctx.FindMod("SdlDependencyProbe", mod) != BML::ERROR_OK ||
            ctx.ReadMods(mods) != BML::ERROR_OK || mods.length() < 3 ||
            mod.Read(info) != BML::ERROR_OK || info.Definition.DependencyCount != 1 ||
            mod.CheckDependencies(satisfied) != BML::ERROR_OK || !satisfied ||
            info.Definition.ReadDependency(0, dependency) != BML::ERROR_OK ||
            dependency.Id != "test.aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" ||
            dependency.MinimumVersion != "1.2.3" || !dependency.Optional ||
            info.Definition.ReadDependency(1, dependency) != BML::ERROR_NOT_FOUND) {
            ctx.LogError("Dependency snapshot or status contract changed");
            return;
        }
        BML::CommandDefinition definition;
        definition.Name = "bml_probe_dependencies";
        definition.Description = "Marks successful script dependency validation";
        BML::CommandRef@ command;
        ctx.Commands().Register(definition, BML::CommandExecute(this.OnCommand), command);
    }
    int OnCommand(const BML::ModContext &in ctx, const BML::CommandInvocation &in invocation) { return 0; }
}
