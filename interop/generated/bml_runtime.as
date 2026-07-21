// Generated Interop AngelScript binding. Do not edit by hand.
// Include this API binding from a mod. CreateApi() is for providers;
// the typed facade below is for consumers.

namespace BMLInteropGenerated {
namespace BmlRuntime {

::BML::Interop::ApiBuilder@ CreateApi() {
  ::BML::Interop::ApiBuilder@ api = ::BML::Interop::CreateApi(
      "bml.runtime", 1, 0, uint64(0x7467626056A20F28));
  if (api is null)
    return null;
  if (api.AddSchema(1, "runtime_state") != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 1, "in_game", ::BML::Interop::FIELD_BOOL, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 2, "in_level", ::BML::Interop::FIELD_BOOL, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 3, "paused", ::BML::Interop::FIELD_BOOL, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 4, "playing", ::BML::Interop::FIELD_BOOL, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 5, "cheat_enabled", ::BML::Interop::FIELD_BOOL, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddSchema(2, "clock_state") != ::BML::ERROR_OK)
    return null;
  if (api.AddField(2, 1, "time_ms", ::BML::Interop::FIELD_FLOAT, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(2, 2, "absolute_ms", ::BML::Interop::FIELD_FLOAT, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(2, 3, "delta_ms", ::BML::Interop::FIELD_FLOAT, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(2, 4, "frame", ::BML::Interop::FIELD_INT, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddSchema(3, "score_state") != ::BML::ERROR_OK)
    return null;
  if (api.AddField(3, 1, "sr", ::BML::Interop::FIELD_FLOAT, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(3, 2, "hs", ::BML::Interop::FIELD_INT, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddEndpoint("clock", ::BML::Interop::ENDPOINT_RESOURCE, 0, 2, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddEndpoint("score", ::BML::Interop::ENDPOINT_RESOURCE, 0, 3, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddEndpoint("state", ::BML::Interop::ENDPOINT_RESOURCE, 0, 1, false) != ::BML::ERROR_OK)
    return null;
  return api;
}

// Typed consumer facade.  It is generated from this API and uses
// BML::Interop only as its private transport layer.
const uint Major = 1;
const uint64 Hash = uint64(0x7467626056A20F28);

int Require() {
  return ::BML::Interop::RequireApi("bml.runtime", Major, Hash);
}

class RuntimeStateValue {
  bool InGame;
  bool InLevel;
  bool Paused;
  bool Playing;
  bool CheatEnabled;
}

int DecodeRuntimeState(::BML::Interop::Record@ record, RuntimeStateValue &out value) {
  if (record is null)
    return ::BML::ERROR_INTEROP_RECORD_INVALID;
  if (record.Schema != 1)
    return ::BML::ERROR_INTEROP_SCHEMA_MISMATCH;
  RuntimeStateValue decoded;
  int status = ::BML::ERROR_OK;
  if (status == ::BML::ERROR_OK) status = record.GetBool(1, decoded.InGame);
  if (status == ::BML::ERROR_OK) status = record.GetBool(2, decoded.InLevel);
  if (status == ::BML::ERROR_OK) status = record.GetBool(3, decoded.Paused);
  if (status == ::BML::ERROR_OK) status = record.GetBool(4, decoded.Playing);
  if (status == ::BML::ERROR_OK) status = record.GetBool(5, decoded.CheatEnabled);
  if (status == ::BML::ERROR_OK)
    value = decoded;
  return status;
}

class ClockStateValue {
  float TimeMs;
  float AbsoluteMs;
  float DeltaMs;
  int Frame;
}

int DecodeClockState(::BML::Interop::Record@ record, ClockStateValue &out value) {
  if (record is null)
    return ::BML::ERROR_INTEROP_RECORD_INVALID;
  if (record.Schema != 2)
    return ::BML::ERROR_INTEROP_SCHEMA_MISMATCH;
  ClockStateValue decoded;
  int status = ::BML::ERROR_OK;
  if (status == ::BML::ERROR_OK) status = record.GetFloat(1, decoded.TimeMs);
  if (status == ::BML::ERROR_OK) status = record.GetFloat(2, decoded.AbsoluteMs);
  if (status == ::BML::ERROR_OK) status = record.GetFloat(3, decoded.DeltaMs);
  if (status == ::BML::ERROR_OK) status = record.GetInt(4, decoded.Frame);
  if (status == ::BML::ERROR_OK)
    value = decoded;
  return status;
}

class ScoreStateValue {
  float Sr;
  int Hs;
}

int DecodeScoreState(::BML::Interop::Record@ record, ScoreStateValue &out value) {
  if (record is null)
    return ::BML::ERROR_INTEROP_RECORD_INVALID;
  if (record.Schema != 3)
    return ::BML::ERROR_INTEROP_SCHEMA_MISMATCH;
  ScoreStateValue decoded;
  int status = ::BML::ERROR_OK;
  if (status == ::BML::ERROR_OK) status = record.GetFloat(1, decoded.Sr);
  if (status == ::BML::ERROR_OK) status = record.GetInt(2, decoded.Hs);
  if (status == ::BML::ERROR_OK)
    value = decoded;
  return status;
}

int ReadClock(ClockStateValue &out value) {
  ::BML::Interop::Record@ record;
  int status = Require();
  if (status == ::BML::ERROR_OK) status = ::BML::Interop::ReadResource("bml.runtime", "clock", record);
  if (status == ::BML::ERROR_OK) status = DecodeClockState(record, value);
  return status;
}

int ReadScore(ScoreStateValue &out value) {
  ::BML::Interop::Record@ record;
  int status = Require();
  if (status == ::BML::ERROR_OK) status = ::BML::Interop::ReadResource("bml.runtime", "score", record);
  if (status == ::BML::ERROR_OK) status = DecodeScoreState(record, value);
  return status;
}

int ReadState(RuntimeStateValue &out value) {
  ::BML::Interop::Record@ record;
  int status = Require();
  if (status == ::BML::ERROR_OK) status = ::BML::Interop::ReadResource("bml.runtime", "state", record);
  if (status == ::BML::ERROR_OK) status = DecodeRuntimeState(record, value);
  return status;
}

} // namespace BmlRuntime
} // namespace BMLInteropGenerated
