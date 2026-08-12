#ifndef BML_BUILTINCAPABILITIES_H
#define BML_BUILTINCAPABILITIES_H

#include "BML/Gameplay.h"
#include "BML/Scene.h"
#include "BML/Types.h"
#include "CKTypes.h"

#include <cstddef>

class BMLMod;
class ILogger;
class ModContext;
class CKObject;

/* Registers the reads behind the loader's own interface structs. */
void RegisterBuiltinCapabilities(BMLMod &mod, ILogger *logger);
void UnregisterBuiltinCapabilities(BMLMod &mod);

/* Called only by the loader's Virtools-manager callbacks.  This stays private
 * because ObjectRef never exposes CK lifetime mechanics across the ABI. */
void InvalidateBuiltinObjectRefs(ModContext &context, const CK_ID *ids, int count);
void InvalidateAllBuiltinObjectRefs(ModContext &context);

/* Script bindings are allowed to translate an already-borrowed CKObject to the
 * opaque identity the scene reads hand out, and back again at the final host
 * boundary.  These are private loader helpers: neither CKObject nor this
 * mapping crosses the ABI. */
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

/* The gameplay interface in Gameplay.h answers out of these, and the script
 * bindings call them directly as well.  That keeps the Ballance data-array
 * interpretation in one place for both. */
int ReadBuiltinGameplayLevel(ModContext &context, BML_GameplayLevelState &out);
int ReadBuiltinGameplayEnergy(ModContext &context, BML_GameplayEnergyState &out);
int ReadBuiltinGameplayCatalogCount(ModContext &context, std::size_t &out);
int ReadBuiltinGameplayCatalogEntry(ModContext &context, std::size_t index,
                                    BML_GameplayCatalogEntry &out);
int ReadBuiltinGameplayCheckpointCount(ModContext &context, std::size_t &out);
int ReadBuiltinGameplayCheckpoint(ModContext &context, std::size_t index,
                                  BML_GameplayCheckpoint &out);
int ReadBuiltinGameplayResetpointCount(ModContext &context, std::size_t &out);
int ReadBuiltinGameplayResetpoint(ModContext &context, std::size_t index,
                                  BML_GameplayResetpoint &out);

#endif // BML_BUILTINCAPABILITIES_H
