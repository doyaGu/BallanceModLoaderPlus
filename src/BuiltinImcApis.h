#ifndef BML_BUILTINIMCAPIS_H
#define BML_BUILTINIMCAPIS_H

#include "BML/Scene.h"
#include "BML/Types.h"
#include "CKTypes.h"

#include <utility>

#include "ImcEventSnapshot.h"

class BMLMod;
class ILogger;
class ModContext;
class CKObject;

namespace BML::Imc::Generated::Bml::Gameplay {
struct CatalogResponseValue;
struct CheckpointsResponseValue;
struct EnergyStateValue;
struct LevelStateValue;
struct ResetpointsResponseValue;
}

/* Registers the BML-owned generated IMC providers. */
void RegisterBuiltinImcApis(BMLMod &mod, ILogger *logger);
void UnregisterBuiltinImcApis(BMLMod &mod);
void PublishBuiltinImcEvent(ModContext &context, const BML::ImcEventSnapshot &event);
bool HasBuiltinImcEventConsumers(ModContext &context) noexcept;

/* Event telemetry is strictly observational.  Construct the potentially
 * allocating snapshot inside this boundary so an OOM or a provider failure
 * never changes the outcome of an original game hook. */
template <typename Capture>
void CaptureBuiltinImcEventNoexcept(ModContext &context, Capture &&capture) noexcept {
    try {
        if (!HasBuiltinImcEventConsumers(context))
            return;
        BML::ImcEventSnapshot event;
        std::forward<Capture>(capture)(event);
        PublishBuiltinImcEvent(context, event);
    } catch (...) {
        // Observability must never escape into the original game callback.
    }
}

/* Called only by the loader's Virtools-manager callbacks.  This stays private
 * because ObjectRef never exposes CK lifetime mechanics across the ABI. */
void InvalidateBuiltinObjectRefs(ModContext &context, const CK_ID *ids, int count);
void InvalidateAllBuiltinObjectRefs(ModContext &context);

/* Script bindings are allowed to translate an already-borrowed CKObject to
 * the built-in scene provider's opaque identity, and back again at the final
 * host boundary.  These are private loader helpers: neither CKObject nor this
 * mapping appears in the public IMC ABI. */
BML_ObjectRef MakeBuiltinObjectRef(ModContext &context, CKObject *object);
CKObject *ResolveBuiltinObjectRef(ModContext &context, BML_ObjectRef reference);

/* The scene interface in Scene.h answers out of these.  The CKObject handling
 * and the object-reference registry stay on this side, so the thunks that fill
 * the interface struct never touch either. */
int ReadBuiltinSceneObject(ModContext &context, BML_ObjectRef object, BML_SceneObjectInfo &out);
int ReadBuiltinSceneEntityTransform(ModContext &context, BML_ObjectRef object,
                                    BML_SceneEntityTransform &out);
int FindBuiltinSceneObject(ModContext &context, const char *name, BML_ObjectRef &out);
int FindBuiltinSceneObjectOfClass(ModContext &context, const char *name, int classId,
                                  BML_ObjectRef &out);

/* Script bindings reuse the built-in gameplay provider directly.  This keeps
 * the Ballance data-array interpretation in one place without routing a local
 * call through IMC serialization. */
int ReadBuiltinGameplayCatalog(
    ModContext &context,
    BML::Imc::Generated::Bml::Gameplay::CatalogResponseValue &out);
int ReadBuiltinGameplayCheckpoints(
    ModContext &context,
    BML::Imc::Generated::Bml::Gameplay::CheckpointsResponseValue &out);
int ReadBuiltinGameplayEnergy(
    ModContext &context,
    BML::Imc::Generated::Bml::Gameplay::EnergyStateValue &out);
int ReadBuiltinGameplayLevel(
    ModContext &context,
    BML::Imc::Generated::Bml::Gameplay::LevelStateValue &out);
int ReadBuiltinGameplayResetpoints(
    ModContext &context,
    BML::Imc::Generated::Bml::Gameplay::ResetpointsResponseValue &out);

#endif // BML_BUILTINIMCAPIS_H
