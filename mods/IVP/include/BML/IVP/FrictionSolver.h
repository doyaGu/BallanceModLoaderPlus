#ifndef BML_IVP_FRICTION_SOLVER_H
#define BML_IVP_FRICTION_SOLVER_H

#include "BML/IVP/Friction.h"
#include "BML/IVP/GreatMatrix.h"

#include <cstddef>

// Ballance keeps the 512 pointer slots inline. The base vector therefore
// points at this object-owned buffer and must never free it as heap storage.
class IVP_Vector_of_Contact_Info_512
    : public IVP_U_Vector<IVP_Impact_Solver_Long_Term> {
  IVP_Impact_Solver_Long_Term *elem_buffer[512];

public:
  IVP_Vector_of_Contact_Info_512()
      : IVP_U_Vector<IVP_Impact_Solver_Long_Term>(
            reinterpret_cast<void **>(elem_buffer), 512) {}
};

// Temporary solver built on the stack by IVP_Friction_System. The 0x840
// layout is fixed by the retail do_friction_system stack frame and retained
// constructor. Unions suppress host-side member construction: the retained
// complete constructor/destructor already own those member lifetimes.
class alignas(8) IVP_Friction_Solver {
public:
  union {
    IVP_Great_Matrix_Many_Zero dist_change_mat;
  };
  IVP_DOUBLE correct_x_factor;
  IVP_Environment *l_environment;
  const IVP_Event_Sim *es;
  union {
    IVP_Vector_of_Contact_Info_512 contact_info_vector;
  };
  int gauss_succed;

