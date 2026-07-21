// Generated Interop AngelScript binding. Do not edit by hand.
// Include this API binding from a mod. CreateApi() is for providers;
// the typed facade below is for consumers.

namespace BMLInteropGenerated {
namespace BmlScene {

::BML::Interop::ApiBuilder@ CreateApi() {
  ::BML::Interop::ApiBuilder@ api = ::BML::Interop::CreateApi(
      "bml.scene", 1, 0, uint64(0x4E6FC19797065751));
  if (api is null)
    return null;
  if (api.AddSchema(1, "object_info") != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 1, "id", ::BML::Interop::FIELD_INT, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 2, "name", ::BML::Interop::FIELD_STRING, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 3, "class_id", ::BML::Interop::FIELD_INT, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 4, "visible", ::BML::Interop::FIELD_BOOL, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(1, 5, "dynamic", ::BML::Interop::FIELD_BOOL, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddSchema(2, "entity_transform") != ::BML::ERROR_OK)
    return null;
  if (api.AddField(2, 1, "position", ::BML::Interop::FIELD_VEC3, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(2, 2, "scale", ::BML::Interop::FIELD_VEC3, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(2, 3, "parent", ::BML::Interop::FIELD_OBJECT, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(2, 4, "child_count", ::BML::Interop::FIELD_INT, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddSchema(3, "find_name_request") != ::BML::ERROR_OK)
    return null;
  if (api.AddField(3, 1, "name", ::BML::Interop::FIELD_STRING, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddSchema(4, "find_name_class_request") != ::BML::ERROR_OK)
    return null;
  if (api.AddField(4, 1, "name", ::BML::Interop::FIELD_STRING, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddField(4, 2, "class_id", ::BML::Interop::FIELD_INT, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddSchema(5, "find_result") != ::BML::ERROR_OK)
    return null;
  if (api.AddField(5, 1, "object", ::BML::Interop::FIELD_OBJECT, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddEndpoint("entity", ::BML::Interop::ENDPOINT_COMPONENT, 0, 2, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddEndpoint("find_name", ::BML::Interop::ENDPOINT_QUERY, 3, 5, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddEndpoint("find_name_class", ::BML::Interop::ENDPOINT_QUERY, 4, 5, false) != ::BML::ERROR_OK)
    return null;
  if (api.AddEndpoint("object", ::BML::Interop::ENDPOINT_COMPONENT, 0, 1, false) != ::BML::ERROR_OK)
    return null;
  return api;
}

// Typed consumer facade.  It is generated from this API and uses
// BML::Interop only as its private transport layer.
const uint Major = 1;
const uint64 Hash = uint64(0x4E6FC19797065751);

int Require() {
  return ::BML::Interop::RequireApi("bml.scene", Major, Hash);
}

class ObjectInfoValue {
  int Id;
  string Name;
  int ClassId;
  bool Visible;
  bool Dynamic;
}

int DecodeObjectInfo(::BML::Interop::Record@ record, ObjectInfoValue &out value) {
  if (record is null)
    return ::BML::ERROR_INTEROP_RECORD_INVALID;
  if (record.Schema != 1)
    return ::BML::ERROR_INTEROP_SCHEMA_MISMATCH;
  ObjectInfoValue decoded;
  int status = ::BML::ERROR_OK;
  if (status == ::BML::ERROR_OK) status = record.GetInt(1, decoded.Id);
  if (status == ::BML::ERROR_OK) status = record.GetString(2, decoded.Name);
  if (status == ::BML::ERROR_OK) status = record.GetInt(3, decoded.ClassId);
  if (status == ::BML::ERROR_OK) status = record.GetBool(4, decoded.Visible);
  if (status == ::BML::ERROR_OK) status = record.GetBool(5, decoded.Dynamic);
  if (status == ::BML::ERROR_OK)
    value = decoded;
  return status;
}

class EntityTransformValue {
  ::BML::Vec3 Position;
  ::BML::Vec3 Scale;
  ::BML::Interop::ObjectRef Parent;
  int ChildCount;
}

int DecodeEntityTransform(::BML::Interop::Record@ record, EntityTransformValue &out value) {
  if (record is null)
    return ::BML::ERROR_INTEROP_RECORD_INVALID;
  if (record.Schema != 2)
    return ::BML::ERROR_INTEROP_SCHEMA_MISMATCH;
  EntityTransformValue decoded;
  int status = ::BML::ERROR_OK;
  if (status == ::BML::ERROR_OK) status = record.GetVec3(1, decoded.Position);
  if (status == ::BML::ERROR_OK) status = record.GetVec3(2, decoded.Scale);
  if (status == ::BML::ERROR_OK) status = record.GetObject(3, decoded.Parent);
  if (status == ::BML::ERROR_OK) status = record.GetInt(4, decoded.ChildCount);
  if (status == ::BML::ERROR_OK)
    value = decoded;
  return status;
}

class FindNameRequestValue {
  string Name;
}

int DecodeFindNameRequest(::BML::Interop::Record@ record, FindNameRequestValue &out value) {
  if (record is null)
    return ::BML::ERROR_INTEROP_RECORD_INVALID;
  if (record.Schema != 3)
    return ::BML::ERROR_INTEROP_SCHEMA_MISMATCH;
  FindNameRequestValue decoded;
  int status = ::BML::ERROR_OK;
  if (status == ::BML::ERROR_OK) status = record.GetString(1, decoded.Name);
  if (status == ::BML::ERROR_OK)
    value = decoded;
  return status;
}

class FindNameClassRequestValue {
  string Name;
  int ClassId;
}

int DecodeFindNameClassRequest(::BML::Interop::Record@ record, FindNameClassRequestValue &out value) {
  if (record is null)
    return ::BML::ERROR_INTEROP_RECORD_INVALID;
  if (record.Schema != 4)
    return ::BML::ERROR_INTEROP_SCHEMA_MISMATCH;
  FindNameClassRequestValue decoded;
  int status = ::BML::ERROR_OK;
  if (status == ::BML::ERROR_OK) status = record.GetString(1, decoded.Name);
  if (status == ::BML::ERROR_OK) status = record.GetInt(2, decoded.ClassId);
  if (status == ::BML::ERROR_OK)
    value = decoded;
  return status;
}

class FindResultValue {
  ::BML::Interop::ObjectRef Object;
}

int DecodeFindResult(::BML::Interop::Record@ record, FindResultValue &out value) {
  if (record is null)
    return ::BML::ERROR_INTEROP_RECORD_INVALID;
  if (record.Schema != 5)
    return ::BML::ERROR_INTEROP_SCHEMA_MISMATCH;
  FindResultValue decoded;
  int status = ::BML::ERROR_OK;
  if (status == ::BML::ERROR_OK) status = record.GetObject(1, decoded.Object);
  if (status == ::BML::ERROR_OK)
    value = decoded;
  return status;
}

int EncodeFindNameRequest(const FindNameRequestValue &in value, ::BML::Interop::Input@ &out input) {
  int status = ::BML::Interop::CreateInput("bml.scene", 3, input);
  if (status == ::BML::ERROR_OK) status = input.SetString(1, value.Name);
  return status;
}

int EncodeFindNameClassRequest(const FindNameClassRequestValue &in value, ::BML::Interop::Input@ &out input) {
  int status = ::BML::Interop::CreateInput("bml.scene", 4, input);
  if (status == ::BML::ERROR_OK) status = input.SetString(1, value.Name);
  if (status == ::BML::ERROR_OK) status = input.SetInt(2, value.ClassId);
  return status;
}

int ReadEntity(const ::BML::Interop::ObjectRef &in object, EntityTransformValue &out value) {
  ::BML::Interop::Record@ record;
  int status = Require();
  if (status == ::BML::ERROR_OK) status = ::BML::Interop::ReadComponent("bml.scene", "entity", object, record);
  if (status == ::BML::ERROR_OK) status = DecodeEntityTransform(record, value);
  return status;
}

int QueryFindName(const FindNameRequestValue &in inputValue, FindResultValue &out value) {
  ::BML::Interop::Input@ input;
  int status = Require();
  if (status == ::BML::ERROR_OK) status = EncodeFindNameRequest(inputValue, input);
  ::BML::Interop::Record@ record;
  if (status == ::BML::ERROR_OK) status = ::BML::Interop::InvokeQuery("bml.scene", "find_name", input, record);
  if (status == ::BML::ERROR_OK) status = DecodeFindResult(record, value);
  return status;
}

int QueryFindNameClass(const FindNameClassRequestValue &in inputValue, FindResultValue &out value) {
  ::BML::Interop::Input@ input;
  int status = Require();
  if (status == ::BML::ERROR_OK) status = EncodeFindNameClassRequest(inputValue, input);
  ::BML::Interop::Record@ record;
  if (status == ::BML::ERROR_OK) status = ::BML::Interop::InvokeQuery("bml.scene", "find_name_class", input, record);
  if (status == ::BML::ERROR_OK) status = DecodeFindResult(record, value);
  return status;
}

int ReadObject(const ::BML::Interop::ObjectRef &in object, ObjectInfoValue &out value) {
  ::BML::Interop::Record@ record;
  int status = Require();
  if (status == ::BML::ERROR_OK) status = ::BML::Interop::ReadComponent("bml.scene", "object", object, record);
  if (status == ::BML::ERROR_OK) status = DecodeObjectInfo(record, value);
  return status;
}

} // namespace BmlScene
} // namespace BMLInteropGenerated
