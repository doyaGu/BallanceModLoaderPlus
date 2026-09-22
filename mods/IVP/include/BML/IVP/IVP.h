#ifndef BML_IVP_RECONSTRUCTED_API_H
#define BML_IVP_RECONSTRUCTED_API_H

#if defined(_MSC_VER)
#pragma pack(push, 8)
#endif

// Ballance-compatible IVP surface. It combines selectively adapted inline
// implementations with calls into the exact retail physics_RT.dll. No foreign
// IVP library is linked and no other version's complete header tree is exposed.
#include "BML/IVP.h"
#include "BML/IVP/Calls.h"
#include "BML/IVP/Types.h"
#include "BML/IVP/Memory.h"
#include "BML/IVP/GreatMatrix.h"
#include "BML/IVP/StringHash.h"
#include "BML/IVP/Set.h"
#include "BML/IVP/Attacher.h"
#include "BML/IVP/MinHash.h"
#include "BML/IVP/MinList.h"
#include "BML/IVP/TimeManager.h"
#include "BML/IVP/Collision.h"
#include "BML/IVP/Material.h"
#include "BML/IVP/Debug.h"
#include "BML/IVP/Performance.h"
#include "BML/IVP/Templates.h"
#include "BML/IVP/Geometry.h"
#include "BML/IVP/Surface.h"
#include "BML/IVP/SurfaceBuilder.h"
#include "BML/IVP/Core.h"
#include "BML/IVP/Reaction.h"
#include "BML/IVP/Object.h"
#include "BML/IVP/ObjectAttach.h"
#include "BML/IVP/Radar.h"
#include "BML/IVP/Universe.h"
#include "BML/IVP/Broadphase.h"
#include "BML/IVP/Mindist.h"
#include "BML/IVP/Anomaly.h"
#include "BML/IVP/CollisionFilter.h"
#include "BML/IVP/Environment.h"
#include "BML/IVP/Cache.h"
#include "BML/IVP/CollisionSolver.h"
#include "BML/IVP/Listeners.h"
#include "BML/IVP/Car.h"
#include "BML/IVP/Controller.h"
#include "BML/IVP/Friction.h"
#include "BML/IVP/FrictionSolver.h"
#include "BML/IVP/SimulationUnit.h"
#include "BML/IVP/Forcefield.h"
#include "BML/IVP/Constraint.h"
#include "BML/IVP/ConstraintCar.h"
#include "BML/IVP/Interpolation.h"
#include "BML/IVP/Buoyancy.h"
#include "BML/IVP/ActiveValue.h"
#include "BML/IVP/Actuator.h"
#include "BML/IVP/RealWheelsCar.h"
#include "BML/IVP/Ray.h"
#include "BML/IVP/Grid.h"
#include "BML/IVP/RaycastCar.h"
#include "BML/IVP/PhysicsRT.h"

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

#endif // BML_IVP_RECONSTRUCTED_API_H
