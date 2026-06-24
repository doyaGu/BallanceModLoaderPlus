[bml.mod id="bml.runtime.error.smoke" name="BML AngelScript Runtime Error Smoke" version="0.0.0" author="BML" bml="0.3.0" description="Negative smoke test for script mod runtime diagnostics."]
class BMLRuntimeErrorSmokeMod {
  bool firstProcess = true;

  void OnLoad(const BML::ModContext &in ctx) {
    BML::Logger@ logger = ctx.BorrowLogger();
    if (logger !is null) {
      logger.Info("BML runtime error smoke loaded");
    }
  }

  void OnProcess(const BML::ModContext &in ctx) {
    if (!firstProcess) {
      return;
    }
    firstProcess = false;

    ImGui::Begin("BML runtime error smoke leaked window", ImGuiWindowFlags_AlwaysAutoResize);
    throw("intentional BML runtime error smoke");
  }
}
