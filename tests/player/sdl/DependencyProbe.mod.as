[bml.mod id="SdlDependencyProbe" name="Dependency read regression" version="1.0.0"]
[bml.optional id="test.aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" version="1.2.3"]
class DependencyProbe {
    void OnLoad(const BML::ModContext &in ctx) {
        BML::ModRef@ mod = ctx.FindMod("SdlDependencyProbe");
        if (mod is null || ctx.GetModCount() < 3 || mod.GetDependencyCount() != 1 ||
            mod.CheckDependencies() != 1 ||
            mod.GetDependencyId(0) != "test.aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" ||
            mod.GetDependencyVersion(0) != "1.2.3" ||
            mod.GetDependencyVersionMajor(0) != 1 ||
            mod.GetDependencyVersionMinor(0) != 2 ||
            mod.GetDependencyVersionPatch(0) != 3 || !mod.IsDependencyOptional(0) ||
            mod.GetDependencyId(-1) != "" || mod.GetDependencyVersion(-1) != "" ||
            mod.GetDependencyVersionMajor(-1) != 0 || mod.IsDependencyOptional(-1) ||
            mod.GetDependencyId(1) != "" || mod.GetDependencyVersion(1) != "" ||
            mod.GetDependencyVersionPatch(1) != 0 || mod.IsDependencyOptional(1)) {
            ctx.LogError("Dependency reads changed their value, index, or ownership contract");
            return;
        }
        BML::CommandDefinition definition;
        definition.Name = "bml_probe_dependencies";
        definition.Description = "Marks successful script dependency validation";
        BML::CommandRef@ command = ctx.RegisterCommand(
            definition, BML::CommandCallback(this.OnCommand), null);
    }

    void OnCommand(const BML::ModContext &in ctx, const BML::CommandEvent &in event) {}
}
