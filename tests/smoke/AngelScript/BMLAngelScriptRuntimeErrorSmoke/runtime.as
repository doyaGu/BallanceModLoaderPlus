[bml.mod id="bml.runtime.error.smoke" name="BML AngelScript Runtime Error Smoke" version="0.0.0" author="BML" bml="0.3.0" description="Negative smoke test for script mod runtime diagnostics."]
class BMLRuntimeErrorSmokeMod {
  bool firstProcess = true;

  void OnLoad(const BML::ModContext &in ctx) {
    ctx.LogInfo("BML runtime error smoke loaded");
  }

  void OnProcess(const BML::ModContext &in ctx) {
    if (!firstProcess) {
      return;
    }
    firstProcess = false;

    throw("intentional BML runtime error smoke");
  }
}
