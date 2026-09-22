#include "BML/IVP/IVP.h"

#include <type_traits>

namespace {

class StiffSpringLayoutProbe : public IVP_Controller_Stiff_Spring {
public:
  using IVP_Controller_Stiff_Spring::break_max_len;
  using IVP_Controller_Stiff_Spring::listeners_spring_event;
  using IVP_Controller_Stiff_Spring::max_len_exceed_type;
  using IVP_Controller_Stiff_Spring::spring_constant;
  using IVP_Controller_Stiff_Spring::spring_damp;
  using IVP_Controller_Stiff_Spring::spring_len;
};

class ActiveStiffSpringLayoutProbe
    : public IVP_Controller_Stiff_Spring_Active {
public:
  using IVP_Controller_Stiff_Spring_Active::active_float_spring_constant;
  using IVP_Controller_Stiff_Spring_Active::active_float_spring_damp;
  using IVP_Controller_Stiff_Spring_Active::active_float_spring_len;
};

class StabilizerLayoutProbe : public IVP_Actuator_Stabilizer {
public:
  using IVP_Actuator_Stabilizer::stabi_constant;
};

class SuspensionLayoutProbe : public IVP_Actuator_Suspension {
public:
  using IVP_Actuator_Suspension::max_body_force;
  using IVP_Actuator_Suspension::spring_dampening_compression;
};

class MotionLayoutProbe : public IVP_Controller_Motion {
public:
  using IVP_Controller_Motion::angular_damp_factor;
  using IVP_Controller_Motion::core;
  using IVP_Controller_Motion::damp_factor;
  using IVP_Controller_Motion::force_factor;
  using IVP_Controller_Motion::l_environment;
  using IVP_Controller_Motion::max_torque;
  using IVP_Controller_Motion::max_translation_force;
  using IVP_Controller_Motion::real_object;
  using IVP_Controller_Motion::target_pos_ws;
  using IVP_Controller_Motion::target_q_world_f_core;
  using IVP_Controller_Motion::torque_factor;
};

class GolemLayoutProbe : public IVP_Controller_Golem {
public:
  using IVP_Controller_Golem::angular_velocity_set;
  using IVP_Controller_Golem::filter_dtime;
  using IVP_Controller_Golem::integrated_delta_position;
  using IVP_Controller_Golem::max_delta_position;
  using IVP_Controller_Golem::max_golem_force;
  using IVP_Controller_Golem::prime_orientation_0;
  using IVP_Controller_Golem::prime_position_ws;
  using IVP_Controller_Golem::time_of_prime_position;
  using IVP_Controller_Golem::velocity_ws;
};

class FixedKeyframedLayoutProbe : public IVP_Constraint_Fixed_Keyframed {
public:
  using IVP_Constraint_Fixed_Keyframed::angular_velocity_set;
  using IVP_Constraint_Fixed_Keyframed::attached_obj;
  using IVP_Constraint_Fixed_Keyframed::cores_of_constraint_system;
  using IVP_Constraint_Fixed_Keyframed::force_factor;
  using IVP_Constraint_Fixed_Keyframed::l_environment;
  using IVP_Constraint_Fixed_Keyframed::max_translation_force;
  using IVP_Constraint_Fixed_Keyframed::prime_orientation_0;
  using IVP_Constraint_Fixed_Keyframed::prime_position_Ros;
  using IVP_Constraint_Fixed_Keyframed::reference_obj;
  using IVP_Constraint_Fixed_Keyframed::velocity_Ros;
};

class ForcefieldLayoutProbe : public IVP_Forcefield {
public:
  using IVP_Forcefield::i_am_owner_of_set_of_cores;
  using IVP_Forcefield::set_of_cores;
};

class SurfaceManagerGridLayoutProbe : public IVP_SurfaceManager_Grid {
public:
  using IVP_SurfaceManager_Grid::compact_grid;
};

class FloatingControllerLayoutProbe : public IVP_Controller_Floating {
public:
  using IVP_Controller_Floating::current_distance;
  using IVP_Controller_Floating::max_adhesive_force;
  using IVP_Controller_Floating::max_repulsive_force;
  using IVP_Controller_Floating::object;
  using IVP_Controller_Floating::position_os;
  using IVP_Controller_Floating::ray_direction_ws;
  using IVP_Controller_Floating::target_distance;
};

class WorldFrictionControllerLayoutProbe
    : public IVP_Controller_World_Friction {
public:
  using IVP_Controller_World_Friction::clip_manhattan;
  using IVP_Controller_World_Friction::desired_rot_speed_cs;
  using IVP_Controller_World_Friction::desired_speed_ws;
  using IVP_Controller_World_Friction::friction_value_rotation;
  using IVP_Controller_World_Friction::friction_value_translation;
  using IVP_Controller_World_Friction::real_obj;
};

class CompileMatrixSeparationSolver final : public IVP_3D_Solver {
protected:
  IVP_DOUBLE get_value(IVP_U_Matrix *, IVP_U_Matrix *) override {
    return 0.0;
  }
};

static_assert(offsetof(StiffSpringLayoutProbe, spring_len) == 0x74);
static_assert(offsetof(StiffSpringLayoutProbe, spring_constant) == 0x78);
static_assert(offsetof(StiffSpringLayoutProbe, spring_damp) == 0x7C);
static_assert(offsetof(StiffSpringLayoutProbe, break_max_len) == 0x80);
static_assert(offsetof(StiffSpringLayoutProbe, max_len_exceed_type) == 0x84);
static_assert(offsetof(StiffSpringLayoutProbe, listeners_spring_event) == 0x88);
static_assert(
    offsetof(ActiveStiffSpringLayoutProbe, active_float_spring_len) == 0x94);
static_assert(offsetof(ActiveStiffSpringLayoutProbe,
                       active_float_spring_constant) == 0x98);
static_assert(
    offsetof(ActiveStiffSpringLayoutProbe, active_float_spring_damp) == 0x9C);
static_assert(offsetof(StabilizerLayoutProbe, stabi_constant) == 0xD4);
static_assert(
    offsetof(SuspensionLayoutProbe, spring_dampening_compression) == 0x98);
static_assert(offsetof(SuspensionLayoutProbe, max_body_force) == 0x9C);
static_assert(offsetof(MotionLayoutProbe, target_pos_ws) == 0x08);
static_assert(offsetof(MotionLayoutProbe, target_q_world_f_core) == 0x28);
static_assert(offsetof(MotionLayoutProbe, max_translation_force) == 0x48);
static_assert(offsetof(MotionLayoutProbe, max_torque) == 0x58);
static_assert(offsetof(MotionLayoutProbe, force_factor) == 0x68);
static_assert(offsetof(MotionLayoutProbe, damp_factor) == 0x6C);
static_assert(offsetof(MotionLayoutProbe, torque_factor) == 0x70);
static_assert(offsetof(MotionLayoutProbe, angular_damp_factor) == 0x74);
static_assert(offsetof(MotionLayoutProbe, l_environment) == 0x78);
static_assert(offsetof(MotionLayoutProbe, real_object) == 0x7C);
static_assert(offsetof(MotionLayoutProbe, core) == 0x80);
static_assert(offsetof(GolemLayoutProbe, integrated_delta_position) == 0x88);
static_assert(offsetof(GolemLayoutProbe, time_of_prime_position) == 0x90);
static_assert(offsetof(GolemLayoutProbe, angular_velocity_set) == 0xA4);
static_assert(offsetof(GolemLayoutProbe, prime_orientation_0) == 0xA8);
static_assert(offsetof(GolemLayoutProbe, prime_position_ws) == 0xE8);
static_assert(offsetof(GolemLayoutProbe, velocity_ws) == 0x108);
static_assert(offsetof(GolemLayoutProbe, max_delta_position) == 0x118);
static_assert(offsetof(GolemLayoutProbe, max_golem_force) == 0x124);
static_assert(offsetof(GolemLayoutProbe, filter_dtime) == 0x128);
static_assert(
    offsetof(FixedKeyframedLayoutProbe, max_translation_force) == 0x04);
static_assert(offsetof(FixedKeyframedLayoutProbe, force_factor) == 0x24);
static_assert(offsetof(FixedKeyframedLayoutProbe, l_environment) == 0x34);
static_assert(offsetof(FixedKeyframedLayoutProbe, prime_orientation_0) == 0x50);
static_assert(offsetof(FixedKeyframedLayoutProbe, angular_velocity_set) == 0x90);
static_assert(offsetof(FixedKeyframedLayoutProbe, prime_position_Ros) == 0x98);
static_assert(offsetof(FixedKeyframedLayoutProbe, velocity_Ros) == 0xB8);
static_assert(offsetof(FixedKeyframedLayoutProbe, reference_obj) == 0xC8);
static_assert(offsetof(FixedKeyframedLayoutProbe, attached_obj) == 0xCC);
static_assert(
    offsetof(FixedKeyframedLayoutProbe, cores_of_constraint_system) == 0xD0);
static_assert(offsetof(ForcefieldLayoutProbe, set_of_cores) == 0x08);
static_assert(
    offsetof(ForcefieldLayoutProbe, i_am_owner_of_set_of_cores) == 0x0C);
static_assert(
    offsetof(SurfaceManagerGridLayoutProbe, compact_grid) == 0x04);
static_assert(offsetof(FloatingControllerLayoutProbe, object) == 0x04);
static_assert(
    offsetof(FloatingControllerLayoutProbe, max_repulsive_force) == 0x08);
static_assert(
    offsetof(FloatingControllerLayoutProbe, max_adhesive_force) == 0x0C);
static_assert(offsetof(FloatingControllerLayoutProbe, position_os) == 0x10);
static_assert(
    offsetof(FloatingControllerLayoutProbe, ray_direction_ws) == 0x20);
static_assert(
    offsetof(FloatingControllerLayoutProbe, target_distance) == 0x30);
static_assert(
    offsetof(FloatingControllerLayoutProbe, current_distance) == 0x38);
static_assert(offsetof(WorldFrictionControllerLayoutProbe, real_obj) == 0x04);
static_assert(
    offsetof(WorldFrictionControllerLayoutProbe, desired_speed_ws) == 0x08);
static_assert(offsetof(WorldFrictionControllerLayoutProbe,
                       desired_rot_speed_cs) == 0x18);
static_assert(offsetof(WorldFrictionControllerLayoutProbe,
                       friction_value_translation) == 0x28);
static_assert(offsetof(WorldFrictionControllerLayoutProbe,
                       friction_value_rotation) == 0x38);
static_assert(
    offsetof(WorldFrictionControllerLayoutProbe, clip_manhattan) == 0x48);

class CollisionListener final : public IVP_Listener_Collision {
public:
  void event_post_collision(IVP_Event_Collision *) override {}
  void event_collision_object_deleted(IVP_Real_Object *) override {}
  void event_friction_deleted(IVP_Event_Friction *) override {}
  void event_friction_created(IVP_Event_Friction *) override {}
};

class CustomMaterial final : public IVP_Material {
public:
  IVP_DOUBLE get_friction_factor() override { return 0.5; }
  IVP_DOUBLE get_second_friction_factor() override { return 0.25; }
  IVP_DOUBLE get_elasticity() override { return 0.1; }
  IVP_DOUBLE get_adhesion() override { return 0.0; }
  const char *get_name() override { return "BML custom material"; }
};

class StreamingUniverseManager final : public IVP_Universe_Manager {
public:
  void ensure_objects_in_environment(IVP_Real_Object *object,
                                     IVP_U_Float_Point *center,
                                     IVP_DOUBLE radius) override {
    lastObject = object;
    lastCenter = center;
    lastRadius = radius;
  }
  void object_no_longer_needed(IVP_Real_Object *object) override {
    lastObject = object;
  }
  void event_object_deleted(IVP_Real_Object *object) override {
    lastObject = object;
  }
  const IVP_Universe_Manager_Settings *provide_universe_settings() override {
    return &settings;
  }

