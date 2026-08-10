[bml.mod id="bml.bindings.smoke" name="BML Bindings Smoke" version="1.0.0" author="BML+" bml="0.3.13" description="Smoke test for BML's built-in AngelScript capability APIs."]
[bml.require id="BML" version="0.3.13"]

class BMLBindingsSmokeMod {
  BML::Events::Stream@ events;
  bool loggedPoll = false;

  void Log(const BML::ModContext &in ctx, const string &in message) {
    BML::Logger@ logger = ctx.BorrowLogger();
    if (logger !is null)
      logger.Info(message);
  }

  void OnLoad(const BML::ModContext &in ctx) {
    BML::Runtime::State runtime = BML::Runtime::GetState();
    BML::Runtime::Clock clock = BML::Runtime::GetClock();
    BML::Runtime::Score score = BML::Runtime::GetScore();
    bool runtimeOk = runtime.Playing == (runtime.InGame && !runtime.Paused) &&
                     clock.Frame >= 0 && score.HS >= 0;
    bool streamOk = BML::Events::Open(events, 8) == BML::ERROR_OK && events !is null && events.IsOpen;
    Log(ctx, "BML capability smoke: runtime=" + (runtimeOk ? "true" : "false") +
             " stream=" + (streamOk ? "true" : "false"));
    Log(ctx, "BML script mod summary: capabilities");
  }

  void OnProcess(const BML::ModContext &in ctx) {
    if (events is null || !events.IsOpen || loggedPoll)
      return;
    BML::Events::Event@ event;
    int status = events.Poll(event);
    Log(ctx, "BML IMC stream poll: status=" + status +
             " event=" + ((event is null) ? "none" : "record"));
    loggedPoll = true;
  }

  void OnUnload(const BML::ModContext &in ctx) {
    if (events !is null)
      events.Close();
    Log(ctx, "Goodbye!");
  }
}
