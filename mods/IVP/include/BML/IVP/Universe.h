#ifndef BML_IVP_UNIVERSE_H
#define BML_IVP_UNIVERSE_H

#include "BML/IVP/Types.h"

#include <cstddef>
#include <cstdint>

class IVP_Real_Object;

// Policy thresholds consumed by IVP_Cluster_Manager at each PSI. The retail
// call site reads exactly these four consecutive 32-bit fields.
class IVP_Universe_Manager_Settings {
public:
    IVP_Universe_Manager_Settings()
        : num_objects_in_environment_threshold_0(1),
          check_objects_per_second_threshold_0(1),
          num_objects_in_environment_threshold_1(1000000),
          check_objects_per_second_threshold_1(10) {}

    int num_objects_in_environment_threshold_0;
    int check_objects_per_second_threshold_0;
    int num_objects_in_environment_threshold_1;
    int check_objects_per_second_threshold_1;
};

// User-supplied large-world streaming policy. Ballance dispatches these four
// slots directly and the nearby revision does not define a virtual destructor;
// instances therefore remain owned and destroyed by the Mod that installs
// them in IVP_Application_Environment.
class IVP_Universe_Manager {
public:
    virtual void ensure_objects_in_environment(
        IVP_Real_Object *, IVP_U_Float_Point *, IVP_DOUBLE) {}
    virtual void object_no_longer_needed(IVP_Real_Object *) {}
    virtual void event_object_deleted(IVP_Real_Object *) {}
    virtual const IVP_Universe_Manager_Settings *provide_universe_settings() {
        return nullptr;
    }

    IVP_Universe_Manager() = default;
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_Universe_Manager_Settings) == 0x10);
static_assert(offsetof(
                  IVP_Universe_Manager_Settings,
                  num_objects_in_environment_threshold_0) == 0x00);
static_assert(offsetof(
                  IVP_Universe_Manager_Settings,
                  check_objects_per_second_threshold_0) == 0x04);
static_assert(offsetof(
                  IVP_Universe_Manager_Settings,
                  num_objects_in_environment_threshold_1) == 0x08);
static_assert(offsetof(
                  IVP_Universe_Manager_Settings,
                  check_objects_per_second_threshold_1) == 0x0C);
static_assert(sizeof(IVP_Universe_Manager) == 0x04);
#endif

#endif // BML_IVP_UNIVERSE_H
