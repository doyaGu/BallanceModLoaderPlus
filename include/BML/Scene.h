#ifndef BML_SCENE_H
#define BML_SCENE_H
#include "BML/Generated/bml_scene_imc.hpp"
#include <string>
#include <utility>
namespace BML::Scene {
using ObjectRef = BML_ObjectRef; using Vec3 = BML_Vec3;
struct ObjectInfo { int Id = 0; std::string Name; int ClassId = 0; bool Visible = false; bool Dynamic = false; };
struct EntityTransform { Vec3 Position{}; Vec3 Scale{}; ObjectRef Parent{}; int ChildCount = 0; };
namespace Detail {
namespace Api = Imc::Generated::Bml::Scene;
inline Imc::LazyClient<Api::Client> &ClientState() { static Imc::LazyClient<Api::Client> state; return state; }
inline Api::Client &Client() { return ClientState().Get(); }
inline int RequireApi() { return ClientState().EnsureOpen(); }
}
inline int RequireApi() { return Detail::RequireApi(); }
inline int ReadObject(ObjectRef object, ObjectInfo &out) { Detail::Api::ObjectRequestValue input{}; input.Object = object; Detail::Api::ObjectInfoValue wire{}; int status = RequireApi(); if (status == BML_OK) status = Detail::Client().CallObject(input, wire); if (status == BML_OK) out = {wire.Id, std::move(wire.Name), wire.ClassId, wire.Visible, wire.Dynamic}; return status; }
inline int ReadEntityTransform(ObjectRef object, EntityTransform &out) { Detail::Api::ObjectRequestValue input{}; input.Object = object; Detail::Api::EntityTransformValue wire{}; int status = RequireApi(); if (status == BML_OK) status = Detail::Client().CallEntity(input, wire); if (status == BML_OK) out = {wire.Position, wire.Scale, wire.Parent, wire.ChildCount}; return status; }
inline int FindObject(const std::string &name, ObjectRef &out) { Detail::Api::FindNameRequestValue input{}; input.Name = name; Detail::Api::FindResultValue result{}; int status = RequireApi(); if (status == BML_OK) status = Detail::Client().CallFindName(input, result); if (status == BML_OK) out = result.Object; return status; }
inline int FindObject(const std::string &name, int classId, ObjectRef &out) { Detail::Api::FindNameClassRequestValue input{}; input.Name = name; input.ClassId = classId; Detail::Api::FindResultValue result{}; int status = RequireApi(); if (status == BML_OK) status = Detail::Client().CallFindNameClass(input, result); if (status == BML_OK) out = result.Object; return status; }
} // namespace BML::Scene
#endif // BML_SCENE_H
