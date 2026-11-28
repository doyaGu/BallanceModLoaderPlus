namespace BMLSmoke {
  [bml.mod id="bml.singlefile.smoke" name="BML Single File Script Smoke" version="1.0.0" author="BML+" bml="0.4.0" description="Smoke test for single-file BML AngelScript script mod loading."]
  class BMLSingleFileScriptSmoke {
    string BoolText(bool value) {
      return value ? "true" : "false";
    }

    void OnLoad(const BML::ModContext &in ctx) {
      string root;
      const bool rootOk = ctx.GetDirectory(BML::DIR_MOD_ROOT, root) == BML::ERROR_OK && BML::Path::IsDirectory(root);
      bool exists = false; array<uint8> bytes;
      const bool resourceOk = ctx.ResourceExists("Resources/probe.txt", exists) == BML::ERROR_OK && exists &&
                              ctx.ReadResource("Resources/probe.txt", bytes) == BML::ERROR_OK && bytes.length() > 0;
      if (ctx.IsValid()) {
        ctx.LogInfo("BML single-file script smoke loaded resource=" + BoolText(rootOk && resourceOk));
      }
    }
  }
}
