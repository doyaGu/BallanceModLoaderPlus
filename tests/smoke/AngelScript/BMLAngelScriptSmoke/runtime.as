[bml.mod id="bml.bindings.smoke" name="BML Bindings Smoke" version="1.0.0" author="BML+" bml="0.3.13" description="Smoke test for BML's IMC-backed AngelScript facades."]
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
    BML::Runtime::State runtime;
    BML::Runtime::Clock clock;
    BML::Runtime::Score score;
    bool runtimeOk = BML::Runtime::ReadState(runtime) == BML::ERROR_OK &&
                     BML::Runtime::ReadClock(clock) == BML::ERROR_OK &&
                     BML::Runtime::ReadScore(score) == BML::ERROR_OK;
    bool streamOk = BML::Events::Open(events, 8) == BML::ERROR_OK && events !is null && events.IsOpen;
    Log(ctx, "BML IMC facade smoke: runtime=" + (runtimeOk ? "true" : "false") +
             " stream=" + (streamOk ? "true" : "false"));
    Log(ctx, "BML script mod summary: imc-facades");
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
