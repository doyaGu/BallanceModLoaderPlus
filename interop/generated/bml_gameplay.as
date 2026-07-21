// Generated Interop AngelScript binding. Do not edit by hand.
// Include this API binding from a mod. CreateApi() is for providers;
// the typed facade below is for consumers.

namespace BMLInteropGenerated {
namespace BmlGameplay {

::BML::Interop::ApiBuilder@ CreateApi() {
  ::BML::Interop::ApiBuilder@ api = ::BML::Interop::CreateApi(
      "bml.gameplay", 1, 0, uint64(0xF6A68405E0438B70));
  if (api is null)
    return null;
  if (api.AddSchema(1, "level_state") != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 1, "id", ::BML::Interop::FIELD_INT, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 2, "active_ball", ::BML::Interop::FIELD_OBJECT, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 3, "reset_matrix", ::BML::Interop::FIELD_MAT4, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 4, "points", ::BML::Interop::FIELD_INT, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddSchema(2, "energy_state") != ::BML::ERROR_OK)
    return null;
  if (api.AddField(2, 1, "points", ::BML::Interop::FIELD_INT, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(2, 2, "lives", ::BML::Interop::FIELD_INT, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(2, 3, "start_points", ::BML::Interop::FIELD_INT, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(2, 4, "start_lives", ::BML::Interop::FIELD_INT, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(2, 5, "time_factor", ::BML::Interop::FIELD_FLOAT, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(2, 6, "life_bonus", ::BML::Interop::FIELD_INT, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddSchema(3, "catalog_entry") != ::BML::ERROR_OK)
    return null;
  if (api.AddField(3, 1, "file", ::BML::Interop::FIELD_STRING, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(3, 2, "start_ball", ::BML::Interop::FIELD_STRING, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(3, 3, "sky", ::BML::Interop::FIELD_STRING, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(3, 4, "bonus", ::BML::Interop::FIELD_INT, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(3, 5, "music", ::BML::Interop::FIELD_INT, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddSchema(4, "checkpoint") != ::BML::ERROR_OK)
    return null;
  if (api.AddField(4, 1, "matrix", ::BML::Interop::FIELD_MAT4, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(4, 2, "object", ::BML::Interop::FIELD_OBJECT, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddSchema(5, "resetpoint") != ::BML::ERROR_OK)
    return null;
  if (api.AddField(5, 1, "object", ::BML::Interop::FIELD_OBJECT, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddEndpoint("catalog", ::BML::Interop::ENDPOINT_COLLECTION, 0, 3, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddEndpoint("checkpoints", ::BML::Interop::ENDPOINT_COLLECTION, 0, 4, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddEndpoint("energy", ::BML::Interop::ENDPOINT_RESOURCE, 0, 2, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddEndpoint("level", ::BML::Interop::ENDPOINT_RESOURCE, 0, 1, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddEndpoint("resetpoints", ::BML::Interop::ENDPOINT_COLLECTION, 0, 5, true) != ::BML::ERROR_OK)
    return null;
  return api;
}

// Typed consumer facade.  It is generated from this API and uses
// BML::Interop only as its private transport layer.
const uint Major = 1;
const uint64 Hash = uint64(0xF6A68405E0438B70);

int Require() {
  return ::BML::Interop::RequireApi("bml.gameplay", Major, Hash);
}

class LevelStateValue {
  int Id;
  ::BML::Interop::ObjectRef ActiveBall;
  ::BML::Mat4 ResetMatrix;
  int Points;
}

int DecodeLevelState(::BML::Interop::Record@ record, LevelStateValue &out value) {
  if (record is null)
    return ::BML::ERROR_INTEROP_RECORD_INVALID;
  if (record.Schema != 1)
    return ::BML::ERROR_INTEROP_SCHEMA_MISMATCH;
  LevelStateValue decoded;
  int status = ::BML::ERROR_OK;
  if (status == ::BML::ERROR_OK) status = record.GetInt(1, decoded.Id);
  if (status == ::BML::ERROR_OK) status = record.GetObject(2, decoded.ActiveBall);
  if (status == ::BML::ERROR_OK) status = record.GetMat4(3, decoded.ResetMatrix);
  if (status == ::BML::ERROR_OK) status = record.GetInt(4, decoded.Points);
  if (status == ::BML::ERROR_OK)
    value = decoded;
  return status;
}

class EnergyStateValue {
  int Points;
  int Lives;
  int StartPoints;
  int StartLives;
  float TimeFactor;
  int LifeBonus;
}

int DecodeEnergyState(::BML::Interop::Record@ record, EnergyStateValue &out value) {
  if (record is null)
    return ::BML::ERROR_INTEROP_RECORD_INVALID;
  if (record.Schema != 2)
    return ::BML::ERROR_INTEROP_SCHEMA_MISMATCH;
  EnergyStateValue decoded;
  int status = ::BML::ERROR_OK;
  if (status == ::BML::ERROR_OK) status = record.GetInt(1, decoded.Points);
  if (status == ::BML::ERROR_OK) status = record.GetInt(2, decoded.Lives);
  if (status == ::BML::ERROR_OK) status = record.GetInt(3, decoded.StartPoints);
  if (status == ::BML::ERROR_OK) status = record.GetInt(4, decoded.StartLives);
  if (status == ::BML::ERROR_OK) status = record.GetFloat(5, decoded.TimeFactor);
  if (status == ::BML::ERROR_OK) status = record.GetInt(6, decoded.LifeBonus);
  if (status == ::BML::ERROR_OK)
    value = decoded;
  return status;
}

class CatalogEntryValue {
  string File;
  string StartBall;
  string Sky;
  int Bonus;
  int Music;
}

int DecodeCatalogEntry(::BML::Interop::Record@ record, CatalogEntryValue &out value) {
  if (record is null)
    return ::BML::ERROR_INTEROP_RECORD_INVALID;
  if (record.Schema != 3)
    return ::BML::ERROR_INTEROP_SCHEMA_MISMATCH;
  CatalogEntryValue decoded;
  int status = ::BML::ERROR_OK;
  if (status == ::BML::ERROR_OK) status = record.GetString(1, decoded.File);
  if (status == ::BML::ERROR_OK) status = record.GetString(2, decoded.StartBall);
  if (status == ::BML::ERROR_OK) status = record.GetString(3, decoded.Sky);
  if (status == ::BML::ERROR_OK) status = record.GetInt(4, decoded.Bonus);
  if (status == ::BML::ERROR_OK) status = record.GetInt(5, decoded.Music);
  if (status == ::BML::ERROR_OK)
    value = decoded;
  return status;
}

class CheckpointValue {
  ::BML::Mat4 Matrix;
  ::BML::Interop::ObjectRef Object;
}

int DecodeCheckpoint(::BML::Interop::Record@ record, CheckpointValue &out value) {
  if (record is null)
    return ::BML::ERROR_INTEROP_RECORD_INVALID;
  if (record.Schema != 4)
    return ::BML::ERROR_INTEROP_SCHEMA_MISMATCH;
  CheckpointValue decoded;
  int status = ::BML::ERROR_OK;
  if (status == ::BML::ERROR_OK) status = record.GetMat4(1, decoded.Matrix);
  if (status == ::BML::ERROR_OK) status = record.GetObject(2, decoded.Object);
  if (status == ::BML::ERROR_OK)
    value = decoded;
  return status;
}

class ResetpointValue {
  ::BML::Interop::ObjectRef Object;
}

int DecodeResetpoint(::BML::Interop::Record@ record, ResetpointValue &out value) {
  if (record is null)
    return ::BML::ERROR_INTEROP_RECORD_INVALID;
  if (record.Schema != 5)
    return ::BML::ERROR_INTEROP_SCHEMA_MISMATCH;
  ResetpointValue decoded;
  int status = ::BML::ERROR_OK;
  if (status == ::BML::ERROR_OK) status = record.GetObject(1, decoded.Object);
  if (status == ::BML::ERROR_OK)
    value = decoded;
  return status;
}

int OpenCatalog(::BML::Interop::Cursor@ &out cursor) {
  const int status = Require();
  return status == ::BML::ERROR_OK ? ::BML::Interop::OpenCollection("bml.gameplay", "catalog", cursor) : status;
}

int NextCatalog(::BML::Interop::Cursor@ cursor, CatalogEntryValue &out value, bool &out hasValue, bool &out complete) {
  hasValue = false;
  complete = false;
  if (cursor is null)
    return ::BML::ERROR_INTEROP_CURSOR_STALE;
  ::BML::Interop::Record@ record;
  int status = cursor.Next(record, hasValue, complete);
  if (status == ::BML::ERROR_OK && hasValue) status = DecodeCatalogEntry(record, value);
  return status;
}

int OpenCheckpoints(::BML::Interop::Cursor@ &out cursor) {
  const int status = Require();
  return status == ::BML::ERROR_OK ? ::BML::Interop::OpenCollection("bml.gameplay", "checkpoints", cursor) : status;
}

int NextCheckpoints(::BML::Interop::Cursor@ cursor, CheckpointValue &out value, bool &out hasValue, bool &out complete) {
  hasValue = false;
  complete = false;
  if (cursor is null)
    return ::BML::ERROR_INTEROP_CURSOR_STALE;
  ::BML::Interop::Record@ record;
  int status = cursor.Next(record, hasValue, complete);
  if (status == ::BML::ERROR_OK && hasValue) status = DecodeCheckpoint(record, value);
  return status;
}

int ReadEnergy(EnergyStateValue &out value) {
  ::BML::Interop::Record@ record;
  int status = Require();
  if (status == ::BML::ERROR_OK) status = ::BML::Interop::ReadResource("bml.gameplay", "energy", record);
  if (status == ::BML::ERROR_OK) status = DecodeEnergyState(record, value);
  return status;
}

int ReadLevel(LevelStateValue &out value) {
  ::BML::Interop::Record@ record;
  int status = Require();
  if (status == ::BML::ERROR_OK) status = ::BML::Interop::ReadResource("bml.gameplay", "level", record);
  if (status == ::BML::ERROR_OK) status = DecodeLevelState(record, value);
  return status;
}

int OpenResetpoints(::BML::Interop::Cursor@ &out cursor) {
  const int status = Require();
  return status == ::BML::ERROR_OK ? ::BML::Interop::OpenCollection("bml.gameplay", "resetpoints", cursor) : status;
}

int NextResetpoints(::BML::Interop::Cursor@ cursor, ResetpointValue &out value, bool &out hasValue, bool &out complete) {
  hasValue = false;
  complete = false;
  if (cursor is null)
    return ::BML::ERROR_INTEROP_CURSOR_STALE;
  ::BML::Interop::Record@ record;
  int status = cursor.Next(record, hasValue, complete);
  if (status == ::BML::ERROR_OK && hasValue) status = DecodeResetpoint(record, value);
  return status;
}

} // namespace BmlGameplay
} // namespace BMLInteropGenerated
