#ifndef BML_EVENTSNAPSHOT_H
#define BML_EVENTSNAPSHOT_H

#include <cstdint>
#include <string>
#include <vector>

#include "BML/EventKinds.h"
#include "BML/Types.h"

namespace BML {

/* Private hook ingress for the event streams.  It contains copied scalar data
 * and opaque ObjectRefs only: no CK pointer is retained past the hook frame. */
struct EventSnapshot {
    int Kind = BML_EVENT_PRE_START_MENU;

    std::string Filename;
    std::string MasterName;
    bool IsMap = false;
    int FilterClass = 0;
    bool AddToScene = false;
    bool ReuseMeshes = false;
    bool ReuseMaterials = false;
    bool IsDynamic = false;
    std::vector<BML_ObjectRef> ObjectIds;
    BML_ObjectRef MasterObject{};
    BML_ObjectRef Script{};

    BML_ObjectRef Target{};
    bool Fixed = false;
    float Friction = 0.0f;
    float Elasticity = 0.0f;
    float Mass = 0.0f;
    std::string CollisionGroup;
    bool StartFrozen = false;
    bool EnableCollision = false;
    bool AutoCalculateMassCenter = false;
    float LinearDamp = 0.0f;
    float RotDamp = 0.0f;
    std::string CollisionSurface;
    BML_Vec3 MassCenter{};
    std::vector<BML_ObjectRef> ConvexMeshes;
    std::vector<BML_Vec3> BallCenters;
    std::vector<float> BallRadii;
    std::vector<BML_ObjectRef> ConcaveMeshes;

    std::string Command;
    std::vector<std::string> CommandArgs;
    std::string ConfigCategory;
    std::string ConfigKey;
    int ConfigType = -1;
    std::string ConfigValue;
    bool CheatEnabled = false;
};

} // namespace BML

#endif // BML_EVENTSNAPSHOT_H
