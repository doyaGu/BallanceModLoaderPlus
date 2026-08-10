[bml.mod id="bml.bindings.smoke" name="BML Bindings Smoke" version="1.0.0" author="BML+" bml="0.3.13" description="Smoke test for BML's built-in AngelScript capability APIs."]
[bml.require id="BML" version="0.3.13"]

class BMLBindingsSmokeMod {
  BML::Events::Stream@ events;
  bool loggedGameplay = false;
  bool loggedPoll = false;

  void OnLoad(const BML::ModContext &in ctx) {
    BML::Runtime::State runtime = BML::Runtime::GetState();
    BML::Runtime::Clock clock = BML::Runtime::GetClock();
    BML::Runtime::Score score = BML::Runtime::GetScore();
    bool runtimeOk = runtime.Playing == (runtime.InGame && !runtime.Paused) &&
                     clock.Frame >= 0 && score.HS >= 0;
    bool streamOk = BML::Events::Open(events, 8) == BML::ERROR_OK && events !is null && events.IsOpen;
    ctx.LogInfo("BML capability smoke: runtime=" + (runtimeOk ? "true" : "false") +
                " stream=" + (streamOk ? "true" : "false"));
    ctx.LogInfo("BML script mod summary: capabilities");
  }

  void OnProcess(const BML::ModContext &in ctx) {
    if (!loggedGameplay) {
      array<BML::Gameplay::CatalogEntry>@ catalog;
      int status = BML::Gameplay::ReadCatalog(catalog);
      if (status == BML::ERROR_OK) {
        int count = catalog is null ? -1 : int(catalog.length());
        bool valuesOk = catalog !is null && catalog.length() > 0 &&
                        catalog[0].File.length() > 0;
        ctx.LogInfo("BML gameplay snapshot: status=" + status +
                    " count=" + count +
                    " values=" + (valuesOk ? "true" : "false"));
        loggedGameplay = true;
      } else if (status != BML::ERROR_IMC_UNSUPPORTED) {
        ctx.LogInfo("BML gameplay snapshot: status=" + status +
                    " count=-1 values=false");
        loggedGameplay = true;
      }
    }

    if (events is null || !events.IsOpen || loggedPoll)
      return;
    BML::Events::Event@ event;
    int status = events.Poll(event);
    ctx.LogInfo("BML IMC stream poll: status=" + status +
                " event=" + ((event is null) ? "none" : "record"));
    loggedPoll = true;
  }

  void OnUnload(const BML::ModContext &in ctx) {
    if (events !is null)
      events.Close();
    ctx.LogInfo("Goodbye!");
  }
}
