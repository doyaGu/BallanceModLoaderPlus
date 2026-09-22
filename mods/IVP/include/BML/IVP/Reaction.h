#ifndef BML_IVP_REACTION_H
#define BML_IVP_REACTION_H

#include "BML/IVP/Core.h"

#include <cstddef>
#include <cstring>

// Four-component inertia vector used by IVP's core-reaction solver.  The
// fourth component is inverse mass; the first three are inverse rotational
// inertia in core space.
class IVP_U_Point_4 : public IVP_U_Float_Hesse {
public:
    IVP_DOUBLE dot_product4(const IVP_U_Point_4 &other) {
        return static_cast<IVP_DOUBLE>(k[0]) * other.k[0] +
               static_cast<IVP_DOUBLE>(k[1]) * other.k[1] +
               static_cast<IVP_DOUBLE>(k[2]) * other.k[2] +
               static_cast<IVP_DOUBLE>(hesse_val) * other.hesse_val;
    }

    void set_line_wise_mult4(
        const IVP_U_Point_4 *left, const IVP_U_Point_4 *right) {
        k[0] = left->k[0] * right->k[0];
        k[1] = left->k[1] * right->k[1];
        k[2] = left->k[2] * right->k[2];
        hesse_val = left->hesse_val * right->hesse_val;
    }
};

// Ballance retains the translation initializer and two-dimensional impulse
// path in physics_RT.dll.  The other short methods were link-stripped, so only
// those methods are reconstructed from the neighboring implementation after
// checking this complete 0x148 layout against the Ballance IDB.
class IVP_Solver_Core_Reaction {
private:
    void init_rot_ws(
        IVP_Core *core, IVP_U_Point_4 crossDirectionPositionCs[3],
        IVP_U_Point_4 crossMultInverse[3], IVP_FLOAT sign) {
        const IVP_U_Matrix *worldFromCore =
            core->get_m_world_f_core_PSI();
        const int coreIndex =
            crossDirectionPositionCs == cross_direction_position_cs0 ? 0 : 1;
        m_world_f_core_last_psi[coreIndex] = worldFromCore;
        const auto *inverseMasses = reinterpret_cast<const IVP_U_Point_4 *>(
            core->get_inv_rot_inertia());

        for (int axis = 0; axis < 3 && direction_ws[axis]; ++axis) {
            worldFromCore->inline_vimult3(
                direction_ws[axis], &crossDirectionPositionCs[axis]);
            crossDirectionPositionCs[axis].hesse_val = 0.0f;
            crossMultInverse[axis].set_line_wise_mult4(
                &crossDirectionPositionCs[axis], inverseMasses);

            const IVP_DOUBLE diagonal =
                m_velocity_ds_f_impulse_ds.get_elem(axis, axis) +
                crossMultInverse[axis].dot_product(
                    &crossDirectionPositionCs[axis]);
            m_velocity_ds_f_impulse_ds.set_elem(axis, axis, diagonal);
            for (int previous = 0; previous < axis; ++previous) {
                const IVP_DOUBLE mixed =
                    m_velocity_ds_f_impulse_ds.get_elem(previous, axis) +
                    crossMultInverse[axis].dot_product(
                        &crossDirectionPositionCs[previous]);
                m_velocity_ds_f_impulse_ds.set_elem(
                    previous, axis, mixed);
            }
            delta_velocity_ds.k[axis] += static_cast<IVP_FLOAT>(
                core->rot_speed.dot_product(
                    &crossDirectionPositionCs[axis]) * sign);
        }
    }

    void exert_impulse(
        IVP_Core *core0, IVP_Core *core1,
        const IVP_U_Float_Point &impulse, int dimensions,
        bool angularOnly) {
        if (core0) {
            for (int axis = 0; axis < dimensions; ++axis) {
                if (!angularOnly) {
                    core0->speed.add_multiple(
                        direction_ws[axis],
                        impulse.k[axis] * core0->get_inv_mass());
                }
                core0->rot_speed.add_multiple(
                    &cr_mult_inv0[axis], impulse.k[axis]);
            }
        }
        if (core1) {
            for (int axis = 0; axis < dimensions; ++axis) {
                if (!angularOnly) {
                    core1->speed.add_multiple(
                        direction_ws[axis],
                        -impulse.k[axis] * core1->get_inv_mass());
                }
                core1->rot_speed.add_multiple(
                    &cr_mult_inv1[axis], -impulse.k[axis]);
            }
        }
    }

public:
    IVP_U_Float_Point *direction_ws[3];
    IVP_U_Point_4 cross_direction_position_cs0[3];
    IVP_U_Point_4 cross_direction_position_cs1[3];
    const IVP_U_Matrix *m_world_f_core_last_psi[2];
    IVP_U_Point_4 cr_mult_inv0[3];
    IVP_U_Point_4 cr_mult_inv1[3];
    IVP_U_Matrix3 m_velocity_ds_f_impulse_ds;
    IVP_U_Float_Point delta_velocity_ds;

