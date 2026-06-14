namespace BMLSmoke {
  [bml.mod id="bml.singlefile.smoke" name="BML Single File Script Smoke" version="1.0.0" author="BML+" bml="0.3.11" description="Smoke test for single-file BML AngelScript script mod loading."]
  class BMLSingleFileScriptSmoke {
    string BoolText(bool value) {
      return value ? "true" : "false";
    }

    void OnLoad(const BML::ModContext &in ctx) {
      const bool rootOk = BML::DirectoryExistsUtf8(ctx.GetModRootUtf8());
      const bool resourceOk = ctx.ModFileExistsUtf8("Resources/probe.txt") &&
                              ctx.ReadModTextFileUtf8("Resources/probe.txt", "") != "";
      ctx.LogInfo("BML single-file script smoke loaded resource=" + BoolText(rootOk && resourceOk));
    }
  }
}