  IVP_Universe_Manager_Settings settings;
  IVP_Real_Object *lastObject = nullptr;
  IVP_U_Float_Point *lastCenter = nullptr;
  IVP_DOUBLE lastRadius = 0.0;
};

static_assert(!std::is_abstract_v<CustomMaterial>);
static_assert(std::has_virtual_destructor_v<IVP_Material>);
static_assert(!std::is_abstract_v<StreamingUniverseManager>);
static_assert(!std::has_virtual_destructor_v<IVP_Universe_Manager>);
static_assert(
    !std::is_abstract_v<IVP_Collision_Delegator_Root_Mindist>);
static_assert(std::is_default_constructible_v<
              IVP_Collision_Delegator_Root_Mindist>);

[[maybe_unused]] void CompileRootMindistSurface(
    IVP_Collision_Delegator_Root_Mindist *root, IVP_Collision *collision,
    IVP_Real_Object *first, IVP_Real_Object *second,
    IVP_Environment *environment) {
  root->collision_is_going_to_be_deleted_event(collision);
  root->object_is_removed_from_collision_detection(first);
  (void)root->delegate_collisions_for_object(first, second);
  root->environment_is_going_to_be_deleted_event(environment);
}

[[maybe_unused]] void
CompileRetailControllerCompatibilitySurface(IVP_Controller *controller,
                                            IVP_Actuator_Spring *spring) {
  (void)controller->get_controller_name();
  (void)spring->get_only_stretch();
}

[[maybe_unused]] void
CompileCarFoundationSurface(IVP_Car_System *car,
                            IVP_CarSystemDebugData_t *debugData) {
  IVP_Template_Car_System definition(4, 2);
  IVP_Wheel_Skid_Info skidInfo[IVP_CAR_SYSTEM_MAX_WHEELS]{};
  car->do_steering(0.25f);
  car->get_skid_info(skidInfo);
  car->GetCarSystemDebugData(*debugData);
  (void)definition;
  (void)IVP_Car_System::calc_ackerman_angle(0.25f, 1.5f, 2.5f);
}

static_assert(sizeof(IVP_Car_System_Real_Wheels) == 0x450);
static_assert(std::is_base_of_v<IVP_Car_System, IVP_Car_System_Real_Wheels>);
static_assert(!std::is_abstract_v<IVP_Car_System_Real_Wheels>);
static_assert(
    std::is_same_v<
        decltype(&IVP_Car_System_Real_Wheels::change_wheel_speed_dampening),
        void (IVP_Car_System_Real_Wheels::*)(IVP_POS_WHEEL, IVP_FLOAT)>);
static_assert(std::is_same_v<
              decltype(&IVP_Car_System_Real_Wheels::get_booster_time_to_go),
              IVP_FLOAT (IVP_Car_System_Real_Wheels::*)()>);

[[maybe_unused]] void CompileGreatMatrixShortSurface(
    IVP_Great_Matrix_Many_Zero *matrix, IVP_Great_Matrix_Many_Zero *subMatrix,
    IVP_DOUBLE *values, IVP_DOUBLE *desired, int *positions) {
  matrix->calc_aligned_row_len();
  matrix->debug_fill_zero();
  matrix->set_value(1.0, 0, 0);
  (void)matrix->get_value(0, 0);
  matrix->copy_matrix(values, desired);
  matrix->copy_matrix(subMatrix);
  matrix->copy_to_sub_matrix(values, subMatrix, positions);
  matrix->matrix_multiplication(values, values);
  IVP_DOUBLE sign = 0.0;
  (void)matrix->lu_crout(positions, &sign);
  (void)matrix->lu_solve(positions);
  (void)matrix->lu_inverse(subMatrix, positions);
  (void)matrix->invert(subMatrix);
  matrix->mult();
}

[[maybe_unused]] void
CompileIncrementalLuShortSurface(IVP_Incr_L_U_Matrix *matrix) {
  matrix->add_neg_row_upwards_l_u(1, 0, 0.25);
  matrix->add_neg_col_L(0, 1, 0.25);
}

[[maybe_unused]] void CompileVectorFpuSurface(IVP_DOUBLE *first,
                                              IVP_DOUBLE *second) {
  IVP_VecFPU::fpu_add_multiple_row(first, second, 0.25, 3, IVP_TRUE);
  IVP_VecFPU::fpu_multiply_row(first, 0.5, 3, IVP_TRUE);
  IVP_VecFPU::fpu_exchange_rows(first, second, 3, IVP_TRUE);
  IVP_VecFPU::fpu_copy_rows(first, second, 3, IVP_TRUE);
  IVP_VecFPU::fpu_set_row_to_zero(first, 3, IVP_TRUE);
  (void)IVP_VecFPU::fpu_large_dot_product(first, second, 3, IVP_TRUE);
}

[[maybe_unused]] void CompileTimeManagerSurface(IVP_Time_Manager *manager,
                                                IVP_Time_Event *event,
                                                IVP_Environment *environment) {
  manager->insert_event(event, IVP_Time(1.0));
  manager->update_event(event, IVP_Time(2.0));
  (void)manager->get_event_count();
  manager->remove_event(event);
  manager->reset_time(IVP_Time(0.0));
  IVP_Event_Manager_Standard standard;
  IVP_Event_Manager *base = &standard;
  base->simulate_time_events(manager, environment, IVP_Time(3.0));
}

[[maybe_unused]] void CompileContactPointApi(IVP_Contact_Point *contactPoint,
                                             IVP_U_Float_Point *normal) {
  (void)IVP_Contact_Point_API::get_eliminated_energy(contactPoint);
  IVP_Contact_Point_API::reset_eliminated_energy(contactPoint);
  (void)IVP_Contact_Point_API::get_vert_force(contactPoint);
  IVP_Contact_Point_API::get_surface_normal_ws(contactPoint, normal);
}

[[maybe_unused]] void CompileFrictionSystemApi(
    IVP_Friction_System *system, IVP_Friction_System *secondSystem,
    IVP_Contact_Point *firstContact, IVP_Contact_Point *secondContact,
    IVP_Core *firstCore, IVP_Core *secondCore, IVP_Friction_Core_Pair *pair,
    IVP_Event_Sim *event) {
  (void)system->get_environment();
  (void)system->cores_of_friction_system.len();
  (void)system->moveable_cores_of_friction_system.len();
  (void)system->fr_pairs_of_objs.len();
  system->union_find_necessary = IVP_TRUE;
  system->fr_sys_simulated = IVP_FALSE;
  system->sum_energy_destroyed = 0.0;
  (void)system->get_cores();
  (void)system->get_moveable_cores();
  (void)system->get_friction_pairs();
  (void)system->get_friction_object_count();
  (void)system->get_fr_dist_number();
  (void)system->get_complex_not_necessary_count();
  (void)system->get_first_friction_dist();
  (void)system->get_next_friction_dist(firstContact);
  (void)system->get_prev_friction_dist(firstContact);
  (void)system->is_union_find_necessary();
  system->set_union_find_necessary(IVP_TRUE);
  (void)system->was_simulated();
  (void)system->get_sum_energy_destroyed();
  system->add_core_to_system(firstCore);
  system->remove_core_from_system(firstCore);
  system->add_fr_pair(pair);
  system->del_fr_pair(pair);
  (void)system->get_pair_info_for_objs(firstCore, secondCore);
  (void)system->find_pair_of_cores(firstCore, secondCore);
  system->exchange_friction_dists(firstContact, secondContact);
  system->add_dist_to_system(firstContact);
  system->remove_dist_from_system(firstContact);
  system->delete_friction_distance(firstContact);
  (void)system->dist_removed_update_pair_info(firstContact);
  system->dist_added_update_pair_info(firstContact);
  system->fs_recalc_all_contact_points();
  system->static_fr_oversized_matrix_panic();
  (void)system->core_is_terminal_in_fs(firstCore);
  system->reorder_mindists_for_complex();
  system->do_friction_system(event);
  system->confirm_complex_pushes();
  system->undo_complex_pushes();
  (void)system->get_num_supposed_active_frdists();
  system->apply_real_friction(event);
  system->clear_integrated_anti_energy();
  system->remove_energy_gained_by_real_friction();
  system->ease_friction_forces();
  system->calc_friction_forces(event);
  (void)system->core_is_found_in_pairs(firstCore);
  system->bubble_sort_dists_importance();
  (void)system->get_max_energy_gain();
  (void)system->kinetic_energy_of_hole_frs();
  system->fusion_friction_systems(secondSystem);
  (void)system->union_find_fr_sys();
  system->split_friction_system(firstCore);
  (void)system->get_associated_controlled_cores();
  system->get_controlled_cores(system->get_moveable_cores());
  system->reset_time(IVP_Time(0.0));
  system->do_simulation_controller(event, system->get_moveable_cores());
  (void)system->get_controller_priority();
}

[[maybe_unused]] void
CompileFrictionContactApi(IVP_Contact_Point *contactPoint, IVP_Mindist *mindist,
                          IVP_Friction_System *system,
                          IVP_Friction_Core_Pair *pair,
                          IVP_Friction_Info_For_Core *coreInfo) {
  IVP_Material *materials[2]{};
  (void)contactPoint->get_synapse(0);
  (void)contactPoint->get_next_friction_dist();
  (void)contactPoint->get_prev_friction_dist();
  (void)contactPoint->get_friction_system();
  (void)contactPoint->get_friction_factor();
  contactPoint->set_friction_to_neutral();
  contactPoint->get_material_info(materials);
  contactPoint->recalc_friction_s_vals();
  contactPoint->recompute_friction();

  coreInfo->set_all_dists_of_obj_neutral();
  coreInfo->friction_info_insert_friction_dist(contactPoint);
  coreInfo->friction_info_delete_friction_dist(contactPoint);
  (void)coreInfo->dist_number();

  (void)pair->get_contact_points();
  (void)pair->number_of_pair_dists();
  pair->add_fr_dist_obj_pairs(contactPoint);
  pair->del_fr_dist_obj_pairs(contactPoint);
  (void)pair->check_all_fr_mindists_to_be_valid(system);
  pair->remove_energy_gained_by_real_friction();

  IVP_BOOL wasSuccessful = IVP_FALSE;
  (void)IVP_Friction_Manager::generate_contact_point(mindist, &wasSuccessful);
  (void)IVP_Friction_Manager::get_associated_contact_point(mindist);
}

[[maybe_unused]] void CompileMutualEnergizerApi(IVP_Mutual_Energizer *energizer,
                                                IVP_Core *firstCore,
                                                IVP_Core *secondCore) {
  (void)IVP_Mutual_Energizer::calc_energy_potential(1.0, 2.0, 3.0, 0.5,
                                                    1.0 / 3.0);
  energizer->init_mutual_energizer(firstCore, secondCore);
  energizer->calc_energy_potential();
  energizer->destroy_percent_energy(0.25);
  (void)energizer->core[0];
  (void)energizer->whole_mutual_energy;
}

[[maybe_unused]] void CompileFrictionControllerApi(
    IVP_Friction_System *system, IVP_Friction_Sys_Static *staticController,
    IVP_Friction_Sys_Energy *energyController, IVP_Event_Sim *event,
    IVP_U_Vector<IVP_Core> *cores, IVP_Core *core) {
  (void)system->get_static_friction_controller();
  (void)system->get_energy_friction_controller();
  staticController->core_is_going_to_be_deleted_event(core);
  staticController->do_simulation_controller(event, cores);
  (void)staticController->get_controller_priority();
  (void)staticController->get_minimum_simulation_frequency();
  energyController->core_is_going_to_be_deleted_event(core);
  energyController->do_simulation_controller(event, cores);
  (void)energyController->get_controller_priority();
  (void)energyController->get_mimumum_simulation_frequency();
}

[[maybe_unused]] void CompileFrictionSolverApi(
    IVP_Friction_Solver *solver, IVP_Friction_System *system,
    IVP_Friction_Core_Pair *pair, IVP_Contact_Point *firstContact,
    IVP_Contact_Point *secondContact, IVP_Impact_Solver_Long_Term *information,
    IVP_U_Float_Point *firstVector, IVP_U_Float_Point *secondVector,
    IVP_U_Memory *memory, int *activePositions, IVP_Event_Sim *event) {
  IVP_Friction_Solver constructed(system, event);
  (void)constructed;
  (void)IVP_Friction_Solver::get_closing_speed_core_i(
      information, 0, firstVector, secondVector);
  (void)IVP_Friction_Solver::get_inv_virtual_mass(information);
  IVP_Friction_Solver::apply_impulse(information, 1.0);
  IVP_Friction_Solver::async_apply_impulse(information, 1.0);
  IVP_Friction_Solver::test_push_core_i(information, 0, firstVector,
                                        secondVector, -1.0f);
  (void)IVP_Friction_Solver::calc_desired_gap_speed(0.0, 0.0, 1.0f);
  IVP_Friction_Solver::ease_test_two_mindists(firstContact, secondContact,
                                              firstVector);
  IVP_Friction_Solver::ease_friction_pair(pair, memory);
  IVP_Friction_Solver::ease_two_mindists(firstContact, secondContact,
                                         firstVector, secondVector, 0.5);
  solver->setup_coords_mindists(system);
  (void)solver->calc_solver_PSI(system, activePositions);
  solver->calc_distance_matrix_column(0, information->contact_core[0],
                                      information->friction.friction_infos[0],
                                      firstVector, secondVector);
  (void)solver->test_gauss_solution_suggestion(nullptr, activePositions, 0,
                                               memory);
  solver->solve_linear_equation_and_push(system, activePositions, 0, memory);
  (void)solver->do_resulting_pushes(system);
  solver->normize_constraint_equ();
  solver->factor_result_vec();
  solver->debug_distance_after_push(0);
}

[[maybe_unused]] void CompileMindistReadSurface(IVP_Mindist_Base *base,
                                                IVP_Mindist *mindist) {
  IVP_Real_Object *objects[2]{};
  const IVP_Compact_Ledge *ledges[2]{};
  base->get_objects(objects);
  base->get_ledges(ledges);
  (void)base->get_mindist_synapse(0);
  (void)base->get_length();
  (void)base->get_mindist_status();
  (void)base->get_mindist_function();
  (void)base->get_recalc_result();
  (void)base->get_contact_plane();
  (void)mindist->get_synapse(0);
  (void)mindist->get_sorted_synapse(0);
  (void)mindist->get_environment();
  (void)mindist->is_recursive();
}

[[maybe_unused]] void CompileRecursiveMindistSurface(
    IVP_Mindist_Recursive *mindist, IVP_Mindist_Manager *manager) {
  (void)mindist->is_recursive();
  mindist->mindist_rescue_push();
  mindist->rec_hull_limit_exceeded_event();
  mindist->exact_mindist_went_invalid(manager);
  mindist->do_impact();
  (void)mindist->recursive_status;
  (void)mindist->mindists.len();
}

static_assert(!std::is_constructible_v<
              IVP_Mindist_Recursive, IVP_Environment *,
              IVP_Collision_Delegator *>);

[[maybe_unused]] void CompileMindistManagedSurface(
    IVP_Mindist *mindist, IVP_Mindist_Manager *manager, IVP_Real_Object *first,
    IVP_Real_Object *second, const IVP_Compact_Ledge *firstLedge,
    const IVP_Compact_Ledge *secondLedge, const IVP_Compact_Edge *firstEdge,
    const IVP_Compact_Edge *secondEdge, IVP_U_FVector<IVP_Collision> *existing,
    IVP_Collision_Delegator *delegator, IVP_Friction_System **frictionSystem,
    IVP_Simulation_Unit *simulationUnit) {
  IVP_BOOL havingNew = IVP_FALSE;
  mindist->init_mindist(first, second, firstEdge, secondEdge);
  (void)mindist->recalc_mindist();
  (void)mindist->recalc_invalid_mindist();
  mindist->update_exact_mindist_events(IVP_TRUE, IVP_EH_SMALL_DELAY);
  mindist->create_cp_in_advance_pretension(first, 0.01f);
  (void)mindist->try_to_generate_managed_friction(frictionSystem, &havingNew,
                                                  simulationUnit, IVP_TRUE);

  IVP_Mindist_Manager::create_exact_mindists(
      first, second, 0.25, existing, firstLedge, secondLedge, firstLedge,
      secondLedge, delegator);
  manager->insert_exact_mindist(mindist);
  manager->insert_and_recalc_exact_mindist(mindist);
  manager->insert_and_recalc_phantom_mindist(mindist);
  manager->insert_invalid_mindist(mindist);
  manager->remove_exact_mindist(mindist);
  manager->remove_invalid_mindist(mindist);
  manager->remove_hull_mindist(mindist);
  IVP_Mindist_Manager::mindist_entered_phantom(mindist);
  IVP_Mindist_Manager::mindist_left_phantom(mindist);
  IVP_Mindist_Manager::insert_hull_mindist(mindist, 0.5f);
  IVP_Mindist_Manager::insert_hull_mindist(mindist, 0.25f, 0.5f);
  IVP_Mindist_Manager::insert_lazy_hull_mindist(mindist, 0.25f, 0.5f);
  manager->recalc_all_exact_mindists();
  manager->recalc_exact_mindist(mindist);
  manager->recalc_all_exact_wheel_mindist();
  manager->recalc_all_exact_mindists_events();
  manager->recheck_ov_element(first);
  manager->enable_collision_detection_for_object(first);
}

class ObjectListener final : public IVP_Listener_Object {
public:
  void event_object_deleted(IVP_Event_Object *) override {}
  void event_object_created(IVP_Event_Object *) override {}
  void event_object_revived(IVP_Event_Object *) override {}
  void event_object_frozen(IVP_Event_Object *) override {}
};

class PsiListener final : public IVP_Listener_PSI {
public:
  void event_PSI(IVP_Event_PSI *) override {}
  void environment_will_be_deleted(IVP_Environment *) override {}
};

class ConstraintListener final : public IVP_Listener_Constraint {
public:
  void event_constraint_broken(IVP_Constraint *) override {}
};

class CollisionDelegator final : public IVP_Collision_Delegator_Root {
public:
  void collision_is_going_to_be_deleted_event(IVP_Collision *) override {}
  void object_is_removed_from_collision_detection(IVP_Real_Object *) override {}
  IVP_Collision *delegate_collisions_for_object(IVP_Real_Object *,
                                                IVP_Real_Object *) override {
    return nullptr;
  }
  void environment_is_going_to_be_deleted_event(IVP_Environment *) override {}
};

class CollisionCheck final : public IVP_Collision {
public:
  explicit CollisionCheck(IVP_Collision_Delegator *delegator)
      : IVP_Collision(delegator) {}

