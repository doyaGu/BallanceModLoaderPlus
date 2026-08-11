// Reads scene objects over IMC. This is a thin inline wrapper around the
// generated bml.scene client, so including it costs nothing at link time.
//
// Status handling, first-call client opening, and the thread rules are the same
// as in Runtime.h: BML_OK means the out parameter was written, the main thread
// never blocks, and off-thread calls time out after 5000 ms.
//
// An ObjectRef is not a CKObject pointer. It is a slot plus a generation counter
// the loader hands out, and it is deliberately unforgeable, so a reference to a
// deleted object fails cleanly with BML_ERROR_IMC_OBJECT_INVALID instead of
// resolving to freed memory. A ref with Domain == 0 is null.
//
// Watch the two distinct failure modes on the Find functions: a name that
// matches nothing still returns BML_OK and writes a null ref, because "absent"
// is an answer rather than an error. Check out.Domain, not just the status.
#ifndef BML_SCENE_H
#define BML_SCENE_H

#include "BML/Generated/bml_scene_imc.hpp"

#include <string>
#include <utility>

namespace BML::Scene {

using ObjectRef = BML_ObjectRef;
using Vec3 = BML_Vec3;

// Id is the CK_ID; ClassId is the CK_CLASSID.
struct ObjectInfo {
    int Id = 0;
    std::string Name;
    int ClassId = 0;
    bool Visible = false;
    bool Dynamic = false;
};

// Position and Scale are local to the parent, matching CK3dEntity::GetPosition
// and GetScale with no reference frame argument. Parent is null at the root.
struct EntityTransform {
    Vec3 Position{};
    Vec3 Scale{};
    ObjectRef Parent{};
    int ChildCount = 0;
};

namespace Detail {

namespace Api = Imc::Generated::Bml::Scene;

inline Imc::LazyClient<Api::Client> &ClientState() {
    static Imc::LazyClient<Api::Client> state;
    return state;
}

inline Api::Client &Client() { return ClientState().Get(); }

[[nodiscard]] inline int RequireApi() { return ClientState().EnsureOpen(); }

} // namespace Detail

// Opens the client if it is not open yet. The functions below already do this,
// so call it directly only to probe whether the routes exist.
[[nodiscard]] inline int RequireApi() { return Detail::RequireApi(); }

[[nodiscard]] inline int ReadObject(ObjectRef object, ObjectInfo &out) {
    Detail::Api::ObjectRequestValue input{};
    input.Object = object;
    Detail::Api::ObjectInfoValue wire{};
    int status = RequireApi();
    if (status == BML_OK)
        status = Detail::Client().CallObject(input, wire);
    if (status == BML_OK)
        out = {wire.Id, std::move(wire.Name), wire.ClassId, wire.Visible, wire.Dynamic};
    return status;
}

// Returns BML_ERROR_IMC_OBJECT_INVALID when the reference is stale and also when
// it resolves to an object that is not a CK3dEntity.
[[nodiscard]] inline int ReadEntityTransform(ObjectRef object, EntityTransform &out) {
    Detail::Api::ObjectRequestValue input{};
    input.Object = object;
    Detail::Api::EntityTransformValue wire{};
    int status = RequireApi();
    if (status == BML_OK)
        status = Detail::Client().CallEntity(input, wire);
    if (status == BML_OK)
        out = {wire.Position, wire.Scale, wire.Parent, wire.ChildCount};
    return status;
}

// Looks up by name across every class, like CKContext::GetObjectByName. Writes a
// null ref and still returns BML_OK when nothing matches.
[[nodiscard]] inline int FindObject(const std::string &name, ObjectRef &out) {
    Detail::Api::FindNameRequestValue input{};
    input.Name = name;
    Detail::Api::FindResultValue result{};
    int status = RequireApi();
    if (status == BML_OK)
        status = Detail::Client().CallFindName(input, result);
    if (status == BML_OK)
        out = result.Object;
    return status;
}

// classId is a CK_CLASSID. This is the IMC counterpart of the IBML GetXxxByName
// family: pass CKCID_3DOBJECT here instead of calling IBML::Get3dObjectByName.
// Writes a null ref and still returns BML_OK when nothing matches.
[[nodiscard]] inline int FindObject(const std::string &name, int classId, ObjectRef &out) {
    Detail::Api::FindNameClassRequestValue input{};
    input.Name = name;
    input.ClassId = classId;
    Detail::Api::FindResultValue result{};
    int status = RequireApi();
    if (status == BML_OK)
        status = Detail::Client().CallFindNameClass(input, result);
    if (status == BML_OK)
        out = result.Object;
    return status;
}

} // namespace BML::Scene

#endif // BML_SCENE_H