    void init_reaction_solver_translation_ws(
        IVP_Core *core0, IVP_Core *core1, IVP_U_Point &positionWs,
        IVP_U_Float_Point *direction0Ws,
        IVP_U_Float_Point *direction1Ws,
        IVP_U_Float_Point *direction2Ws) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SolverCoreReactionInitTranslation,
            this, core0, core1, &positionWs, direction0Ws,
            direction1Ws, direction2Ws);
    }

    void init_reaction_solver_rotation_ws(
        IVP_Core *core0, IVP_Core *core1,
        IVP_U_Float_Point *direction0Ws,
        IVP_U_Float_Point *direction1Ws,
        IVP_U_Float_Point *direction2Ws) {
        direction_ws[0] = direction0Ws;
        direction_ws[1] = direction1Ws;
        direction_ws[2] = direction2Ws;
        std::memset(&m_velocity_ds_f_impulse_ds, 0,
                    sizeof(m_velocity_ds_f_impulse_ds));
        std::memset(&delta_velocity_ds, 0, sizeof(delta_velocity_ds));
        if (core0) {
            init_rot_ws(core0, cross_direction_position_cs0,
                        cr_mult_inv0, 1.0f);
        }
        if (core1) {
            init_rot_ws(core1, cross_direction_position_cs1,
                        cr_mult_inv1, -1.0f);
        }
    }

    IVP_RETURN_TYPE invert_3x3_matrix() {
        m_velocity_ds_f_impulse_ds.set_elem(
            1, 0, m_velocity_ds_f_impulse_ds.get_elem(0, 1));
        m_velocity_ds_f_impulse_ds.set_elem(
            2, 0, m_velocity_ds_f_impulse_ds.get_elem(0, 2));
        m_velocity_ds_f_impulse_ds.set_elem(
            2, 1, m_velocity_ds_f_impulse_ds.get_elem(1, 2));
        return m_velocity_ds_f_impulse_ds.real_invert();
    }

    IVP_U_Matrix3 *get_m_velocity_ds_f_impulse_ds() {
        return &m_velocity_ds_f_impulse_ds;
    }

    void exert_impulse_dim1(
        IVP_Core *core0, IVP_Core *core1,
        IVP_U_Float_Point &impulseDs) {
        exert_impulse(core0, core1, impulseDs, 1, false);
    }
    void exert_impulse_dim2(
        IVP_Core *core0, IVP_Core *core1,
        IVP_U_Float_Point &impulseDs) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SolverCoreReactionExertImpulse2,
            this, core0, core1, &impulseDs);
    }
    void exert_impulse_dim3(
        IVP_Core *core0, IVP_Core *core1,
        IVP_U_Float_Point &impulseDs) {
        exert_impulse(core0, core1, impulseDs, 3, false);
    }
    void exert_angular_impulse_dim1(
        IVP_Core *core0, IVP_Core *core1,
        IVP_U_Float_Point &impulseRs) {
        exert_impulse(core0, core1, impulseRs, 1, true);
    }
    void exert_angular_impulse_dim2(
        IVP_Core *core0, IVP_Core *core1,
        IVP_U_Float_Point &impulseRs) {
        exert_impulse(core0, core1, impulseRs, 2, true);
    }
    void exert_angular_impulse_dim3(
        IVP_Core *core0, IVP_Core *core1,
        IVP_U_Float_Point &impulseRs) {
        exert_impulse(core0, core1, impulseRs, 3, true);
    }
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_U_Point_4) == 0x10);
static_assert(sizeof(IVP_Solver_Core_Reaction) == 0x148);
static_assert(offsetof(IVP_Solver_Core_Reaction, direction_ws) == 0x00);
static_assert(offsetof(IVP_Solver_Core_Reaction,
                       cross_direction_position_cs0) == 0x0C);
static_assert(offsetof(IVP_Solver_Core_Reaction,
                       cross_direction_position_cs1) == 0x3C);
static_assert(offsetof(IVP_Solver_Core_Reaction,
                       m_world_f_core_last_psi) == 0x6C);
static_assert(offsetof(IVP_Solver_Core_Reaction, cr_mult_inv0) == 0x74);
static_assert(offsetof(IVP_Solver_Core_Reaction, cr_mult_inv1) == 0xA4);
static_assert(offsetof(IVP_Solver_Core_Reaction,
                       m_velocity_ds_f_impulse_ds) == 0xD8);
static_assert(offsetof(IVP_Solver_Core_Reaction,
                       delta_velocity_ds) == 0x138);
#endif

#endif // BML_IVP_REACTION_H
