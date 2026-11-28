[bml.mod id="bml.shutdown.smoke" name="BML AngelScript Shutdown Smoke" version="0.0.0" author="BML" bml="0.4.0" description="Smoke test for script-triggered clean shutdown."]
class BMLShutdownSmokeMod {
  bool requestedExit = false;

  void OnProcess(const BML::ModContext &in ctx) {
    if (requestedExit) {
      return;
    }
    requestedExit = true;
    if (ctx.IsValid()) {
      ctx.LogInfo("BML shutdown smoke requesting exit");
    }
    { int status; ctx.Commands().ExecuteLine("exit", status); }
  }
}