  void simulate_time_event(IVP_Environment *) override {}
  void get_objects(IVP_Real_Object *objects[2]) override {
    objects[0] = nullptr;
    objects[1] = nullptr;
  }
  void get_ledges(const IVP_Compact_Ledge *ledges[2]) override {
    ledges[0] = nullptr;
    ledges[1] = nullptr;
  }
  void
  delegator_is_going_to_be_deleted_event(IVP_Collision_Delegator *) override {}
};

[[maybe_unused]] void CompileCollisionDelegationSurface() {
  CollisionDelegator delegator;
  CollisionCheck collision(&delegator);
  collision.set_fvector_index(-1, 3);
  (void)collision.get_fvector_index(0);
  collision.delegator_is_going_to_be_deleted_event(&delegator);
  delegator.change_spawned_mindist_count(1);
  (void)delegator.get_spawned_mindist_count();
}

class PhantomListener final : public IVP_Listener_Phantom {
public:
  void mindist_entered_volume(IVP_Controller_Phantom *,
                              IVP_Mindist_Base *) override {}
  void mindist_left_volume(IVP_Controller_Phantom *,
                           IVP_Mindist_Base *) override {}
  void core_entered_volume(IVP_Controller_Phantom *, IVP_Core *) override {}
  void core_left_volume(IVP_Controller_Phantom *, IVP_Core *) override {}
  void phantom_is_going_to_be_deleted_event(IVP_Controller_Phantom *) override {
  }
};

class SpringListener final : public IVP_Listener_Spring {
public:
  void event_spring_broken(IVP_Actuator_Spring *) override {}
};

class CheckDistListener final : public IVP_Listener_Check_Dist_Event {
public:
  void check_dist_event(IVP_Actuator_Check_Dist *, IVP_BOOL) override {}
  void
  check_dist_is_going_to_be_deleted_event(IVP_Actuator_Check_Dist *) override {}
};

class StiffSpringListener final : public IVP_Listener_Stiff_Spring {
public:
  void event_stiff_spring_broken(IVP_Controller_Stiff_Spring *) override {}
};

static_assert(sizeof(CheckDistListener) == sizeof(void *));
static_assert(sizeof(StiffSpringListener) == sizeof(void *));
static_assert(
    std::is_base_of_v<IVP_U_Active_Float_Listener, IVP_Actuator_Spring_Active>);
static_assert(
    std::is_base_of_v<IVP_U_Active_Float_Listener, IVP_Actuator_Force_Active>);
static_assert(
    std::is_base_of_v<IVP_U_Active_Float_Listener, IVP_Actuator_Torque_Active>);
static_assert(std::is_base_of_v<IVP_U_Active_Float_Listener,
                                IVP_Actuator_Rot_Mot_Active>);
static_assert(std::is_base_of_v<IVP_U_Active_Float_Listener,
                                IVP_Controller_Stiff_Spring_Active>);
static_assert(std::has_virtual_destructor_v<IVP_Actuator_Spring>);
static_assert(std::has_virtual_destructor_v<IVP_Actuator_Force>);
static_assert(std::has_virtual_destructor_v<IVP_Actuator_Torque>);
static_assert(std::has_virtual_destructor_v<IVP_Actuator_Rot_Mot>);
static_assert(std::has_virtual_destructor_v<IVP_Controller_Stiff_Spring>);
static_assert(
    std::is_same_v<decltype(&IVP_Controller_Stiff_Spring::set_constant),
                   void (IVP_Controller_Stiff_Spring::*)(IVP_DOUBLE)>);
static_assert(
    std::is_same_v<decltype(&IVP_Controller_Stiff_Spring::get_constant),
                   IVP_FLOAT (IVP_Controller_Stiff_Spring::*)()>);
static_assert(std::is_same_v<
              decltype(&IVP_Environment::create_force),
              IVP_Actuator_Force *(IVP_Environment::*)(IVP_Template_Force *)>);
static_assert(std::is_same_v<decltype(&IVP_Environment::create_torque),
                             IVP_Actuator_Torque *(
                                 IVP_Environment::*)(IVP_Template_Torque *)>);
static_assert(std::is_same_v<decltype(&IVP_Environment::create_rotmot),
                             IVP_Actuator_Rot_Mot *(
                                 IVP_Environment::*)(IVP_Template_Rot_Mot *)>);
static_assert(
    std::is_same_v<decltype(&IVP_Environment::create_check_dist),
                   IVP_Actuator_Check_Dist *(
                       IVP_Environment::*)(IVP_Template_Check_Dist *)>);
static_assert(
    std::is_same_v<decltype(&IVP_Environment::create_stabilizer),
                   IVP_Actuator_Stabilizer *(
                       IVP_Environment::*)(IVP_Template_Stabilizer *)>);
static_assert(std::has_virtual_destructor_v<IVP_Anchor_Check_Dist>);
static_assert(std::is_base_of_v<IVP_Actuator, IVP_Actuator_Four_Point>);
static_assert(std::has_virtual_destructor_v<IVP_Actuator_Stabilizer>);
static_assert(std::has_virtual_destructor_v<IVP_Actuator_Suspension>);
static_assert(
    std::is_same_v<decltype(&IVP_Environment::create_suspension),
                   IVP_Actuator_Suspension *(
                       IVP_Environment::*)(IVP_Template_Suspension *)>);
static_assert(std::is_same_v<
              decltype(&IVP_Actuator_Suspension::set_spring_damp_compression),
              void (IVP_Actuator_Suspension::*)(IVP_FLOAT)>);
static_assert(
    std::is_same_v<decltype(&IVP_Actuator_Suspension::set_max_body_force),
                   void (IVP_Actuator_Suspension::*)(IVP_FLOAT)>);
class CompileCoreAttachment {
public:
  CompileCoreAttachment(IVP_Attacher_To_Cores<CompileCoreAttachment> *,
                        IVP_Core *) {}
};
using CompileCoreAttacher = IVP_Attacher_To_Cores<CompileCoreAttachment>;
static_assert(
    std::is_same_v<
        decltype(&CompileCoreAttacher::attachment_is_going_to_be_deleted),
        void (CompileCoreAttacher::*)(CompileCoreAttachment *, IVP_Core *)>);
static_assert(std::is_destructible_v<IVP_Template_Real_Object>);
static_assert(std::is_base_of_v<IVP_U_Point, IVP_Template_Point>);
static_assert(std::is_default_constructible_v<IVP_Template_Polygon>);
static_assert(std::is_destructible_v<IVP_Template_Polygon>);
static_assert(std::is_same_v<decltype(&IVP_Template_Surface::get_surface_index),
                             int (IVP_Template_Surface::*)()>);
static_assert(std::is_same_v<decltype(&IVP_Template_Surface::set_line),
                             void (IVP_Template_Surface::*)(int, int, char)>);
static_assert(std::is_destructible_v<IVP_Environment_Manager>);
static_assert(!std::is_default_constructible_v<IVP_Environment_Manager>);
static_assert(std::is_default_constructible_v<IVP_Template_Controller_Golem>);
static_assert(
    std::is_default_constructible_v<IVP_Template_Controller_Floating>);
static_assert(std::is_abstract_v<IVP_Controller_Floating>);
static_assert(
    std::is_base_of_v<IVP_Controller_Independent, IVP_Controller_Floating>);
static_assert(
    std::is_same_v<decltype(&IVP_Controller_Floating::set_current_distance),
                   void (IVP_Controller_Floating::*)(IVP_DOUBLE)>);
static_assert(
    std::is_same_v<decltype(&IVP_Controller_Floating::get_ray_direction_ws),
                   const IVP_U_Float_Point *(IVP_Controller_Floating::*)()>);
static_assert(
    std::is_default_constructible_v<IVP_Template_Controller_World_Friction>);
static_assert(std::is_base_of_v<IVP_Controller_Independent,
                                IVP_Controller_World_Friction>);
static_assert(
    std::is_same_v<decltype(&IVP_Controller_World_Friction::
                                set_friction_value_translation),
                   void (IVP_Controller_World_Friction::*)(IVP_U_Point)>);
static_assert(
    std::is_default_constructible_v<IVP_Template_Constraint_Fixed_Keyframed>);

class GolemPolicy final : public IVP_Controller_Golem {
public:
  using IVP_Controller_Golem::IVP_Controller_Golem;

