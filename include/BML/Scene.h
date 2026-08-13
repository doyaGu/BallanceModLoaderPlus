// Reads the game's objects. The interface struct below is what the loader hands
// out and what a Mod written in C uses directly; the inline C++ namespace under it
// is the same thing with the lookup and the checks folded in, so including this
// header still costs nothing at link time.
//
// Interface.h explains the header, the version rules, BML_IFACE_HAS, and how a
// name is written into a fixed-capacity buffer. Every function here runs on the
// calling thread and touches Virtools objects, so every one of them answers
// BML_ERROR_WRONG_THREAD when called from any other thread than the game thread.
//
// Each read fills its out parameter only when it answers BML_OK, and answers
// BML_ERROR_INVALID_PARAMETER for a null argument or BML_ERROR_FAIL before the
// loader has loaded its mods.
//
// An ObjectRef is not a CKObject pointer. It is a slot plus a generation counter
// the loader hands out, described in Types.h, and it is deliberately unforgeable,
// so a reference to a deleted object fails cleanly with BML_ERROR_OBJECT_INVALID
// instead of resolving to freed memory. A ref with Domain == 0 is null.
//
// Watch the two distinct failure modes on the Find functions: a name that matches
// nothing still returns BML_OK and writes a null ref, because "absent" is an
// answer rather than an error. Check out.Domain, not just the status.
#ifndef BML_SCENE_H
#define BML_SCENE_H

#include "BML/Interface.h"
#include "BML/Types.h"

BML_BEGIN_CDECLS

#define BML_SCENE_INTERFACE_ID "bml.scene"
#define BML_SCENE_INTERFACE_MAJOR 1
#define BML_SCENE_INTERFACE_MINOR 0

// Capacity of the Name buffer below, terminator included.
#define BML_SCENE_NAME_CAPACITY 128u

// Id is the CK_ID and ClassId is the CK_CLASSID. Name is always terminated and
// holds at most BML_SCENE_NAME_CAPACITY - 1 characters; NameLength is how long the
// object's name actually is, so a name longer than that is detectable rather than
// silently short. Visible and Dynamic are 0 or 1.
//
// These are C structs and have no default member initializers: zero them with
// BML_SceneObjectInfo info = {0} in C, or with ObjectInfo info{} in C++.
typedef struct BML_SceneObjectInfo {
    int Id;
    int ClassId;
    char Name[BML_SCENE_NAME_CAPACITY];
    int NameLength;
    int Visible;
    int Dynamic;
} BML_SceneObjectInfo;

// Position and Scale are local to the parent, matching CK3dEntity::GetPosition and
// GetScale with no reference frame argument. Parent is null at the root.
typedef struct BML_SceneEntityTransform {
    BML_Vec3 Position;
    BML_Vec3 Scale;
    BML_ObjectRef Parent;
    int ChildCount;
} BML_SceneEntityTransform;

typedef struct BML_SceneInterface {
    BML_InterfaceHeader Header;

    // Answers BML_ERROR_OBJECT_INVALID when the reference is stale.
    int (*ReadObject)(BML_ObjectRef object, BML_SceneObjectInfo *out);

    // Answers BML_ERROR_OBJECT_INVALID when the reference is stale and also when it
    // resolves to an object that is not a CK3dEntity.
    int (*ReadEntityTransform)(BML_ObjectRef object, BML_SceneEntityTransform *out);

    // Looks up by name across every class, like CKContext::GetObjectByName. Writes
    // a null ref and still answers BML_OK when nothing matches.
    int (*FindObject)(const char *name, BML_ObjectRef *out);

    // classId is a CK_CLASSID. This is the counterpart of the IBML GetXxxByName
    // family: pass CKCID_3DOBJECT here instead of calling IBML::Get3dObjectByName.
    // Writes a null ref and still answers BML_OK when nothing matches.
    int (*FindObjectOfClass)(const char *name, int classId, BML_ObjectRef *out);
} BML_SceneInterface;

BML_END_CDECLS

#ifdef __cplusplus

#include <string>

namespace BML::Scene {

using ObjectRef = BML_ObjectRef;
using Vec3 = BML_Vec3;
using EntityTransform = BML_SceneEntityTransform;

// The C++ shape of BML_SceneObjectInfo. Name is the text that fit in the C
// buffer; NameTruncated reports whether the object's full name was longer. Id is
// the CK_ID and ClassId is the CK_CLASSID.
struct ObjectInfo {
    int Id = 0;
    std::string Name;
    int ClassId = 0;
    bool Visible = false;
    bool Dynamic = false;
    bool NameTruncated = false;
};

namespace Detail {

// Looked up once per Mod. The loader's table is static, so a null answer means
// the running loader does not carry this interface at all, which is not something
// that can change later in the process.
inline const BML_SceneInterface *Interface() {
    static const BML_SceneInterface *found =
        FindInterface<BML_SceneInterface>(BML_SCENE_INTERFACE_ID, BML_SCENE_INTERFACE_MAJOR);
    return found;
}

} // namespace Detail

// Whether the running loader carries this interface. The functions below check
// for themselves, so call this directly only to probe.
[[nodiscard]] inline int RequireApi() {
    return Detail::Interface() != nullptr ? BML_OK : BML_ERROR_NOT_FOUND;
}

[[nodiscard]] inline int ReadObject(ObjectRef object, ObjectInfo &out) {
    const BML_SceneInterface *scene = Detail::Interface();
    if (!BML_IFACE_HAS(scene, BML_SceneInterface, ReadObject))
        return BML_ERROR_NOT_FOUND;
    BML_SceneObjectInfo info = {};
    const int status = scene->ReadObject(object, &info);
    if (status == BML_OK)
        out = {info.Id, std::string(info.Name), info.ClassId, info.Visible != 0,
               info.Dynamic != 0,
               info.NameLength >= static_cast<int>(BML_SCENE_NAME_CAPACITY)};
    return status;
}

[[nodiscard]] inline int ReadEntityTransform(ObjectRef object, EntityTransform &out) {
    const BML_SceneInterface *scene = Detail::Interface();
    if (!BML_IFACE_HAS(scene, BML_SceneInterface, ReadEntityTransform))
        return BML_ERROR_NOT_FOUND;
    return scene->ReadEntityTransform(object, &out);
}

[[nodiscard]] inline int FindObject(const std::string &name, ObjectRef &out) {
    const BML_SceneInterface *scene = Detail::Interface();
    if (!BML_IFACE_HAS(scene, BML_SceneInterface, FindObject))
        return BML_ERROR_NOT_FOUND;
    return scene->FindObject(name.c_str(), &out);
}

[[nodiscard]] inline int FindObject(const std::string &name, int classId, ObjectRef &out) {
    const BML_SceneInterface *scene = Detail::Interface();
    if (!BML_IFACE_HAS(scene, BML_SceneInterface, FindObjectOfClass))
        return BML_ERROR_NOT_FOUND;
    return scene->FindObjectOfClass(name.c_str(), classId, &out);
}

} // namespace BML::Scene

#endif // __cplusplus

#endif // BML_SCENE_H
