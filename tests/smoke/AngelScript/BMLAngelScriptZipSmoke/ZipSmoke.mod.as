[bml.mod id="bml.zip.smoke" name="BML Zip Script Smoke" version="1.0.0" author="BML+" bml="0.3.11" description="Smoke test for zipped BML AngelScript script mod loading."]
class BMLZipScriptSmoke {
  string BoolText(bool value) {
    return value ? "true" : "false";
  }

  void OnLoad(const BML::ModContext &in ctx) {
    const bool resourceOk = ctx.ModFileExistsUtf8("Resources/probe.txt") &&
                            ctx.ReadModTextFileUtf8("Resources/probe.txt", "") != "";
    BML::Logger@ logger = ctx.BorrowLogger();
    if (logger !is null) {
      logger.Info("BML zip script smoke loaded resource=" + BoolText(resourceOk));
    }
  }
}
