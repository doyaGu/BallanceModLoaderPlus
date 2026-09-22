#ifndef BML_IVP_ANOMALY_H
#define BML_IVP_ANOMALY_H

#include "BML/IVP/Mindist.h"

#include <cstddef>

// Ballance's retail object ends at max_angular_velocity_per_psi (+0x10).
// Later source fields for collision-check and friction-mass limits are not
// present in this DLL and are deliberately not exposed here.
class IVP_Anomaly_Limits {
private:
    IVP_BOOL delete_this_if_env_is_deleted;

public:
    BML_IVP_RETAIL_ALLOCATED_OBJECT;

    explicit IVP_Anomaly_Limits(
        IVP_BOOL deleteOnEnvironmentDelete = IVP_TRUE) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::AnomalyLimitsConstruct,
            this, deleteOnEnvironmentDelete);
    }
    IVP_FLOAT max_velocity;
    int max_collisions_per_psi;
    IVP_FLOAT max_angular_velocity_per_psi;

    IVP_FLOAT get_max_velocity() const { return max_velocity; }
    IVP_FLOAT get_max_angular_velocity_per_psi() const {
        return max_angular_velocity_per_psi;
    }
    int get_max_collisions_per_psi() {
        return max_collisions_per_psi;
    }
    int get_max_collision_checks_per_psi() {
        return max_collisions_per_psi;
    }

    virtual void environment_will_be_deleted(
        IVP_Environment *environment) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::AnomalyLimitsEnvironmentDeleted,
            this, environment);
    }

    // Retail slot 1, after environment_will_be_deleted.
    virtual ~IVP_Anomaly_Limits() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::AnomalyLimitsDestruct, this);
    }
};

class IVP_Anomaly_Manager {
    friend struct BML_IvpAnomalyManagerLayoutCheck;

private:
    IVP_BOOL delete_this_if_env_is_deleted;

public:
    BML_IVP_RETAIL_ALLOCATED_OBJECT;

    explicit IVP_Anomaly_Manager(
        IVP_BOOL deleteOnEnvironmentDelete = IVP_TRUE) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::AnomalyManagerConstruct,
            this, deleteOnEnvironmentDelete);
    }
    virtual void max_velocity_exceeded(
        IVP_Anomaly_Limits *limits, IVP_Core *core,
        IVP_U_Float_Point *velocityInOut) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::AnomalyManagerMaxVelocity,
            this, limits, core, velocityInOut);
    }
    virtual void max_angular_velocity_exceeded(
        IVP_Anomaly_Limits *limits, IVP_Core *core,
        IVP_U_Float_Point *angularVelocityInOut) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::AnomalyManagerMaxAngularVelocity,
            this, limits, core, angularVelocityInOut);
    }

    // Retail cleans 0x0c bytes here: mindist plus two objects. The extra
    // IVP_DOUBLE in the nearby source revision is not part of Ballance's ABI.
    virtual void inter_penetration(
        IVP_Mindist *mindist, IVP_Real_Object *first,
        IVP_Real_Object *second) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::AnomalyManagerInterPenetration,
            this, mindist, first, second);
    }
    virtual IVP_BOOL max_collisions_exceeded_check_freezing(
        IVP_Anomaly_Limits *limits, IVP_Core *core) {
        return BML::IVP::ABI::InvokeThis<IVP_BOOL>(
            BML::IVP::ABI::Address::AnomalyManagerMaxCollisions,
            this, limits, core);
    }
    virtual void environment_will_be_deleted(
        IVP_Environment *environment) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::AnomalyManagerEnvironmentDeleted,
            this, environment);
    }
    virtual IVP_FLOAT get_push_speed_penetration(
        IVP_Real_Object *first, IVP_Real_Object *second) {
        return BML::IVP::ABI::InvokeThis<IVP_FLOAT>(
            BML::IVP::ABI::Address::AnomalyManagerPushSpeedPenetration,
            this, first, second);
    }

    // Retail slot 6, after all anomaly callbacks.
    virtual ~IVP_Anomaly_Manager() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::AnomalyManagerDestruct, this);
    }

    void solve_inter_penetration_simple(
        IVP_Real_Object *first, IVP_Real_Object *second) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::AnomalyManagerSolvePenetrationSimple,
            this, first, second);
    }
};

struct BML_IvpAnomalyManagerLayoutCheck {
    static constexpr std::size_t delete_flag =
        offsetof(IVP_Anomaly_Manager, delete_this_if_env_is_deleted);
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_Anomaly_Limits) == 0x14);
static_assert(offsetof(IVP_Anomaly_Limits, max_velocity) == 0x08);
static_assert(
    offsetof(IVP_Anomaly_Limits, max_angular_velocity_per_psi) == 0x10);
static_assert(sizeof(IVP_Anomaly_Manager) == 0x08);
static_assert(BML_IvpAnomalyManagerLayoutCheck::delete_flag == 0x04);
#endif

#endif // BML_IVP_ANOMALY_H