  IVP_RETURN_TYPE resolve_for_problem(IVP_Event_Sim *,
                                      IVP_GOLEM_PROBLEM) override {
    return IVP_FAULT;
  }
};

static_assert(std::is_abstract_v<IVP_Controller_Golem>);
static_assert(!std::is_abstract_v<GolemPolicy>);
static_assert(std::is_base_of_v<IVP_Controller_Motion, IVP_Controller_Golem>);
static_assert(std::has_virtual_destructor_v<IVP_Controller_Golem>);
static_assert(
    std::is_same_v<decltype(&IVP_Controller_Golem::resolve_for_problem),
                   IVP_RETURN_TYPE (IVP_Controller_Golem::*)(
                       IVP_Event_Sim *, IVP_GOLEM_PROBLEM)>);
static_assert(
    std::is_same_v<decltype(&IVP_Controller_Golem::set_prime_position),
                   void (IVP_Controller_Golem::*)(const IVP_U_Point *,
                                                  const IVP_U_Float_Point *,
                                                  const IVP_Time &)>);
static_assert(
    std::is_same_v<
        decltype(&IVP_Controller_Golem::set_prime_orientation),
        void (IVP_Controller_Golem::*)(const IVP_U_Quat *, const IVP_Time &,
                                       const IVP_U_Quat *, IVP_FLOAT)>);

[[maybe_unused]] void CompileGolemSurface(GolemPolicy *golem,
                                          const IVP_U_Point *position,
                                          const IVP_U_Float_Point *velocity,
                                          const IVP_U_Quat *orientation,
                                          const IVP_Time &time) {
  golem->set_prime_position(position, velocity, time);
  golem->set_prime_orientation(orientation, time);
  IVP_Controller *controller = golem;
  (void)controller;
}

class MinimalConstraint final : public IVP_Constraint {
protected:
  void do_simulation_controller(IVP_Event_Sim *,
                                IVP_U_Vector<IVP_Core> *) override {}
};

static_assert(std::is_abstract_v<IVP_Constraint>);
static_assert(!std::is_abstract_v<MinimalConstraint>);
static_assert(!std::is_abstract_v<IVP_Constraint_Fixed_Keyframed>);
static_assert(std::is_base_of_v<IVP_Controller_Dependent,
                                IVP_Constraint_Fixed_Keyframed>);
static_assert(std::has_virtual_destructor_v<IVP_Constraint_Fixed_Keyframed>);
static_assert(
    std::is_constructible_v<IVP_Constraint_Fixed_Keyframed, IVP_Real_Object *,
                            IVP_Real_Object *,
                            const IVP_Template_Constraint_Fixed_Keyframed *>);
static_assert(
    std::is_same_v<decltype(&IVP_Constraint_Fixed_Keyframed::get_environment),
                   IVP_Environment *(IVP_Constraint_Fixed_Keyframed::*)()>);
static_assert(
    std::is_same_v<
        decltype(&IVP_Constraint_Fixed_Keyframed::set_prime_position_Ros),
        void (IVP_Constraint_Fixed_Keyframed::*)(
            const IVP_U_Point *, const IVP_U_Float_Point *, const IVP_Time &)>);
static_assert(
    std::is_same_v<
        decltype(&IVP_Constraint_Fixed_Keyframed::set_prime_orientation_Ros),
        void (IVP_Constraint_Fixed_Keyframed::*)(
            const IVP_U_Quat *, const IVP_Time &, const IVP_U_Quat *,
            IVP_FLOAT)>);

[[maybe_unused]] void CompileFixedKeyframedSurface(
    IVP_Constraint_Fixed_Keyframed *constraint, const IVP_U_Point *position,
    const IVP_U_Float_Point *velocity, const IVP_U_Quat *orientation,
    const IVP_Time &time) {
  (void)constraint->get_environment();
  constraint->set_prime_position_Ros(position, velocity, time);
  constraint->set_prime_orientation_Ros(orientation, time);
  IVP_Controller *controller = constraint;
  (void)controller->get_associated_controlled_cores();
}

class Controller final : public IVP_Controller {
public:
  IVP_U_Vector<IVP_Core> *get_associated_controlled_cores() override {
    return nullptr;
  }
  void do_simulation_controller(IVP_Event_Sim *,
                                IVP_U_Vector<IVP_Core> *) override {}
  IVP_CONTROLLER_PRIORITY get_controller_priority() override {
    return IVP_CP_MOTION;
  }
};

class TwoPointActuator final : public IVP_Actuator_Two_Point {
public:
  TwoPointActuator(IVP_Environment *environment,
                   IVP_Template_Two_Point *definition)
      : IVP_Actuator_Two_Point(environment, definition, IVP_ACTUATOR_TYPE_ETC) {
  }

