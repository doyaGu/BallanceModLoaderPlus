#ifndef BML_BUILTINCAPABILITIES_H
#define BML_BUILTINCAPABILITIES_H

#include "BML/Gameplay.h"
#include "BML/Scene.h"
#include "BML/Types.h"

#include <cstddef>

class BMLMod;
class ILogger;
class ModContext;

namespace BML {
class CKIdentityRegistry;
}

/* Registers the reads behind the loader's own interface structs. */
void RegisterBuiltinCapabilities(BMLMod &mod, BML::CKIdentityRegistry &identities,
                                 ILogger *logger);
void UnregisterBuiltinCapabilities(BMLMod &mod);

/* The scene interface in Scene.h answers out of these. CK object identity is
 * borrowed from CKIdentityRegistry, so these reads no longer own it. */
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
