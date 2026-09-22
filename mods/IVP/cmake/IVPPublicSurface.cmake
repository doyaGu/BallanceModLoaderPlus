# IVP public surface.
#
# Keep this inventory explicit: every file here is part of the installed IVP
# interface.  The checks below make the source-tree umbrella and the staged SDK
# consume the same curated set instead of maintaining independent lists.
set(IVP_PUBLIC_RELATIVE_HEADERS
        BML/IVP.h
        BML/IVP/ActiveValue.h
        BML/IVP/Actuator.h
        BML/IVP/Anomaly.h
        BML/IVP/Attacher.h
        BML/IVP/Broadphase.h
        BML/IVP/Buoyancy.h
        BML/IVP/Cache.h
        BML/IVP/Calls.h
        BML/IVP/Car.h
        BML/IVP/Collision.h
        BML/IVP/CollisionFilter.h
        BML/IVP/CollisionSolver.h
        BML/IVP/Constraint.h
        BML/IVP/ConstraintCar.h
        BML/IVP/Controller.h
        BML/IVP/Core.h
        BML/IVP/Debug.h
        BML/IVP/Environment.h
        BML/IVP/Forcefield.h
        BML/IVP/Friction.h
        BML/IVP/FrictionSolver.h
        BML/IVP/Geometry.h
        BML/IVP/GreatMatrix.h
        BML/IVP/Grid.h
        BML/IVP/Interpolation.h
        BML/IVP/IVP.h
        BML/IVP/Listeners.h
        BML/IVP/Material.h
        BML/IVP/Memory.h
        BML/IVP/Mindist.h
        BML/IVP/MinHash.h
        BML/IVP/MinList.h
        BML/IVP/Object.h
        BML/IVP/ObjectAttach.h
        BML/IVP/Performance.h
        BML/IVP/PhysicsRT.h
        BML/IVP/Radar.h
        BML/IVP/Ray.h
        BML/IVP/RaycastCar.h
        BML/IVP/RealWheelsCar.h
        BML/IVP/Reaction.h
        BML/IVP/Set.h
        BML/IVP/SimulationUnit.h
        BML/IVP/StringHash.h
        BML/IVP/Surface.h
        BML/IVP/SurfaceBuilder.h
        BML/IVP/Templates.h
        BML/IVP/TimeManager.h
        BML/IVP/Types.h
        BML/IVP/Universe.h
        BML/IVP/detail/AddressEntries.inc
        BML/IVP/detail/DataAddresses.inc
)

set(IVP_PUBLIC_HEADERS)
foreach(IVP_PUBLIC_RELATIVE_HEADER IN LISTS IVP_PUBLIC_RELATIVE_HEADERS)
    set(IVP_PUBLIC_HEADER
            "${IVP_INCLUDE_DIR}/${IVP_PUBLIC_RELATIVE_HEADER}")
    if(NOT EXISTS "${IVP_PUBLIC_HEADER}")
        message(FATAL_ERROR
                "IVP public-surface inventory names a missing file: "
                "${IVP_PUBLIC_HEADER}")
    endif()
    list(APPEND IVP_PUBLIC_HEADERS "${IVP_PUBLIC_HEADER}")
endforeach()

# Reject an unregistered public header.  This deliberately does not glob for
# installation; the explicit inventory remains the reviewed contract.
file(GLOB IVP_DISCOVERED_HEADERS
        RELATIVE "${IVP_INCLUDE_DIR}"
        "${IVP_INCLUDE_DIR}/BML/IVP/*.h")
list(APPEND IVP_DISCOVERED_HEADERS BML/IVP.h)
list(SORT IVP_DISCOVERED_HEADERS)

set(IVP_INVENTORIED_HEADERS ${IVP_PUBLIC_RELATIVE_HEADERS})
list(FILTER IVP_INVENTORIED_HEADERS INCLUDE REGEX "\\.h$")
list(SORT IVP_INVENTORIED_HEADERS)
if(NOT IVP_DISCOVERED_HEADERS STREQUAL IVP_INVENTORIED_HEADERS)
    message(FATAL_ERROR
            "IVP public headers and IVP_PUBLIC_RELATIVE_HEADERS differ.\n"
            "On disk: ${IVP_DISCOVERED_HEADERS}\n"
            "Inventory: ${IVP_INVENTORIED_HEADERS}")
endif()

set(IVP_UMBRELLA "${IVP_INCLUDE_DIR}/BML/IVP/IVP.h")
file(READ "${IVP_UMBRELLA}" IVP_UMBRELLA_TEXT)
foreach(IVP_HEADER IN LISTS IVP_INVENTORIED_HEADERS)
    if(IVP_HEADER STREQUAL "BML/IVP/IVP.h")
        continue()
    endif()
    string(FIND "${IVP_UMBRELLA_TEXT}"
            "#include \"${IVP_HEADER}\"" IVP_INCLUDE_OFFSET)
    if(IVP_INCLUDE_OFFSET EQUAL -1)
        message(FATAL_ERROR
                "${IVP_UMBRELLA} does not include ${IVP_HEADER}")
    endif()
endforeach()
