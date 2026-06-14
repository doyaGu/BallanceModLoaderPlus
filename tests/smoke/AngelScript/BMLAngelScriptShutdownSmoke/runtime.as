[bml.mod id="bml.shutdown.smoke" name="BML AngelScript Shutdown Smoke" version="0.0.0" author="BML" bml="0.3.0" description="Smoke test for script-triggered clean shutdown."]
class BMLShutdownSmokeMod {
  bool requestedExit = false;

  void OnProcess(const BML::ModContext &in ctx) {
    if (requestedExit) {
      return;
    }
    requestedExit = true;
    BML::Logger@ logger = ctx.BorrowLogger();
    if (logger !is null) {
      logger.Info("BML shutdown smoke requesting exit");
    }
    ctx.ExecuteCommand("exit");
  }
}