  IVP_Friction_Solver(IVP_Friction_System *system,
                      const IVP_Event_Sim *event) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSolverConstruct, this, system, event);
  }
  ~IVP_Friction_Solver() { contact_info_vector.clear(); }

  static IVP_DOUBLE get_closing_speed_core_i(
      const IVP_Impact_Solver_Long_Term *information, int coreIndex,
      const IVP_U_Float_Point *rotationalSpeed,
      IVP_U_Float_Point *linearSpeed) {
    return BML::IVP::ABI::Invoke<IVP_DOUBLE>(
        BML::IVP::ABI::Address::FrictionSolverGetClosingSpeedCore,
        information, coreIndex, rotationalSpeed, linearSpeed);
  }
  static IVP_DOUBLE
  get_inv_virtual_mass(const IVP_Impact_Solver_Long_Term *information) {
    return information->inv_virtual_mass;
  }
  static void apply_impulse(const IVP_Impact_Solver_Long_Term *information,
                            IVP_DOUBLE impulse) {
    IVP_Core *first = information->contact_core[0];
    if (first) {
      IVP_U_Float_Point angular;
      angular.set_pairwise_mult(&information->contact_cross_nomal_cs[0],
                                first->get_inv_rot_inertia());
      first->rot_speed.add_multiple(&angular, -impulse);
      first->speed.add_multiple(&information->surf_normal,
                                -impulse * first->get_inv_mass());
    }
    IVP_Core *second = information->contact_core[1];
    if (second) {
      IVP_U_Float_Point angular;
      angular.set_pairwise_mult(&information->contact_cross_nomal_cs[1],
                                second->get_inv_rot_inertia());
      second->rot_speed.add_multiple(&angular, impulse);
      second->speed.add_multiple(&information->surf_normal,
                                 impulse * second->get_inv_mass());
    }
  }
  static void
  async_apply_impulse(const IVP_Impact_Solver_Long_Term *information,
                      IVP_DOUBLE impulse) {
    IVP_Core *first = information->contact_core[0];
    if (first) {
      IVP_U_Float_Point angular;
      angular.set_pairwise_mult(&information->contact_cross_nomal_cs[0],
                                first->get_inv_rot_inertia());
      first->rot_speed_change.add_multiple(&angular, -impulse);
      first->speed_change.add_multiple(&information->surf_normal,
                                       -impulse * first->get_inv_mass());
    }
    IVP_Core *second = information->contact_core[1];
    if (second) {
      IVP_U_Float_Point angular;
      angular.set_pairwise_mult(&information->contact_cross_nomal_cs[1],
                                second->get_inv_rot_inertia());
      second->rot_speed_change.add_multiple(&angular, impulse);
      second->speed_change.add_multiple(&information->surf_normal,
                                        impulse * second->get_inv_mass());
    }
  }
  static void test_push_core_i(
      const IVP_Impact_Solver_Long_Term *information, int coreIndex,
      IVP_U_Float_Point *rotationChange, IVP_U_Float_Point *speedChange,
      IVP_FLOAT factor) {
    IVP_Core *core = information->contact_core[coreIndex];
    rotationChange->set_pairwise_mult(
        &information->contact_cross_nomal_cs[coreIndex],
        core->get_inv_rot_inertia());
    if (factor < 0.0f)
      rotationChange->set_negative(rotationChange);
    speedChange->set_multiple(&information->surf_normal,
                              core->get_inv_mass() * factor);
  }
  static IVP_DOUBLE calc_desired_gap_speed(IVP_DOUBLE closingSpeed,
                                           IVP_DOUBLE gapDifference,
                                           IVP_FLOAT speedupFactor) {
    IVP_DOUBLE delay = speedupFactor;
    if (gapDifference < 0.0)
      delay *= 20.0;
    return gapDifference * delay + closingSpeed;
  }
  static void ease_test_two_mindists(IVP_Contact_Point *,
                                     IVP_Contact_Point *,
                                     IVP_U_Float_Point *) {
    // The diagnostic body is disabled in the Ballance build. Calls disappear
    // after inlining, so the compatible public operation is an intentional
    // no-op rather than a fabricated retail entry point.
  }

  static void ease_friction_pair(IVP_Friction_Core_Pair *pair,
                                 IVP_U_Memory *memory) {
    BML::IVP::ABI::Invoke<void>(
        BML::IVP::ABI::Address::FrictionSolverEasePair, pair, memory);
  }
  static void ease_two_mindists(IVP_Contact_Point *first,
                                IVP_Contact_Point *second,
                                IVP_U_Float_Point *firstDifference,
                                IVP_U_Float_Point *secondDifference,
                                IVP_DOUBLE easeFactor) {
    BML::IVP::ABI::Invoke<void>(
        BML::IVP::ABI::Address::FrictionSolverEaseTwoMindists, first, second,
        firstDifference, secondDifference, easeFactor);
  }
  void setup_coords_mindists(IVP_Friction_System *system) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSolverSetupCoordinates, this, system);
  }
  int calc_solver_PSI(IVP_Friction_System *system,
                      int *originalPositionOfActive) {
    return BML::IVP::ABI::InvokeThis<int>(
        BML::IVP::ABI::Address::FrictionSolverCalculatePsi, this, system,
        originalPositionOfActive);
  }
  void calc_distance_matrix_column(
      int currentContactPointIndex, IVP_Core *core,
      IVP_Friction_Info_For_Core *frictionInfo,
      IVP_U_Float_Point *rotationChange,
      IVP_U_Float_Point *translationChange) {
    for (int index = 0; index < frictionInfo->friction_springs.len(); ++index) {
      IVP_Impact_Solver_Long_Term *information =
          frictionInfo->friction_springs.element_at(index)->get_lt();
      const int row = information->index_in_fs;
      if (row < 0)
        continue;

      int coreIndex = 1;
      IVP_DOUBLE sign = 1.0;
      if (information->contact_core[0] == core) {
        coreIndex = 0;
        sign = -1.0;
      }
      if (!information->contact_core[coreIndex])
        continue;

      const IVP_DOUBLE speedChange =
          sign * get_closing_speed_core_i(information, coreIndex,
                                          rotationChange, translationChange);
      dist_change_mat.matrix_values[
          row * dist_change_mat.aligned_row_len + currentContactPointIndex] +=
          speedChange;
    }
  }
  IVP_RETURN_TYPE test_gauss_solution_suggestion(
      IVP_DOUBLE *pushResults, int *activeIsAtPosition, int totalActives,
      IVP_U_Memory *memory) {
    return BML::IVP::ABI::InvokeThis<IVP_RETURN_TYPE>(
        BML::IVP::ABI::Address::FrictionSolverTestGauss, this, pushResults,
        activeIsAtPosition, totalActives, memory);
  }
  void solve_linear_equation_and_push(IVP_Friction_System *system,
                                      int *activeIsAtPosition,
                                      int totalActives,
                                      IVP_U_Memory *memory) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSolverSolveAndPush, this, system,
        activeIsAtPosition, totalActives, memory);
  }
  int do_resulting_pushes(IVP_Friction_System *system) {
    return BML::IVP::ABI::InvokeThis<int>(
        BML::IVP::ABI::Address::FrictionSolverApplyResults, this, system);
  }
  void normize_constraint_equ() {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSolverNormalizeConstraints, this);
  }
  void factor_result_vec() {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSolverFactorResult, this);
  }

  void debug_distance_after_push(int) {
    // The only neighboring implementation is compiled out; the retail
    // release likewise has no diagnostic body or observable operation.
  }

  // These neighboring-revision members have no independent Ballance body.
  // Exact declarations are retained, but they intentionally fail to link
  // until their inlined callsites are reconstructed and verified.
  IVP_FLOAT do_penalty_step(IVP_FLOAT *, IVP_FLOAT *, IVP_FLOAT, IVP_FLOAT);
  void do_penalty_method(IVP_Friction_System *);
  void complex_failed(IVP_Friction_System *);
  void print_dist_velocity(IVP_Friction_System *);
  void do_inactives_pushes(IVP_Friction_System *);
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_Vector_of_Contact_Info_512) == 0x808);
static_assert(sizeof(IVP_Friction_Solver) == 0x840);
static_assert(alignof(IVP_Friction_Solver) == 0x08);
static_assert(offsetof(IVP_Friction_Solver, correct_x_factor) == 0x20);
static_assert(offsetof(IVP_Friction_Solver, l_environment) == 0x28);
static_assert(offsetof(IVP_Friction_Solver, es) == 0x2C);
static_assert(offsetof(IVP_Friction_Solver, contact_info_vector) == 0x30);
static_assert(offsetof(IVP_Friction_Solver, gauss_succed) == 0x838);
#endif

#endif // BML_IVP_FRICTION_SOLVER_H
