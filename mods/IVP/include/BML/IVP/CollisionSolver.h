#ifndef BML_IVP_COLLISION_SOLVER_H
#define BML_IVP_COLLISION_SOLVER_H

#include "BML/IVP/Cache.h"

#include <cstddef>
#include <cstdint>

inline constexpr IVP_DOUBLE IVP_3D_SOLVER_MIN_T_STEP_DPSI_FACTOR =
    1.0f / IVP_3D_SOLVER_MAX_STEPS_PER_PSI;
inline constexpr int IVP_3D_SOLVER_PSIS_PER_SECOND = 10;

enum IVP_3D_SOLVER_TYPE : std::int32_t {
    IVP_3D_SOLVER_TYPE_MAX_DEV,
    IVP_3D_SOLVER_TYPE_MAX_DEV2,
    IVP_3D_SOLVER_TYPE_NO_ZERO_DEV,
};

// Low-level continuous-collision root solver. Ballance calls get_value through
// vtable slot zero, so a Mod-side derived value function can participate in the
// retained stepping algorithm without constructing a foreign IVP implementation.
class alignas(8) IVP_3D_Solver {
protected:
    IVP_Time calc_nullstelle(
        IVP_Time t0, IVP_Time t1, IVP_DOUBLE value,
        IVP_DOUBLE v0, IVP_DOUBLE v1,
        IVP_Real_Object *solverA, IVP_Real_Object *solverB) {
        // VC6 returns this 8-byte class through a hidden first stack argument;
        // current MSVC returns the otherwise-trivial IVP_Time in registers.
        // Spell the retail sret slot explicitly so the following arguments
        // and the callee's RET 0x34 remain aligned across the DLL boundary.
        using Function = void (__thiscall *)(
            void *, IVP_Time *, IVP_Time, IVP_Time,
            IVP_DOUBLE, IVP_DOUBLE, IVP_DOUBLE,
            IVP_Real_Object *, IVP_Real_Object *);
        Function function = BML::IVP::ABI::Resolve<Function>(
            BML::IVP::ABI::Address::Solver3DCalculateZero);
        IVP_Time result(0.0);
        if (function) {
            function(this, &result, t0, t1, value, v0, v1,
                     solverA, solverB);
        }
        return result;
    }

    virtual IVP_DOUBLE get_value(
        IVP_U_Matrix *matrixA, IVP_U_Matrix *matrixB) = 0;

public:
    IVP_DOUBLE max_deviation;
    IVP_DOUBLE inv_max_deviation;
    IVP_3D_SOLVER_TYPE type;
    IVP_DOUBLE max_deviation2;

    void set_max_deviation(IVP_DOUBLE deviation) {
        max_deviation = deviation;
        inv_max_deviation = 1.0 / deviation;
    }

    IVP_BOOL find_first_t_for_value_max_dev(
        IVP_DOUBLE value, IVP_Time tNow, IVP_Time tMax,
        int tNowCacheIndex, IVP_U_Matrix_Cache *cacheA,
        IVP_U_Matrix_Cache *cacheB, IVP_DOUBLE *valueAtNow,
        IVP_Time *timeOut) {
        return BML::IVP::ABI::InvokeThis<IVP_BOOL>(
            BML::IVP::ABI::Address::Solver3DFindFirstMaxDeviation,
            this, value, tNow, tMax, tNowCacheIndex,
            cacheA, cacheB, valueAtNow, timeOut);
    }

    IVP_BOOL find_first_t_for_value_max_dev2(
        IVP_DOUBLE value, IVP_Time tNow, IVP_Time tMax,
        int tNowCacheIndex, IVP_U_Matrix_Cache *cacheA,
        IVP_U_Matrix_Cache *cacheB, IVP_DOUBLE *valueAtNow,
        IVP_Time *timeOut) {
        return find_first_t_for_value_max_dev(
            value, tNow, tMax, tNowCacheIndex,
            cacheA, cacheB, valueAtNow, timeOut);
    }

    IVP_BOOL find_first_t_for_value_coll(
        IVP_DOUBLE value, IVP_DOUBLE absoluteMinValue,
        IVP_Time tNow, IVP_Time tMax,
        IVP_U_Matrix_Cache *cacheA, IVP_U_Matrix_Cache *cacheB,
        IVP_DOUBLE *valueAtNow, IVP_Time *timeOut) {
        // Retail may sample once beyond tMax. Its release body has no explicit
        // index-20 stop on this path, so callers must keep the requested
        // interval short enough that the extra raster sample still fits the
        // Matrix Cache's 21 entries. Normal sub-0.1-second PSIs satisfy this.
        return BML::IVP::ABI::InvokeThis<IVP_BOOL>(
            BML::IVP::ABI::Address::Solver3DFindFirstCollision,
            this, value, absoluteMinValue, tNow, tMax,
            cacheA, cacheB, valueAtNow, timeOut);
    }

    // The adjacent revision also only declares print. Its no-zero-deviation
    // implementation is compiled out and refers to matrix-cache fields absent
    // from Ballance's exact 0xAE8 layout, so neither can be reconstructed safely.
    void print(char *name) = delete;
    IVP_BOOL find_first_t_for_value_no_zero_dev(
        IVP_DOUBLE value, IVP_Time tNow, IVP_Time tMax,
        int tNowCacheIndex, IVP_U_Matrix_Cache *cacheA,
        IVP_U_Matrix_Cache *cacheB, IVP_DOUBLE *valueAtNow,
        IVP_DOUBLE *valueAtNextPsi, IVP_Time *timeOut) = delete;
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_3D_Solver) == 0x28);
static_assert(alignof(IVP_3D_Solver) == 0x08);
static_assert(offsetof(IVP_3D_Solver, max_deviation) == 0x08);
static_assert(offsetof(IVP_3D_Solver, inv_max_deviation) == 0x10);
static_assert(offsetof(IVP_3D_Solver, type) == 0x18);
static_assert(offsetof(IVP_3D_Solver, max_deviation2) == 0x20);
#endif

#endif // BML_IVP_COLLISION_SOLVER_H
