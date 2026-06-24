[bml.mod id="bml.state.reload.smoke" name="BML AngelScript State Reload Smoke" version="1.0.0" author="BML" bml="0.3.0" reload="auto" description="Smoke test for script hot reload state migration."]
class BMLStateReloadSmokeMod {
  int frames = 0;

  void OnLoad(const BML::ModContext &in ctx) {
    BML::Logger@ logger = ctx.BorrowLogger();
    if (logger !is null) {
      logger.Info("BML state reload smoke v1 ready");
    }
  }

  void OnProcess(const BML::ModContext &in ctx) {
    ++frames;
    if ((frames % 30) != 0) {
      return;
    }

    BML::Logger@ logger = ctx.BorrowLogger();
    if (logger !is null) {
      logger.Info("BML state reload smoke v1 heartbeat " + frames);
    }
  }

  void SaveState(BML::StateBag@ state) {
    if (state is null) {
      return;
    }
    state.SetBool("saved", true);
    state.SetInt("counter", 1234);
    state.SetString("text", "from-v1");
  }

  void RestoreState(BML::StateBag@ state) {
    if (state is null) {
      return;
    }
  }
}