  void do_simulation_controller(IVP_Event_Sim *,
                                IVP_U_Vector<IVP_Core> *) override {}
};

class OnlyDifferentObjects final : public IVP_Collision_Filter {
public:
  IVP_BOOL
  check_objects_for_collision_detection(IVP_Real_Object *left,
                                        IVP_Real_Object *right) override {
    return left != right ? IVP_TRUE : IVP_FALSE;
  }
  void environment_will_be_deleted(IVP_Environment *) override {}
};

class CoreSetListener final : public IVP_Listener_Set_Active<IVP_Core> {
public:
  void element_added(IVP_U_Set_Active<IVP_Core> *, IVP_Core *) override {}
  void element_removed(IVP_U_Set_Active<IVP_Core> *, IVP_Core *) override {}
  void pset_is_going_to_be_deleted(IVP_U_Set_Active<IVP_Core> *) override {}
};

static_assert(
    std::is_same_v<decltype(&IVP_Real_Object::get_m_world_f_object_AT),
                   void (IVP_Real_Object::*)(IVP_U_Matrix *) const>);
using RealObjectVoidMethod = void (IVP_Real_Object::*)();
static_assert(
    std::is_same_v<decltype(&IVP_Real_Object::recompile_values_changed),
                   RealObjectVoidMethod>);
static_assert(
    std::is_same_v<decltype(&IVP_Real_Object::recompile_material_changed),
                   RealObjectVoidMethod>);
static_assert(
    std::is_same_v<decltype(&IVP_Real_Object::delete_and_check_vicinity),
                   RealObjectVoidMethod>);
static_assert(
    std::is_same_v<decltype(&IVP_Real_Object::beam_object_to_new_position),
                   void (IVP_Real_Object::*)(const IVP_U_Quat *,
                                             const IVP_U_Point *, IVP_BOOL)>);
static_assert(std::is_same_v<decltype(&IVP_Real_Object::change_unmovable_flag),
                             void (IVP_Real_Object::*)(IVP_BOOL)>);
static_assert(sizeof(IVP_Calc_Next_PSI_Solver) == 0x04);
static_assert(sizeof(IVP_Vector_of_Cores_2) == 0x10);
static_assert(sizeof(IVP_Sim_Unit_Controller_Core_List) == 0x14);
static_assert(sizeof(IVP_Simulation_Unit) == 0x24);
static_assert(sizeof(IVP_Sim_Units_Manager) == 0x1B0);
static_assert(
    std::is_same_v<decltype(&IVP_Simulation_Unit::sim_unit_calc_redundants),
                   void (IVP_Simulation_Unit::*)()>);
static_assert(
    std::is_same_v<decltype(&IVP_Simulation_Unit::sim_unit_calc_movement_state),
                   IVP_BOOL (IVP_Simulation_Unit::*)(IVP_Environment *)>);
static_assert(
    std::is_same_v<decltype(&IVP_Simulation_Unit::simulate_single_sim_unit_psi),
                   void (IVP_Simulation_Unit::*)(IVP_Event_Sim *,
                                                 IVP_U_Vector<IVP_Core> *)>);
static_assert(std::is_same_v<decltype(&IVP_Simulation_Unit::split_sim_unit),
                             void (IVP_Simulation_Unit::*)(IVP_Core *)>);
static_assert(
    std::is_same_v<decltype(&IVP_Simulation_Unit::perform_test_and_split),
                   void (IVP_Simulation_Unit::*)()>);
static_assert(
    std::is_same_v<decltype(&IVP_Simulation_Unit::throw_cores_into_my_sim_unit),
                   void (IVP_Simulation_Unit::*)(IVP_Simulation_Unit *)>);
static_assert(
    std::is_same_v<decltype(&IVP_Simulation_Unit::fusion_simulation_unities),
                   void (IVP_Simulation_Unit::*)(IVP_Simulation_Unit *)>);
static_assert(std::is_same_v<
              decltype(&IVP_Simulation_Unit::add_controller_of_core),
              void (IVP_Simulation_Unit::*)(IVP_Core *, IVP_Controller *)>);
static_assert(
    std::is_same_v<decltype(&IVP_Simulation_Unit::sim_unit_sort_controllers),
                   void (IVP_Simulation_Unit::*)()>);
static_assert(std::is_same_v<
              decltype(&IVP_Simulation_Unit::sim_unit_exchange_controllers),
              void (IVP_Simulation_Unit::*)(int, int)>);
static_assert(offsetof(IVP_Core, sim_unit_of_core) == 0x1D4);
static_assert(offsetof(IVP_Core, tmp) == 0x228);
static_assert(offsetof(IVP_Environment, sim_units_manager) == 0x08);
static_assert(
    std::is_same_v<decltype(&IVP_Sim_Units_Manager::simulate_sim_units_psi),
                   void (IVP_Sim_Units_Manager::*)(IVP_Environment *,
                                                   IVP_U_Vector<IVP_Core> *)>);
static_assert(
    std::is_same_v<decltype(&IVP_Core::calc_virt_mass),
                   IVP_DOUBLE (IVP_Core::*)(const IVP_U_Float_Point *,
                                            const IVP_U_Float_Point *) const>);
static_assert(std::is_base_of_v<IVP_Core_Fast_Static, IVP_Core_Fast_PSI>);
static_assert(std::is_base_of_v<IVP_Core_Fast_PSI, IVP_Core_Fast>);
static_assert(std::is_base_of_v<IVP_Core_Fast, IVP_Core>);
static_assert(sizeof(IVP_Core_Friction_Info) == 0x04);
static_assert(sizeof(IVP_Old_Sync_Rot_Z) == 0x38);
static_assert(std::is_same_v<
              decltype(std::declval<IVP_Core &>().fast_piling_allowed_flag),
              IVP_BOOL>);
static_assert(std::is_same_v<
              decltype(std::declval<IVP_Core &>().tmp_null.old_sync_info),
              IVP_Old_Sync_Rot_Z *>);
static_assert(sizeof(IVP_Real_Object_Flags) == 0x04);
static_assert(std::is_same_v<
              decltype(std::declval<IVP_Real_Object &>()
                           .flags.object_movement_state),
              unsigned int>);
static_assert(std::is_same_v<decltype(&IVP_Core_Fast_Static::get_inv_masses),
                             const IVP_U_Point_4 *(IVP_Core_Fast_Static::*)()>);
static_assert(std::is_same_v<decltype(&IVP_Core_Fast_Static::get_mass),
                             IVP_FLOAT (IVP_Core_Fast_Static::*)() const>);
static_assert(
    std::is_same_v<decltype(&IVP_Core_Fast_Static::get_rot_inertia),
                   const IVP_U_Float_Point *(IVP_Core_Fast_Static::*)() const>);
static_assert(
    std::is_same_v<decltype(&IVP_Core_Fast_Static::get_inv_rot_inertia),
                   const IVP_U_Float_Point *(IVP_Core_Fast_Static::*)() const>);
static_assert(std::is_same_v<decltype(&IVP_Core_Fast_Static::get_inv_mass),
                             IVP_FLOAT (IVP_Core_Fast_Static::*)() const>);
static_assert(std::is_same_v<decltype(&IVP_Compact_Surface::get_size),
                             int (IVP_Compact_Surface::*)() const>);
static_assert(
    std::is_same_v<decltype(&IVP_Compact_Surface::get_compact_ledge_tree_root),
                   const IVP_Compact_Ledgetree_Node *(IVP_Compact_Surface::*)()
                       const>);
using CoreVoidMethod = void (IVP_Core::*)();
static_assert(std::is_destructible_v<IVP_Core>);
static_assert(
    std::is_same_v<decltype(&IVP_Core::apply_velocity_limit), CoreVoidMethod>);
static_assert(std::is_same_v<decltype(&IVP_Core::freeze_simulation_core),
                             CoreVoidMethod>);
static_assert(
    std::is_same_v<decltype(&IVP_Core::ensure_all_core_objs_in_simulation),
                   CoreVoidMethod>);
static_assert(
    std::is_same_v<decltype(&IVP_Core::ensure_all_core_objs_in_simulation_now),
                   CoreVoidMethod>);
static_assert(
    std::is_same_v<decltype(&IVP_Core::ensure_core_in_simulation_delayed),
                   CoreVoidMethod>);
static_assert(std::is_same_v<decltype(&IVP_Core::init_core_for_simulation),
                             CoreVoidMethod>);
static_assert(std::is_same_v<decltype(&IVP_Core::reset_freeze_check_values),
                             CoreVoidMethod>);
static_assert(std::is_same_v<decltype(&IVP_Core::revive_adjacent_to_unmoveable),
                             CoreVoidMethod>);
static_assert(std::is_same_v<decltype(&IVP_Core::stop_physical_movement),
                             CoreVoidMethod>);
static_assert(std::is_same_v<decltype(&IVP_Core::synchronize_with_rot_z),
                             CoreVoidMethod>);
static_assert(
    std::is_same_v<decltype(&IVP_Core::update_exact_mindist_events_of_core),
                   CoreVoidMethod>);
static_assert(
    std::is_same_v<decltype(&IVP_Core::values_changed_recalc_redundants),
                   CoreVoidMethod>);
static_assert(std::is_same_v<decltype(&IVP_Core::calc_movement_state),
                             IVP_Movement_Type (IVP_Core::*)(IVP_Time)>);
static_assert(std::is_same_v<decltype(&IVP_Core::clip_velocity),
                             void (IVP_Core::*)(IVP_U_Float_Point *,
                                                IVP_U_Float_Point *)>);
static_assert(
    std::is_same_v<decltype(&IVP_Core::create_collision_merged_core_with),
                   void (IVP_Core::*)(IVP_Core *)>);
static_assert(std::is_same_v<decltype(&IVP_Environment::simulate_dtime),
                             void (IVP_Environment::*)(IVP_DOUBLE)>);
static_assert(
    std::is_same_v<decltype(static_cast<IVP_Time (IVP_Environment::*)()>(
                       &IVP_Environment::get_current_time)),
                   IVP_Time (IVP_Environment::*)()>);
static_assert(std::is_same_v<decltype(&IVP_Environment::simulate_until),
                             void (IVP_Environment::*)(IVP_Time)>);
static_assert(std::is_same_v<
              decltype(static_cast<void (IVP_U_Quat::*)(const IVP_U_Matrix3 *)>(
                  &IVP_U_Quat::set_quaternion)),
              void (IVP_U_Quat::*)(const IVP_U_Matrix3 *)>);
static_assert(sizeof(IVP_FLOAT) == 4);
static_assert(sizeof(IVP_DOUBLE) == 8);
static_assert(sizeof(IVP_U_Float_Hesse) == 0x10);
static_assert(sizeof(IVP_U_Hesse) == 0x20);
static_assert(sizeof(IVP_U_Straight) == 0x40);
static_assert(sizeof(IVP_U_Plain) == 0x80);
static_assert(sizeof(IVP_U_Min_List_Element) == 0x10);
static_assert(sizeof(IVP_U_Min_List) == 0x14);
static_assert(sizeof(IVP_U_Min_List_Enumerator) == 0x08);
static_assert(sizeof(IVP_Template_Constraint) == 0x200);
static_assert(sizeof(IVP_Template_Buoyancy) == 0x4C);
static_assert(sizeof(IVP_SurfaceBuilder_Ledge_Soup) == 0xA0);

[[maybe_unused]] void
CompileGeometrySurface(const IVP_U_Point *point0, const IVP_U_Point *point1,
                       const IVP_U_Point *point2,
                       const IVP_U_Float_Point *floatPoint0,
                       const IVP_U_Float_Point *floatPoint1,
                       const IVP_U_Float_Point *floatPoint2) {
  IVP_U_Hesse plane;
  plane.calc_hesse(point0, point1, point2);
  plane.calc_hesse(floatPoint0, floatPoint1, floatPoint2);
  plane.calc_hesse_val(point0);
  (void)plane.get_dist(point0);
  (void)plane.get_dist(floatPoint0);
  plane.mult_hesse(2.0);
  plane.normize();

  IVP_U_Point projected;
  plane.proj_on_plane(point0, &projected);
  IVP_U_Straight straight(point0, point1);
  straight.set(floatPoint0, floatPoint1);
  straight.calc_orthogonal_vec_from_point(point2, &projected);
  (void)straight.get_quad_dist_to_point(&projected);
  (void)plane.calc_intersect_with(&straight, &projected);

  IVP_DOUBLE distance = 0.0;
  IVP_U_Straight other(point1, point2);
  (void)straight.calc_intersect_with(&other, &projected, &distance);

  IVP_U_Plain spanningPlane(&plane);
  IVP_U_Plain pointsPlane(point0, point1, point2);
  (void)spanningPlane.calc_intersect_with(&pointsPlane, &straight);
}

[[maybe_unused]] void CompileLinearMathSurface(IVP_U_Point *doublePoint,
                                               IVP_U_Float_Point *floatPoint,
                                               IVP_U_Hesse *plane0,
                                               IVP_U_Hesse *plane1,
                                               IVP_U_Hesse *plane2,
                                               IVP_U_Quat *quaternion) {
  const IVP_FLOAT source[3] = {1.0f, 2.0f, 3.0f};

  IVP_U_Float_Point floatCopy(floatPoint);
  IVP_U_Float_Point floatFromDouble(doublePoint);
  floatCopy.byte_swap();
  floatCopy.calc_cross_product(floatPoint, &floatFromDouble);
  floatCopy.inline_calc_cross_product_and_normize(floatPoint, &floatFromDouble);
  floatCopy.inline_set_vert_to_area_defined_by_three_points(
      floatPoint, &floatFromDouble, floatPoint);
  floatCopy.inline_subtract_and_mult(doublePoint, doublePoint, 0.5);
  floatCopy.subtract(doublePoint, doublePoint);
  floatCopy.set_multiple(doublePoint, 2.0);
  floatCopy.set_multiple(quaternion, 2.0);
  (void)floatCopy.fast_real_length();
  floatCopy.line_sqrt();
  floatCopy.rotate(IVP_INDEX_Z, 0.25f);
  floatCopy.print("float");

  IVP_U_Point pointCopy;
  pointCopy.add(doublePoint, doublePoint);
  pointCopy.add(floatPoint, floatPoint);
  pointCopy.byte_swap();
  (void)pointCopy.fast_normize();
  (void)pointCopy.fast_real_length();
  pointCopy.inline_calc_cross_product_and_normize(doublePoint, doublePoint);
  pointCopy.inline_set_vert_to_area_defined_by_three_points(
      doublePoint, doublePoint, doublePoint);
  pointCopy.inline_set_vert_to_area_defined_by_three_points(
      floatPoint, floatPoint, doublePoint);
  pointCopy.inline_set_vert_to_area_defined_by_three_points(
      floatPoint, floatPoint, floatPoint);
  pointCopy.inline_subtract_and_mult(doublePoint, doublePoint, 0.5);
  pointCopy.inline_subtract_and_mult(floatPoint, floatPoint, 0.5);
  pointCopy.print("double");
  pointCopy.rotate(IVP_INDEX_X, 0.25f);
  pointCopy.set(source);
  pointCopy.set(quaternion);
  pointCopy.set_interpolate(floatPoint, floatPoint, 0.5);
  pointCopy.set_multiple(quaternion, 2.0);
  pointCopy.set_orthogonal_part(doublePoint, doublePoint);
  pointCopy.set_orthogonal_part(doublePoint, floatPoint);
  pointCopy.set_pairwise_mult(doublePoint, floatPoint);
  pointCopy.solve_quadratic_equation_fast(doublePoint);
  pointCopy.subtract(floatPoint);
  pointCopy.subtract(floatPoint, floatPoint);
  pointCopy.subtract(floatPoint, doublePoint);
  pointCopy.subtract(doublePoint, floatPoint);
  (void)pointCopy.set_crossing(plane0, plane1, plane2);
}

[[maybe_unused]] void CompileMatrixQuaternionSurface(
    IVP_U_Matrix3 *matrix3, IVP_U_Matrix *matrix, IVP_U_Point *point,
    IVP_U_Float_Point *floatPoint, IVP_U_Quat *quaternion,
    IVP_U_Float_Quat *floatQuaternion, FILE *stream) {
  IVP_U_Matrix3 matrix3Out;
  matrix3->byte_swap();
  (void)matrix3->calc_eigen_vector(1.0, point);
  IVP_FLOAT alpha = 0.0f, beta = 0.0f, gamma = 0.0f;
  (void)matrix3->get_angles(&alpha, &beta, &gamma);
  matrix3->get_col(IVP_INDEX_X, point);
  matrix3->get_col(IVP_INDEX_X, floatPoint);
  (void)matrix3->get_determinante();
  matrix3->get_row(IVP_INDEX_X, point);
  matrix3->get_row(IVP_INDEX_X, floatPoint);
  matrix3->init_columns3(point, point, point);
  matrix3->init_rows3(point, point, point);
  matrix3->init_normized3_col(point, IVP_INDEX_X, point);
  matrix3->init_normized3_row(point, IVP_INDEX_X);
  matrix3->init_normized3_row(point, IVP_INDEX_X, point);
  matrix3->init_rotated3(IVP_INDEX_Y, 0.25f);
  matrix3->inline_mimult3(matrix3, &matrix3Out);
  matrix3->inline_mmult3(matrix3, &matrix3Out);
  (void)matrix3->normize();
  matrix3->orthogonize();
  (void)matrix3->orthonormize();
  (void)matrix3->quad_rot_distance_to(matrix3);
  (void)matrix3->real_invert();
  matrix3->set_col(IVP_INDEX_X, floatPoint);
  matrix3->set_row(IVP_INDEX_X, point);
  matrix3->set_row(IVP_INDEX_X, floatPoint);
  matrix3->vimult3(floatPoint, floatPoint);

  IVP_U_Matrix matrixOut;
  IVP_FLOAT columnMajor[16]{};
  matrix->byte_swap();
  matrix->get_4x4_column_major(columnMajor);
  matrix->init_columns4(point, point, point, point);
  matrix->init_rows4(point, point, point, point);
  matrix->init_rot_multiple(point, 0.5);
  matrix->inline_mimult4(matrix, &matrixOut);
  matrix->inline_mmult4(matrix, &matrixOut);
  matrix->inline_vimult4(floatPoint, floatPoint);
  matrix->inline_vimult4(point, floatPoint);
  matrix->inline_vmult4(floatPoint, floatPoint);
  matrix->interpolate(matrix, matrix, 0.5);
  matrix->print("matrix");
  (void)matrix->quad_distance_to(matrix);
  (void)matrix->read_from_file(stream);
  (void)matrix->real_invert(matrix);
  matrix->rotate(IVP_INDEX_Z, 0.25f, &matrixOut);
  matrix->rotate_invers(IVP_INDEX_Z, 0.25f, &matrixOut);
  matrix->set_matrix(matrix);
  matrix->shift_ws(point);
  matrix->transpose();
  (void)matrix->write_to_file(stream, "MATRIX_START");

  IVP_DOUBLE matrix44[4][4]{};
  IVP_U_Quat fromPoint(*point);
  IVP_U_Quat fromMatrix(matrix3);
  quaternion->byte_swap();
  (void)quaternion->acos_quat(&fromPoint);
  quaternion->get_angles(floatPoint);
  quaternion->init();
  (void)quaternion->inline_estimate_q_diff_to(floatQuaternion);
  quaternion->inline_set_mult_quat(&fromPoint, floatQuaternion);
  quaternion->inline_set_mult_quat(&fromPoint, &fromMatrix);
  quaternion->invert_quat();
  quaternion->normize_correct_step(3);
  quaternion->set(0.1, 0.2, 0.3);
  quaternion->set_div_unit_quat(&fromPoint, &fromMatrix);
  quaternion->set_fast_multiple(point, 0.5);
  quaternion->set_fast_multiple_with_clip(floatPoint, 0.5);
  quaternion->set_from_rotation_vectors(1.0, 0.0, 0.0, 0.0, 1.0, 0.0);
  quaternion->set_interpolate_linear(&fromPoint, &fromMatrix, 0.5);
  quaternion->set_matrix(matrix44);
  quaternion->set_quaternion(matrix44);
  quaternion->set_very_fast_multiple(floatPoint, 0.5);
}

struct FVectorElement {
  int indices[2] = {-1, -1};
  int get_fvector_index(int position) { return indices[position]; }
  void set_fvector_index(int oldIndex, int newIndex) {
    for (int &index : indices) {
      if (index == oldIndex) {
        index = newIndex;
        return;
      }
    }
  }
};

[[maybe_unused]] void CompileUtilitySurface(IVP_U_Matrix3 *matrix,
                                            IVP_U_Point *point) {
  IVP_Time time(1.0);
  time += 0.5;
  time -= IVP_Time(0.25);
  (void)(time - IVP_Time(0.0));
  (void)(time + 1.0);

  IVP_DOUBLE a = 0.0, b = 0.0, c = 0.0, d = 0.0;
  (void)IVP_Inline_Math::invert_2x2_matrix(1.0, 0.0, 0.0, 1.0, &a, &b, &c, &d);
  (void)IVP_Inline_Math::clamp(0.5f, 0.0f, 1.0f);
  (void)IVP_Inline_Math::isqrt_float(4.0f);
  (void)IVP_Inline_Math::isqrt_double(4.0);
  (void)IVP_Inline_Math::fast_anywhere_asin(0.25f);
  (void)IVP_Inline_Math::save_acosf(0.25f);

  FVectorElement element;
  IVP_U_FVector<FVectorElement> vector(4);
  vector.add(&element);
  (void)vector.index_of(&element);
  vector.swap_elems(0, 0);
  vector.remove_allow_resort(&element);

  IVP_U_Mapping mapping;
  mapping.vapply(point, point);
  mapping.viapply(point, point);
  mapping.mapply(matrix, matrix);
  mapping.miapply(matrix, matrix);
  mapping.mi2apply(matrix, matrix);
  (void)mapping.mapping_type();
  (void)mapping[IVP_INDEX_X];
}

[[maybe_unused]] void CompileSetSurface(IVP_Core *core) {
  IVP_U_Set<IVP_Core> set(8);
  set.add_element(core);
  set.install_element(core);
  (void)set.find_element(core);
  IVP_U_Set_Enumerator<IVP_Core> enumerator(&set);
  (void)enumerator.get_next_element(&set);
  set.remove_element(core);

  IVP_U_Vector<IVP_Core> vector;
  vector.add(core);
  IVP_U_Vector_Enumerator<IVP_Core> vectorEnumerator(&vector);
  (void)vectorEnumerator.get_next_element(&vector);

  IVP_U_BigVector<IVP_Core> bigVector;
  bigVector.add(core);
  IVP_U_BigVector_Enumerator<IVP_Core> bigVectorEnumerator(&bigVector);
  (void)bigVectorEnumerator.get_next_element(&bigVector);

  CoreSetListener listener;
  IVP_U_Set_Active<IVP_Core> activeSet(8);
  activeSet.add_listener_set_active(&listener);
  activeSet.add_element(core);
  activeSet.install_element(core);
  activeSet.remove_element(core);
  activeSet.remove_listener_set_active(&listener);

  IVP_U_Min_Hash minHash(16);
  minHash.add(core, 2.0);
  minHash.change_value(core, 1.0);
  (void)minHash.find_min_elem();
  (void)minHash.find_min_value();
  (void)minHash.is_elem(core);
  IVP_U_Min_Hash_Enumerator minEnumerator(&minHash);
  (void)minEnumerator.get_next_element();
  (void)minEnumerator.get_next_element_lt(3.0);
  minHash.remove(core);

  IVP_VHash_Store store(8);
  const unsigned int storeHash =
      static_cast<unsigned int>(IVP_VHash::hash_index(
          reinterpret_cast<const char *>(&core), sizeof(core)));
  store.add_elem(core, core);
  store.add_elem(core, core, storeHash);
  store.change_elem(core, core);
  (void)store.find_elem(core);
  (void)store.find_elem(core, storeHash);
  (void)store.touch_element(core, storeHash);
  store.untouch_all();
  store.check();
  store.print();
  (void)store.remove_elem(core);

  IVP_VHash_Store_Elem staticElements[8]{};
  IVP_VHash_Store staticStore(staticElements, 8);
  (void)staticStore;

  IVP_U_Min_List minList(8);
  const IVP_U_MINLIST_INDEX minListIndex = minList.add(core, 1.0f);
  (void)minList.has_elements();
  (void)minList.find_min_elem();
  (void)minList.find_min_value();
  minList.prefetch0_minlist();
  minList.prefetch1_minlist();
  minList.check();
  IVP_U_Min_List_Enumerator minListEnumerator(&minList);
  (void)minListEnumerator.get_next_element();
  (void)minListEnumerator.get_next_element_header();
  (void)minListEnumerator.get_next_element_lt(2.0f);
  minList.remove_minlist_elem(minListIndex);
}

[[maybe_unused]] void CompileRepresentativeSurface(IVP_Environment *environment,
                                                   IVP_Real_Object *object,
                                                   IVP_Core *core) {
  IVP_U_Point position;
  IVP_U_Float_Point impulse;
  IVP_U_Matrix matrix;
  IVP_U_Float_Hesse liquidPlane;

  position.set_to_zero();
  impulse.set_to_zero();
  matrix.set_identity();
  liquidPlane.calc_hesse(&impulse, &impulse, &impulse);

  IVP_Event_Sim defaultStep(environment);
  (void)defaultStep.delta_time;
  IVP_Time_Event_PSI psiTimeEvent;
  IVP_Vector_of_Objects objectVector;
  objectVector.reset();
  IVP_Vector_of_Cores_128 coreVector;
  IVP_Vector_of_Hulls_128 hullVector;
  (void)psiTimeEvent;
  (void)coreVector;
  (void)hullVector;

  object->get_m_world_f_object_AT(&matrix);
  if (IVP_Cluster *rootCluster = environment->get_root_cluster()) {
    IVP_Object *firstObject = rootCluster->get_first_object_of_cluster();
    (void)rootCluster->get_next_object_in_cluster(firstObject);
  }
  if (IVP_Cache_Object *cache = object->get_cache_object_no_lock()) {
    IVP_U_Float_Point objectPosition;
    cache->transform_position_to_object_coords(&position, &objectPosition);
    IVP_U_Matrix_Cache matrixCache(cache);
    (void)matrixCache.calc_matrix_at_now(
        environment->get_current_time(), 0);
    matrixCache.calc_calc_matrix_cache(cache);
  }
  object->async_push_object_ws(&position, &impulse);
  core->calc_at_matrix(environment->get_current_time(), &matrix);

  if (IVP_Synapse_Real *synapse = object->get_first_exact_synapse()) {
    (void)synapse->get_object();
    (void)synapse->get_status();
    (void)synapse->get_edge();
    (void)synapse->get_ledge();
    (void)synapse->get_synapse_mindist();
    (void)synapse->get_core();
    (void)synapse->get_next();
    (void)synapse->get_prev();
    (void)synapse->get_mindist();
  }
  if (IVP_Synapse_Friction *synapse = object->get_first_friction_synapse()) {
    (void)synapse->get_object();
    (void)synapse->get_next();
    (void)synapse->get_prev();
    (void)synapse->get_contact_point();
    (void)synapse->get_material_index();
    (void)synapse->get_status();
    (void)synapse->get_edge();
  }

  IVP_Template_Anchor anchorA;
  IVP_Template_Anchor anchorB;
  anchorA.set_anchor_position_ws(object, &position);
  anchorB.set_anchor_position_ws(object, &position);
  anchorA.set_anchor_position_os(object, &impulse);
  anchorA.set_anchor_position_cs(object, &impulse);
  auto nextAnchor =
      static_cast<IVP_Anchor *(IVP_Anchor::*)()>(&IVP_Anchor::get_next_anchor);
  auto previousAnchor =
      static_cast<IVP_Anchor *(IVP_Anchor::*)()>(&IVP_Anchor::get_prev_anchor);
  auto anchorObject = static_cast<IVP_Real_Object *(IVP_Anchor::*)()>(
      &IVP_Anchor::anchor_get_real_object);
  auto collisionMask = static_cast<int (IVP_Listener_Collision::*)()>(
      &IVP_Listener_Collision::get_enabled_callbacks);
  auto anchorObjectDeleted = &IVP_Anchor::object_is_going_to_be_deleted_event;
  (void)nextAnchor;
  (void)previousAnchor;
  (void)anchorObject;
  (void)anchorObjectDeleted;
  (void)collisionMask;
  IVP_Template_Spring springTemplate;
  springTemplate.anchors[0] = &anchorA;
  springTemplate.anchors[1] = &anchorB;
  IVP_Template_Two_Point twoPointTemplate;
  twoPointTemplate.anchors[0] = &anchorA;
  twoPointTemplate.anchors[1] = &anchorB;
  TwoPointActuator twoPointActuator(environment, &twoPointTemplate);
  (void)twoPointActuator.calc_len();
  IVP_Actuator_Spring *spring = environment->create_spring(&springTemplate);
  if (spring) {
    SpringListener springListener;
    spring->add_listener_spring(&springListener);
    spring->set_constant(20.0);
    (void)spring->get_constant();
    (void)spring->get_damp_factor();
    (void)spring->get_rel_pos_damp();
    (void)spring->get_spring_length_zero_force();
    (void)spring->calc_len();
    spring->remove_listener_spring(&springListener);
  }
  IVP_Template_Stiff_Spring_Active stiffSpringTemplate;
  stiffSpringTemplate.anchors[0] = &anchorA;
  stiffSpringTemplate.anchors[1] = &anchorB;
  auto *stiffSpring =
      new IVP_Controller_Stiff_Spring_Active(environment, &stiffSpringTemplate);
  StiffSpringListener stiffSpringListener;
  stiffSpring->add_listener_stiff_spring(&stiffSpringListener);
  stiffSpring->set_constant(0.2);
  stiffSpring->set_damp(0.1);
  stiffSpring->set_len(2.0);
  stiffSpring->set_break_max_len(5.0);
  (void)stiffSpring->get_constant();
  (void)stiffSpring->get_damp_factor();
  (void)stiffSpring->get_spring_length_zero_force();
  stiffSpring->remove_listener_stiff_spring(&stiffSpringListener);
  delete stiffSpring;
  IVP_Template_Check_Dist checkDistanceTemplate;
  checkDistanceTemplate.objects[0] = object;
  checkDistanceTemplate.objects[1] = object;
  checkDistanceTemplate.position_world_space[0] = position;
  checkDistanceTemplate.position_world_space[1] = position;
  IVP_Actuator_Check_Dist *checkDistance =
      environment->create_check_dist(&checkDistanceTemplate);
  if (checkDistance) {
    CheckDistListener checkDistanceListener;
    checkDistance->add_listener_check_dist_event(&checkDistanceListener);
    checkDistance->set_range(2.0);
    checkDistance->remove_listener_check_dist_event(&checkDistanceListener);
    delete checkDistance;
  }
  IVP_Template_Stabilizer stabilizerTemplate;
  stabilizerTemplate.anchors[0] = &anchorA;
  stabilizerTemplate.anchors[1] = &anchorB;
  stabilizerTemplate.anchors[2] = &anchorA;
  stabilizerTemplate.anchors[3] = &anchorB;
  if (IVP_Actuator_Stabilizer *stabilizer =
          environment->create_stabilizer(&stabilizerTemplate)) {
    stabilizer->set_stabi_constant(0.2);
    delete stabilizer;
  }
  IVP_Template_Suspension suspensionTemplate;
  suspensionTemplate.anchors[0] = &anchorA;
  suspensionTemplate.anchors[1] = &anchorB;
  if (IVP_Actuator_Suspension *suspension =
          environment->create_suspension(&suspensionTemplate)) {
    suspension->set_spring_damp_compression(0.4f);
    suspension->set_max_body_force(1000.0f);
    delete suspension;
  }

  IVP_Ray_Solver_Template rayTemplate;
  rayTemplate.ray_start_point = position;
  rayTemplate.ray_normized_direction.set(0.0f, -1.0f, 0.0f);
  rayTemplate.ray_length = 10.0f;
  IVP_Ray_Solver_Min ray(&rayTemplate);
  IVP_U_Float_Point boundsMinimum(-1.0f, -1.0f, -1.0f);
  IVP_U_Float_Point boundsMaximum(1.0f, 1.0f, 1.0f);
  (void)ray.check_ray_against_sphere(&boundsMinimum, 1.0f);
  (void)ray.check_ray_against_cube(&boundsMinimum, &boundsMaximum);
  (void)ray.check_ray_against_square(1.0f, 1.0f, &boundsMinimum, &boundsMaximum,
                                     0, 1);
  (void)ray.check_ray_against_compact_ledge_os(nullptr, object);
  IVP_Ray_Solver_Os objectSpaceRay(&ray, object);
  (void)objectSpaceRay.check_ray_against_compact_ledge_os(nullptr);
  ray.check_ray_against_all_objects_in_sim(environment);
  IVP_Ray_Solver *raySolvers[] = {&ray};
  IVP_Ray_Solver_Group rayGroup(1, raySolvers);
  (void)rayGroup.check_ray_group_against_cube(&boundsMinimum, 2.0f);
  rayGroup.check_ray_group_against_object(object);
  rayGroup.check_ray_group_against_all_objects_in_sim(environment);
  IVP_Ray_Solver_Min_Hash allRayHits(&rayTemplate);
  allRayHits.check_ray_against_object(object);
  (void)allRayHits.get_result_min_hash();
  if (object->get_surface_manager()) {
    object->get_surface_manager()->insert_all_ledges_hitting_ray(&ray, object);
    IVP_Vector_of_Ledges_256 nearbyLedges;
    object->get_surface_manager()->get_all_terminal_ledges(&nearbyLedges);
    if (object->get_surface_manager()->get_type() == IVP_SURMAN_POLYGON) {
      auto *polygonSurface = static_cast<IVP_SurfaceManager_Polygon *>(
          object->get_surface_manager());
      (void)polygonSurface->get_compact_surface();
    }
  }

  CollisionListener collisionListener;
  ObjectListener objectListener;
  Controller controller;
  OnlyDifferentObjects customFilter;
  IVP_Collision_Filter_Coll_Group_Ident groupFilter(IVP_FALSE);
  IVP_Collision_Filter_Exclusive_Pair pairFilter;
  IVP_Meta_Collision_Filter combinedFilter(IVP_FALSE);
  combinedFilter.add_collision_filter(&groupFilter);
  combinedFilter.add_collision_filter(&customFilter);
  pairFilter.disable_collision_between_objects(object, object);
  pairFilter.enable_collision_between_objects(object, object);
  (void)combinedFilter.check_objects_for_collision_detection(object, object);
  IVP_U_Active_Value_Manager *activeValues =
      environment->get_active_value_manager();
  if (activeValues) {
    (void)activeValues->get_active_float_by_name("current_time");
    (void)activeValues->get_active_int_by_name("1");
    IVP_U_Active_Terminal_Double *wind =
        activeValues->create_active_float("mod.wind", 0.0);
    if (wind)
      wind->set_double(1.0, IVP_TRUE);
  }
  IVP_U_Active_Terminal_Double localFloat("mod.local.float", 1.0);
  localFloat.set_double(2.0, IVP_FALSE);
  (void)localFloat.get_name();
  (void)localFloat.give_double_value();
  (void)localFloat.get_float_value();
  IVP_U_Active_Terminal_Int localInt("mod.local.int", 1);
  localInt.set_int(2, IVP_FALSE);
  (void)localInt.give_int_value();
  IVP_FLOAT interpolationValues[3] = {1.0f, 2.0f, 3.0f};
  IVP_MI_Vector *interpolationVector =
      IVP_MI_Vector::malloc_and_set_mi_vector(3, interpolationValues);
  if (interpolationVector) {
    interpolationVector->mult(0.5f);
    (void)interpolationVector->length();
    BML::IVP::ABI::Invoke<void>(BML::IVP::ABI::Address::Free,
                                interpolationVector);
  }
  IVP_Buoyancy_Input buoyancyInput;
  IVP_Buoyancy_Output buoyancyOutput;
  (void)buoyancyInput;
  (void)buoyancyOutput;
  object->add_listener_collision(&collisionListener);
  object->add_listener_object(&objectListener);
  object->remove_listener_object(&objectListener);
  object->unlink_contact_points_for_object(object);
  object->do_radar_checking(nullptr);
  (void)object->disable_simulation();
  environment->reset_time();
  object->reset_time(environment->get_current_time());
  object->recheck_collision_filter();
  object->force_grow_friction_system();
  environment->add_listener_object_global(&objectListener);
  environment->add_listener_object_private(object, &objectListener);
  environment->remove_listener_object_private(object, &objectListener);
  environment->add_listener_collision_private(object, &collisionListener);
  environment->remove_listener_collision_private(object, &collisionListener);
  environment->fire_object_is_removed_from_collision_detection(object);
  environment->force_psi_on_next_simulation();
  core->add_core_controller(&controller);

  IVP_U_String_Hash surfaceNames(64);
  surfaceNames.add("test", object->get_surface_manager());
  (void)surfaceNames.find("test");
  (void)surfaceNames.hash_index("test");

  IVP_Template_Constraint constraintTemplate;
  constraintTemplate.set_ballsocket_ws(object, &position, object);
  constraintTemplate.set_ballsocket_tense_Ros(object, &position, object,
                                              &position);
  constraintTemplate.set_cardanjoint_ws(object, &position, &position, object);
  constraintTemplate.set_cardanjoint_Ros(object, &position, &position, object);
  constraintTemplate.set_hinge_Ros(object, &position, &position, object, -0.5f,
                                   0.5f);
  constraintTemplate.set_fixing_point_Ros(&position);
  constraintTemplate.set_translation_axes_Ros(&matrix);
  constraintTemplate.set_rotation_axes_Ros(&matrix);
  constraintTemplate.set_attached_fixing_point_Aos(&position);
  constraintTemplate.set_attached_translation_axes_Aos(&matrix);
  constraintTemplate.set_max_translation_impulse(IVP_CFE_BREAK, 10.0f);
  constraintTemplate.set_max_translation_impulse(IVP_INDEX_X, IVP_CFE_CLIP,
                                                 5.0f);
  constraintTemplate.set_max_rotation_impulse(IVP_CFE_BREAK, 4.0f);
  constraintTemplate.set_max_rotation_impulse(IVP_INDEX_Z, IVP_CFE_CLIP, 2.0f);
  IVP_Constraint *constraint =
      environment->create_constraint(&constraintTemplate);
  if (constraint) {
    constraint->deactivate();
    constraint->activate();
    (void)constraint->get_associated_controlled_cores();
    IVP_Constraint_Local *local =
        static_cast<IVP_Constraint_Local *>(constraint);
    (void)local->get_objectR();
    (void)local->get_objectA();
    local->change_fixing_point_Ros(&position);
    local->change_target_fixing_point_Ros(&position);
    local->change_translation_axes_Ros(&matrix);
    local->change_target_translation_axes_Ros(&matrix);
    local->fix_translation_axis(IVP_INDEX_X);
    local->free_translation_axis(IVP_INDEX_X);
    local->limit_translation_axis(IVP_INDEX_X, -1.0f, 1.0f);
    local->change_max_translation_impulse(IVP_CFE_CLIP, 5.0f);
    local->change_rotation_axes_Ros(&matrix);
    local->change_target_rotation_axes_Ros(&matrix);
    local->fix_rotation_axis(IVP_INDEX_Z);
    local->free_rotation_axis(IVP_INDEX_Z);
    local->limit_rotation_axis(IVP_INDEX_Z, -0.5f, 0.5f);
    local->change_max_rotation_impulse(IVP_CFE_BREAK, 4.0f);
    local->change_Aos_to_relaxe_constraint();
    local->change_Ros_to_relaxe_constraint();
  }
  (void)IVP_Constraint::create_constraint_any_solver(&constraintTemplate);
  (void)constraint;

  IVP_Template_Controller_Motion motionTemplate;
  IVP_Controller_Motion *motion =
      environment->create_controller_motion(object, &motionTemplate);
  if (motion) {
    IVP_U_Quat targetOrientation;
    targetOrientation.init();
    motion->set_target_position_ws(&position);
    motion->set_target_object_position_ws(object, &targetOrientation,
                                          &position);
    motion->set_target_q_world_f_core(&targetOrientation);
    motion->set_max_translation_force(&impulse);
    motion->set_max_torque(&impulse);
    motion->set_force_factor(0.8f);
    motion->set_damp_factor(1.0f);
    motion->set_torque_factor(0.8f);
    motion->set_angular_damp_factor(1.0f);
    (void)motion->get_target_position_ws();
    (void)motion->get_target_orientation();
  }

  IVP_U_Vector<IVP_U_Point> points;
  points.add(&position);
  IVP_Compact_Ledge *ledge =
      IVP_SurfaceBuilder_Pointsoup::convert_pointsoup_to_compact_ledge(&points);
  IVP_SurfaceBuilder_Ledge_Soup ledgeSoup;
  ledgeSoup.insert_ledge(ledge);
  (void)ledgeSoup.compile();

  IVP_Template_Buoyancy buoyancyTemplate;
  IVP_U_Float_Point current;
  IVP_Liquid_Surface_Descriptor_Simple liquid(&liquidPlane, &current);
  if (object->get_controller_phantom()) {
    IVP_Controller_Phantom *phantom = object->get_controller_phantom();
    PhantomListener phantomListener;
    phantom->add_listener_phantom(&phantomListener);
    phantom->remove_listener_phantom(&phantomListener);
    phantom->wake_all_sleeping_objects();
    (void)phantom->get_object();
    (void)phantom->get_intruding_objects();
    (void)phantom->get_intruding_cores();
    (void)phantom->get_intruding_mindists();
    (void)IVP_Attacher_To_Cores_Buoyancy::create(
        buoyancyTemplate, phantom->get_set_of_cores(), &liquid);
  }
  static_assert(std::is_constructible_v<
                IVP_Attacher_To_Cores_Buoyancy, IVP_Template_Buoyancy &,
                IVP_U_Set_Active<IVP_Core> *, IVP_Liquid_Surface_Descriptor *>);

  IVP_Material_Simple material(0.5, 0.25);
  CustomMaterial customMaterial;
  IVP_Material *materialInterface = &customMaterial;
  static_assert(
      std::is_same_v<decltype(&IVP_Material_Simple::get_friction_factor),
                     IVP_DOUBLE (IVP_Material_Simple::*)()>);
  static_assert(
      std::is_same_v<decltype(&IVP_Material_Simple::get_second_friction_factor),
                     IVP_DOUBLE (IVP_Material_Simple::*)()>);
  static_assert(std::is_same_v<decltype(&IVP_Material_Simple::get_elasticity),
                               IVP_DOUBLE (IVP_Material_Simple::*)()>);
  static_assert(std::is_same_v<decltype(&IVP_Material_Simple::get_adhesion),
                               IVP_DOUBLE (IVP_Material_Simple::*)()>);
  static_assert(std::is_same_v<decltype(&IVP_Material_Simple::get_name),
                               const char *(IVP_Material_Simple::*)()>);
  (void)material.get_friction_factor();
  (void)material.get_second_friction_factor();
  (void)material.get_elasticity();
  (void)material.get_adhesion();
  (void)material.get_name();
  (void)materialInterface->get_friction_factor();
  (void)materialInterface->get_second_friction_factor();
  (void)materialInterface->get_elasticity();
  (void)materialInterface->get_adhesion();
  (void)materialInterface->get_name();

  IVP_Standard_Gravity_Controller gravityController;
  gravityController.set_standard_gravity(&position);
  (void)gravityController.get_controller_priority();
  (void)gravityController.grav_vec;
  IVP_Material_Manager materialManager(IVP_FALSE);
  IVP_Cache_Object_Manager cacheManager(8);
  CompileMatrixSeparationSolver predictionSolver;
  predictionSolver.set_max_deviation(1.0);
  predictionSolver.type = IVP_3D_SOLVER_TYPE_MAX_DEV;
  predictionSolver.max_deviation2 = 0.0;
  IVP_Cache_Object_Manager::invalid_cache_object(object);
  (void)cacheManager.cache_object_at(0);
  IVP_Anomaly_Limits anomalyLimits(IVP_FALSE);
  IVP_Anomaly_Manager anomalyManager(IVP_FALSE);
  static_assert(
      std::is_same_v<decltype(&IVP_Anomaly_Manager::inter_penetration),
                     void (IVP_Anomaly_Manager::*)(
                         IVP_Mindist *, IVP_Real_Object *, IVP_Real_Object *)>);
  (void)anomalyLimits.get_max_velocity();
  (void)anomalyLimits.get_max_angular_velocity_per_psi();
  (void)anomalyLimits.get_max_collisions_per_psi();
  anomalyManager.max_velocity_exceeded(&anomalyLimits, core, &impulse);
  anomalyManager.max_angular_velocity_exceeded(&anomalyLimits, core, &impulse);
  (void)anomalyManager.max_collisions_exceeded_check_freezing(&anomalyLimits,
                                                              core);
  (void)anomalyManager.get_push_speed_penetration(object, object);
  IVP_Freeze_Manager freezeManager;
  freezeManager.init_freeze_manager();
  IVP_Statistic_Manager statisticManager;
  statisticManager.clear_statistic();
  statisticManager.output_statistic();
  IVP_PerformanceCounter_Simple performanceCounter;
  IVP_U_Active_Value_Manager standaloneValues(IVP_FALSE);
  (void)material;
  (void)materialManager;
  (void)performanceCounter;
  (void)standaloneValues;
}

[[maybe_unused]] void CompileEnvironmentSurface(IVP_Environment *environment,
                                                IVP_Real_Object *object,
                                                IVP_Constraint *) {
  PsiListener psiListener;
  ObjectListener objectListener;
  CollisionListener collisionListener;
  environment->add_listener_PSI(&psiListener);
  environment->remove_listener_PSI(&psiListener);
  environment->install_listener_object_global(&objectListener);
  environment->remove_listener_object_global(&objectListener);
  environment->add_listener_collision_global(&collisionListener);
  environment->remove_listener_collision_global(&collisionListener);

  IVP_Event_Object event{environment, object};
  environment->fire_event_object_created(&event);
  environment->fire_event_object_deleted(&event);
  environment->fire_event_object_revived(&event);
  environment->fire_event_object_frozen(&event);

  environment->simulate_until(IVP_Time(1.0));
  environment->simulate_variable_time_step(0.01f);
  environment->simulate_time_step(0.5f);
  IVP_Environment::set_global_collision_tolerance(0.01, 0.01);
  (void)IVP_Environment::get_global_collision_tolerance();
}

} // namespace
