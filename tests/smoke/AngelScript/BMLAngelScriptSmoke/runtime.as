[bml.mod id="bml.bindings.smoke" name="BML Bindings Smoke" version="1.0.0" author="BML+" bml="0.3.13" description="Smoke test for BML's API-based AngelScript Interop."]
[bml.require id="BML" version="0.3.13"]
[bml.require id="bml.native.interop.smoke" version="1.0.0"]

int ReadInteropSmoke(const BML::Interop::Request &in request,
                     BML::Interop::RecordWriter@ writer) {
  int status = writer.SetInt(1, 42);
  array<int> numbers = { 1, 2, 3 };
  if (status == BML::ERROR_OK) status = writer.SetIntArray(2, numbers);
  array<string> names = { "alpha", "beta" };
  if (status == BML::ERROR_OK) status = writer.SetStringArray(3, names);
  BML::Vec3 point;
  point.x = 1.0f; point.y = 2.0f; point.z = 3.0f;
  array<BML::Vec3> points = { point };
  if (status == BML::ERROR_OK) status = writer.SetVec3Array(4, points);
  BML::Mat4 matrix;
  matrix.m00 = 1.0f; matrix.m11 = 1.0f; matrix.m22 = 1.0f; matrix.m33 = 1.0f;
  array<BML::Mat4> matrices = { matrix };
  if (status == BML::ERROR_OK) status = writer.SetMat4Array(5, matrices);
  return status;
}

bool ConsumeNativeInteropSmoke() {
  if (BML::Interop::RequireApi("bml.smoke.api", 1, uint64(0x5467F4E62C5ADA4D)) != BML::ERROR_OK)
    return false;
  BML::Interop::Record@ record;
  int status = BML::Interop::ReadResource("bml.smoke.api", "state", record);
  int value = 0;
  if (status == BML::ERROR_OK && record !is null)
    status = record.GetInt(1, value);
  return status == BML::ERROR_OK && record !is null && record.Schema == 1 && value == 42;
}

class BMLBindingsSmokeMod {
  BML::Events::Stream@ events;
  bool loggedPoll = false;

  bool RegisterProviderSmoke() {
    BML::Interop::ApiBuilder@ api = BML::Interop::CreateApi(
        "bml.smoke.script", 1, 0, uint64(0xD1410B5393DCA38A));
    if (api is null ||
        api.AddSchema(1, "state") != BML::ERROR_OK ||
        api.AddField(1, 1, "value", BML::Interop::FIELD_INT) != BML::ERROR_OK ||
        api.AddField(1, 2, "numbers", BML::Interop::FIELD_INT_ARRAY) != BML::ERROR_OK ||
        api.AddField(1, 3, "names", BML::Interop::FIELD_STRING_ARRAY) != BML::ERROR_OK ||
        api.AddField(1, 4, "points", BML::Interop::FIELD_VEC3_ARRAY) != BML::ERROR_OK ||
        api.AddField(1, 5, "matrices", BML::Interop::FIELD_MAT4_ARRAY) != BML::ERROR_OK ||
        api.AddEndpoint("state", BML::Interop::ENDPOINT_RESOURCE, 0, 1) != BML::ERROR_OK) {
      return false;
    }

    BML::Interop::Provider@ provider = BML::Interop::CreateProvider();
    if (provider is null || provider.SetRead("state", ReadInteropSmoke) != BML::ERROR_OK)
      return false;
    return BML::Interop::RegisterProvider(api, provider) == BML::ERROR_OK && provider.IsRegistered;
  }

  void OnLoad(const BML::ModContext &in ctx) {
    BML::Runtime::State runtime;
    BML::Runtime::Clock clock;
    BML::Runtime::Score score;
    bool runtimeOk = BML::Runtime::ReadState(runtime) == BML::ERROR_OK &&
                     BML::Runtime::ReadClock(clock) == BML::ERROR_OK &&
                     BML::Runtime::ReadScore(score) == BML::ERROR_OK;
    bool streamOk = BML::Events::Open(events, 8) == BML::ERROR_OK && events !is null && events.IsOpen;
    bool providerOk = RegisterProviderSmoke();
    bool nativeOk = ConsumeNativeInteropSmoke();
    LogInfo(ctx, "BML interop smoke: runtime=" + (runtimeOk ? "true" : "false") +
                 " stream=" + (streamOk ? "true" : "false") +
                 " provider=" + (providerOk ? "true" : "false") +
                 " native=" + (nativeOk ? "true" : "false"));
    LogInfo(ctx, "BML script mod summary: api-interop");
  }

  void OnProcess(const BML::ModContext &in ctx) {
    if (events is null || !events.IsOpen || loggedPoll)
      return;
    BML::Events::Event@ event;
    int status = events.Poll(event);
    LogInfo(ctx, "BML interop stream poll: status=" + status +
                 " event=" + ((event is null) ? "none" : "record"));
    loggedPoll = true;
  }

  void OnUnload(const BML::ModContext &in ctx) {
    if (events !is null)
      events.Close();
    LogInfo(ctx, "Goodbye!");
  }
}
