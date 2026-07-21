#ifndef BML_SCENE_H
#define BML_SCENE_H

#include "BML/Generated/bml_scene_api.h"
#include "BML/InteropClient.h"

#include <string>

namespace BML::Scene {

using ObjectRef = Interop::ObjectRef;
using Vec3 = Interop::Vec3;

struct ObjectInfo {
    int Id = 0;
    std::string Name;
    int ClassId = 0;
    bool Visible = false;
    bool Dynamic = false;
};

struct EntityTransform {
    Vec3 Position{};
    Vec3 Scale{};
    ObjectRef Parent{};
    int ChildCount = 0;
};

inline int RequireApi() {
    return Interop::RequireApi(Interop::Generated::Bml::Scene::Descriptor);
}

inline int ReadObject(ObjectRef object, ObjectInfo &out) {
    Interop::Record record;
    int status = RequireApi();
    if (status == BML_OK) status = Interop::ReadComponent(Interop::Generated::Bml::Scene::ApiId, "object", object, record);
    ObjectInfo value{};
    if (status == BML_OK) status = record.GetInt(Interop::Generated::Bml::Scene::ObjectInfoField::Id, value.Id);
    if (status == BML_OK) status = record.GetString(Interop::Generated::Bml::Scene::ObjectInfoField::Name, value.Name);
    if (status == BML_OK) status = record.GetInt(Interop::Generated::Bml::Scene::ObjectInfoField::ClassId, value.ClassId);
    if (status == BML_OK) status = record.GetBool(Interop::Generated::Bml::Scene::ObjectInfoField::Visible, value.Visible);
    if (status == BML_OK) status = record.GetBool(Interop::Generated::Bml::Scene::ObjectInfoField::Dynamic, value.Dynamic);
    if (status == BML_OK) out = std::move(value);
    return status;
}

inline int ReadEntityTransform(ObjectRef object, EntityTransform &out) {
    Interop::Record record;
    int status = RequireApi();
    if (status == BML_OK) status = Interop::ReadComponent(Interop::Generated::Bml::Scene::ApiId, "entity", object, record);
    EntityTransform value{};
    if (status == BML_OK) status = record.GetVec3(Interop::Generated::Bml::Scene::EntityTransformField::Position, value.Position);
    if (status == BML_OK) status = record.GetVec3(Interop::Generated::Bml::Scene::EntityTransformField::Scale, value.Scale);
    if (status == BML_OK) status = record.GetObjectRef(Interop::Generated::Bml::Scene::EntityTransformField::Parent, value.Parent);
    if (status == BML_OK) status = record.GetInt(Interop::Generated::Bml::Scene::EntityTransformField::ChildCount, value.ChildCount);
    if (status == BML_OK) out = value;
    return status;
}

inline int FindObject(const std::string &name, ObjectRef &out) {
    Interop::Detail::InputRecord input;
    int status = RequireApi();
    if (status == BML_OK) status = input.Create(Interop::Generated::Bml::Scene::ApiId, Interop::Generated::Bml::Scene::FindNameRequest.Id);
    if (status == BML_OK) status = input.SetString(Interop::Generated::Bml::Scene::FindNameRequestField::Name, name);
    Interop::Record record;
    if (status == BML_OK) status = Interop::Detail::InvokeQuery(Interop::Generated::Bml::Scene::ApiId, "find_name", input, record);
    ObjectRef value{};
    if (status == BML_OK) status = record.GetObjectRef(Interop::Generated::Bml::Scene::FindResultField::Object, value);
    if (status == BML_OK) out = value;
    return status;
}

inline int FindObject(const std::string &name, int classId, ObjectRef &out) {
    Interop::Detail::InputRecord input;
    int status = RequireApi();
    if (status == BML_OK) status = input.Create(Interop::Generated::Bml::Scene::ApiId, Interop::Generated::Bml::Scene::FindNameClassRequest.Id);
    if (status == BML_OK) status = input.SetString(Interop::Generated::Bml::Scene::FindNameClassRequestField::Name, name);
    if (status == BML_OK) status = input.SetInt(Interop::Generated::Bml::Scene::FindNameClassRequestField::ClassId, classId);
    Interop::Record record;
    if (status == BML_OK) status = Interop::Detail::InvokeQuery(Interop::Generated::Bml::Scene::ApiId, "find_name_class", input, record);
    ObjectRef value{};
    if (status == BML_OK) status = record.GetObjectRef(Interop::Generated::Bml::Scene::FindResultField::Object, value);
    if (status == BML_OK) out = value;
    return status;
}

} // namespace BML::Scene

#endif // BML_SCENE_H
