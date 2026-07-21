// Generated Interop AngelScript binding. Do not edit by hand.
// Include this API binding from a mod. CreateApi() is for providers;
// the typed facade below is for consumers.

namespace BMLInteropGenerated {
namespace BmlEvents {

::BML::Interop::ApiBuilder@ CreateApi() {
  ::BML::Interop::ApiBuilder@ api = ::BML::Interop::CreateApi(
      "bml.events", 1, 0, uint64(0x671750F9EA1C375C));
  if (api is null)
    return null;
  if (api.AddSchema(1, "event") != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 1, "kind", ::BML::Interop::FIELD_INT, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 2, "filename", ::BML::Interop::FIELD_STRING, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 3, "is_map", ::BML::Interop::FIELD_BOOL, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 4, "master_name", ::BML::Interop::FIELD_STRING, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 5, "filter_class", ::BML::Interop::FIELD_INT, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 6, "add_to_scene", ::BML::Interop::FIELD_BOOL, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 7, "reuse_meshes", ::BML::Interop::FIELD_BOOL, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 8, "reuse_materials", ::BML::Interop::FIELD_BOOL, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 9, "dynamic", ::BML::Interop::FIELD_BOOL, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 10, "object_ids", ::BML::Interop::FIELD_OBJECT_ARRAY, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 11, "master_object", ::BML::Interop::FIELD_OBJECT, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 12, "script", ::BML::Interop::FIELD_OBJECT, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 13, "target", ::BML::Interop::FIELD_OBJECT, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 14, "fixed", ::BML::Interop::FIELD_BOOL, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 15, "friction", ::BML::Interop::FIELD_FLOAT, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 16, "elasticity", ::BML::Interop::FIELD_FLOAT, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 17, "mass", ::BML::Interop::FIELD_FLOAT, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 18, "collision_group", ::BML::Interop::FIELD_STRING, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 19, "start_frozen", ::BML::Interop::FIELD_BOOL, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 20, "enable_collision", ::BML::Interop::FIELD_BOOL, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 21, "auto_calculate_mass_center", ::BML::Interop::FIELD_BOOL, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 22, "linear_damp", ::BML::Interop::FIELD_FLOAT, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 23, "rot_damp", ::BML::Interop::FIELD_FLOAT, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 24, "collision_surface", ::BML::Interop::FIELD_STRING, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 25, "mass_center", ::BML::Interop::FIELD_VEC3, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 26, "convex_meshes", ::BML::Interop::FIELD_OBJECT_ARRAY, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 27, "ball_centers", ::BML::Interop::FIELD_VEC3_ARRAY, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 28, "ball_radii", ::BML::Interop::FIELD_FLOAT_ARRAY, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 29, "concave_meshes", ::BML::Interop::FIELD_OBJECT_ARRAY, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 30, "command", ::BML::Interop::FIELD_STRING, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 31, "command_args", ::BML::Interop::FIELD_STRING_ARRAY, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 32, "config_category", ::BML::Interop::FIELD_STRING, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 33, "config_key", ::BML::Interop::FIELD_STRING, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 34, "config_type", ::BML::Interop::FIELD_INT, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 35, "config_value", ::BML::Interop::FIELD_STRING, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 36, "cheat_enabled", ::BML::Interop::FIELD_BOOL, true) != ::BML::ERROR_OK)
    return null;
  if (api.AddEndpoint("all", ::BML::Interop::ENDPOINT_STREAM, 0, 1, false) != ::BML::ERROR_OK)
    return null;
  return api;
}

// Typed consumer facade.  It is generated from this API and uses
// BML::Interop only as its private transport layer.
const uint Major = 1;
const uint64 Hash = uint64(0x671750F9EA1C375C);

int Require() {
  return ::BML::Interop::RequireApi("bml.events", Major, Hash);
}

class EventValue {
  int Kind;
  bool HasFilename = false;
  string Filename;
  bool HasIsMap = false;
  bool IsMap;
  bool HasMasterName = false;
  string MasterName;
  bool HasFilterClass = false;
  int FilterClass;
  bool HasAddToScene = false;
  bool AddToScene;
  bool HasReuseMeshes = false;
  bool ReuseMeshes;
  bool HasReuseMaterials = false;
  bool ReuseMaterials;
  bool HasDynamic = false;
  bool Dynamic;
  bool HasObjectIds = false;
  array<::BML::Interop::ObjectRef> ObjectIds;
  bool HasMasterObject = false;
  ::BML::Interop::ObjectRef MasterObject;
  bool HasScript = false;
  ::BML::Interop::ObjectRef Script;
  bool HasTarget = false;
  ::BML::Interop::ObjectRef Target;
  bool HasFixed = false;
  bool Fixed;
  bool HasFriction = false;
  float Friction;
  bool HasElasticity = false;
  float Elasticity;
  bool HasMass = false;
  float Mass;
  bool HasCollisionGroup = false;
  string CollisionGroup;
  bool HasStartFrozen = false;
  bool StartFrozen;
  bool HasEnableCollision = false;
  bool EnableCollision;
  bool HasAutoCalculateMassCenter = false;
  bool AutoCalculateMassCenter;
  bool HasLinearDamp = false;
  float LinearDamp;
  bool HasRotDamp = false;
  float RotDamp;
  bool HasCollisionSurface = false;
  string CollisionSurface;
  bool HasMassCenter = false;
  ::BML::Vec3 MassCenter;
  bool HasConvexMeshes = false;
  array<::BML::Interop::ObjectRef> ConvexMeshes;
  bool HasBallCenters = false;
  array<::BML::Vec3> BallCenters;
  bool HasBallRadii = false;
  array<float> BallRadii;
  bool HasConcaveMeshes = false;
  array<::BML::Interop::ObjectRef> ConcaveMeshes;
  bool HasCommand = false;
  string Command;
  bool HasCommandArgs = false;
  array<string> CommandArgs;
  bool HasConfigCategory = false;
  string ConfigCategory;
  bool HasConfigKey = false;
  string ConfigKey;
  bool HasConfigType = false;
  int ConfigType;
  bool HasConfigValue = false;
  string ConfigValue;
  bool HasCheatEnabled = false;
  bool CheatEnabled;
}

int DecodeEvent(::BML::Interop::Record@ record, EventValue &out value) {
  if (record is null)
    return ::BML::ERROR_INTEROP_RECORD_INVALID;
  if (record.Schema != 1)
    return ::BML::ERROR_INTEROP_SCHEMA_MISMATCH;
  EventValue decoded;
  int status = ::BML::ERROR_OK;
  if (status == ::BML::ERROR_OK) status = record.GetInt(1, decoded.Kind);
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetString(2, decoded.Filename);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasFilename = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasFilename = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetBool(3, decoded.IsMap);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasIsMap = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasIsMap = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetString(4, decoded.MasterName);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasMasterName = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasMasterName = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetInt(5, decoded.FilterClass);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasFilterClass = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasFilterClass = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetBool(6, decoded.AddToScene);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasAddToScene = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasAddToScene = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetBool(7, decoded.ReuseMeshes);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasReuseMeshes = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasReuseMeshes = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetBool(8, decoded.ReuseMaterials);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasReuseMaterials = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasReuseMaterials = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetBool(9, decoded.Dynamic);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasDynamic = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasDynamic = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetObjectArray(10, decoded.ObjectIds);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasObjectIds = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasObjectIds = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetObject(11, decoded.MasterObject);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasMasterObject = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasMasterObject = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetObject(12, decoded.Script);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasScript = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasScript = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetObject(13, decoded.Target);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasTarget = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasTarget = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetBool(14, decoded.Fixed);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasFixed = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasFixed = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetFloat(15, decoded.Friction);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasFriction = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasFriction = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetFloat(16, decoded.Elasticity);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasElasticity = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasElasticity = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetFloat(17, decoded.Mass);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasMass = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasMass = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetString(18, decoded.CollisionGroup);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasCollisionGroup = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasCollisionGroup = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetBool(19, decoded.StartFrozen);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasStartFrozen = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasStartFrozen = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetBool(20, decoded.EnableCollision);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasEnableCollision = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasEnableCollision = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetBool(21, decoded.AutoCalculateMassCenter);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasAutoCalculateMassCenter = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasAutoCalculateMassCenter = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetFloat(22, decoded.LinearDamp);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasLinearDamp = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasLinearDamp = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetFloat(23, decoded.RotDamp);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasRotDamp = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasRotDamp = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetString(24, decoded.CollisionSurface);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasCollisionSurface = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasCollisionSurface = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetVec3(25, decoded.MassCenter);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasMassCenter = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasMassCenter = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetObjectArray(26, decoded.ConvexMeshes);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasConvexMeshes = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasConvexMeshes = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetVec3Array(27, decoded.BallCenters);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasBallCenters = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasBallCenters = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetFloatArray(28, decoded.BallRadii);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasBallRadii = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasBallRadii = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetObjectArray(29, decoded.ConcaveMeshes);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasConcaveMeshes = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasConcaveMeshes = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetString(30, decoded.Command);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasCommand = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasCommand = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetStringArray(31, decoded.CommandArgs);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasCommandArgs = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasCommandArgs = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetString(32, decoded.ConfigCategory);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasConfigCategory = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasConfigCategory = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetString(33, decoded.ConfigKey);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasConfigKey = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasConfigKey = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetInt(34, decoded.ConfigType);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasConfigType = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasConfigType = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetString(35, decoded.ConfigValue);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasConfigValue = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasConfigValue = true;
    }
  }
  if (status == ::BML::ERROR_OK) {
    const int fieldStatus = record.GetBool(36, decoded.CheatEnabled);
    if (fieldStatus == ::BML::ERROR_NOT_FOUND) {
      decoded.HasCheatEnabled = false;
    } else if (fieldStatus != ::BML::ERROR_OK) {
      status = fieldStatus;
    } else {
      decoded.HasCheatEnabled = true;
    }
  }
  if (status == ::BML::ERROR_OK)
    value = decoded;
  return status;
}

int OpenAll(::BML::Interop::Stream@ &out stream, int capacity = 256) {
  const int status = Require();
  return status == ::BML::ERROR_OK ? ::BML::Interop::OpenStream("bml.events", "all", stream, capacity) : status;
}

int PollAll(::BML::Interop::Stream@ stream, EventValue &out value, bool &out hasValue) {
  hasValue = false;
  if (stream is null)
    return ::BML::ERROR_INTEROP_HANDLE_STALE;
  ::BML::Interop::Record@ record;
  int status = stream.Poll(record);
  hasValue = record !is null;
  if (status == ::BML::ERROR_OK && hasValue) status = DecodeEvent(record, value);
  return status;
}

} // namespace BmlEvents
} // namespace BMLInteropGenerated
