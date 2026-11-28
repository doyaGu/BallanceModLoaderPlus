[bml.mod id="bml.zip.smoke" name="BML Zip Script Smoke" version="1.0.0" author="BML+" bml="0.4.0" description="Smoke test for zipped BML AngelScript script mod loading."]
class BMLZipScriptSmoke {
  string BoolText(bool value) {
    return value ? "true" : "false";
  }

  void OnLoad(const BML::ModContext &in ctx) {
    bool exists = false; array<uint8> bytes;
    const bool resourceOk = ctx.ResourceExists("Resources/probe.txt", exists) == BML::ERROR_OK && exists &&
                            ctx.ReadResource("Resources/probe.txt", bytes) == BML::ERROR_OK && bytes.length() > 0;
    if (ctx.IsValid()) {
      ctx.LogInfo("BML zip script smoke loaded resource=" + BoolText(resourceOk));
    }
  }
}
