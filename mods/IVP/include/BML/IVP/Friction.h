#ifndef BML_IVP_FRICTION_H
#define BML_IVP_FRICTION_H

#include "BML/IVP/Controller.h"

#include <cstddef>
#include <cstdint>
#include <new>

class IVP_Contact_Point;
class IVP_Cache_Ledge_Point;
class IVP_Friction_Manager;
class IVP_Friction_System;
class IVP_Friction_Core_Pair;
class IVP_Friction_Info_For_Core;

// Ballance keeps this temporary solver record at 0xE0 bytes. Its base and
// span/core offsets are confirmed by the retained long-term impact and
// friction-constraint bodies, rather than accepted from the imported IDA type
// alone.
class alignas(8) IVP_Impact_Solver_Long_Term : public IVP_Contact_Situation {
public:
  std::int16_t index_in_fs;
  std::int16_t impacts_while_system;
  IVP_BOOL coll_time_is_valid : 8;
  IVP_BOOL friction_is_broken : 2;
  IVP_BOOL reserved_flags : 22;

  union {
    struct {
      IVP_FLOAT rescue_speed_addon;
      IVP_FLOAT distance_reached_in_time;
      IVP_FLOAT percent_energy_conservation;
      std::uint32_t reserved;
    } impact;
    struct {
      IVP_Friction_Info_For_Core *friction_infos[2];
      int has_negative_pull_since;
      IVP_FLOAT dist_len;
    } friction;
  };

  IVP_FLOAT virtual_mass;
  IVP_FLOAT inv_virtual_mass;
  IVP_Core *contact_core[2];
  IVP_U_Float_Point span_friction_v[2];
  IVP_U_Float_Point contact_point_cs[2];
  IVP_U_Float_Point contact_cross_nomal_cs[2];

  IVP_Impact_Solver_Long_Term() { init_tmp_contact_info(); }

  void init_tmp_contact_info() {
    impacts_while_system = 0;
    coll_time_is_valid = IVP_FALSE;
    friction_is_broken = IVP_FALSE;
  }
  IVP_DOUBLE get_closing_speed() const {
    IVP_DOUBLE result = 0.0;
    if (contact_core[0]) {
      result =
          contact_core[0]->speed.dot_product(&surf_normal) +
          contact_core[0]->rot_speed.dot_product(&contact_cross_nomal_cs[0]);
    }
    if (contact_core[1]) {
      result -=
          contact_core[1]->speed.dot_product(&surf_normal) +
          contact_core[1]->rot_speed.dot_product(&contact_cross_nomal_cs[1]);
    }
    return result;
  }
  void do_impact_long_term(IVP_Core *pushedCores[2], IVP_FLOAT rescueSpeed,
                           IVP_Contact_Point *contactPoint) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::ImpactSolverLongTermDoImpact, this, pushedCores,
        rescueSpeed, contactPoint);
  }
};

enum IVP_CONTACT_POINT_BREAK_STATUS : std::int32_t {
  IVP_CPBS_NORMAL,
  IVP_CPBS_NEEDS_RECHECK,
  IVP_CPBS_BROKEN,
};

// Retail allocates 0x78 bytes for a contact point. The nearby configuration's
// imported 0x88-byte type is not used: Ballance stores the friction-system
// backlink at +0x70 and has no room for that revision's trailing point field.
class alignas(8) IVP_Contact_Point {
  friend class IVP_Friction_Manager;
  friend class IVP_Friction_Core_Pair;
  friend class IVP_Friction_Solver;
  friend class IVP_Friction_System;
  friend class IVP_Mindist;

  IVP_Contact_Point *next_dist_in_friction;
  IVP_Contact_Point *prev_dist_in_friction;
  IVP_Synapse_Friction synapse[2];
  IVP_FLOAT inv_virt_mass_mindist_no_dir;
  IVP_BOOL two_friction_values : 8;
  IVP_FLOAT span_friction_s[2];
  IVP_Impact_Solver_Long_Term *tmp_contact_info;
  IVP_FLOAT real_friction_factor;
  IVP_FLOAT integrated_destroyed_energy;
  IVP_FLOAT inv_triangle_det;
  IVP_FLOAT old_energy_dynamic_fr;
  IVP_FLOAT now_friction_pressure;
  IVP_FLOAT last_gap_len;
  std::int16_t slowly_turn_on_keeper;
  IVP_CONTACT_POINT_BREAK_STATUS cp_status : 8;
  int has_negative_pull_since;
  IVP_Time last_time_of_recalc_friction_s_vals;
  IVP_Friction_System *l_friction_system;
  std::uint32_t tail_padding_;

public:
  IVP_Synapse_Friction *get_synapse(int index) {
    return &synapse[index];
  }
  const IVP_Synapse_Friction *get_synapse(int index) const {
    return &synapse[index];
  }
  IVP_Contact_Point *get_next_friction_dist() const {
    return next_dist_in_friction;
  }
  IVP_Contact_Point *get_prev_friction_dist() const {
    return prev_dist_in_friction;
  }
  IVP_Friction_System *get_friction_system() const {
    return l_friction_system;
  }
  IVP_FLOAT get_friction_factor() const {
    return span_friction_s[0] * span_friction_s[1];
  }
  IVP_Impact_Solver_Long_Term *get_lt() { return tmp_contact_info; }
  const IVP_Impact_Solver_Long_Term *get_lt() const {
    return tmp_contact_info;
  }
  void set_friction_to_neutral() {
    span_friction_s[0] = 0.0f;
    span_friction_s[1] = 0.0f;
  }
  void get_material_info(IVP_Material *materials[2]) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::ContactPointGetMaterialInfo, this, materials);
  }
  void recalc_friction_s_vals() {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::ContactPointRecalculateFriction, this);
  }
  void recompute_friction() {
    IVP_Real_Object *object = get_synapse(0)->get_object();
    IVP_Environment *environment = object ? object->get_environment() : nullptr;
    if (!environment)
      return;
    IVP_U_Memory *memory = environment->get_sim_unit_mem();
    BML::IVP::Detail::StartSimulationMemoryTransaction(memory);
    recalc_friction_s_vals();
    BML::IVP::Detail::EndSimulationMemoryTransaction(memory);
  }

private:
  explicit IVP_Contact_Point(IVP_Mindist *mindist) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::ContactPointConstruct, this, mindist);
  }
  ~IVP_Contact_Point() {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::ContactPointDestruct, this);
  }

  IVP_DOUBLE two_values_friction(IVP_U_Float_Point *worldFriction) {
    return BML::IVP::ABI::InvokeThis<IVP_DOUBLE>(
        BML::IVP::ABI::Address::ContactPointTwoValuesFriction, this,
        worldFriction);
  }
  IVP_DOUBLE
  get_and_set_real_friction_len(IVP_U_Float_Point *worldFriction) {
    return BML::IVP::ABI::InvokeThis<IVP_DOUBLE>(
        BML::IVP::ABI::Address::ContactPointGetSetRealFrictionLength, this,
        worldFriction);
  }
  void static_friction_single(const IVP_Event_Sim *event,
                              IVP_FLOAT desiredGap,
                              IVP_FLOAT speedupFactor) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::ContactPointStaticFrictionSingle, this, event,
        desiredGap, speedupFactor);
  }
  void ease_the_friction_force(IVP_U_Float_Point *difference) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::ContactPointEaseFrictionForce, this,
        difference);
  }
  IVP_FLOAT get_rot_speed_uncertainty() {
    return BML::IVP::ABI::InvokeThis<IVP_FLOAT>(
        BML::IVP::ABI::Address::ContactPointGetRotationUncertainty, this);
  }
  IVP_FLOAT get_rescue_speed_impact(IVP_Environment *environment) {
    return BML::IVP::ABI::InvokeThis<IVP_FLOAT>(
        BML::IVP::ABI::Address::ContactPointGetRescueSpeed, this, environment);
  }
  void calc_coll_distance() {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::ContactPointCalculateCollisionDistance, this);
  }
  void reset_time(IVP_Time offset) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::ContactPointResetTime, this, offset);
  }
  IVP_BOOL is_same_as(const IVP_Mindist *mindist) const {
    return BML::IVP::ABI::InvokeThis<IVP_BOOL>(
        BML::IVP::ABI::Address::ContactPointIsSameMindist,
        const_cast<IVP_Contact_Point *>(this), mindist);
  }
  void calc_virtual_mass_of_mindist() {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::ContactPointCalculateVirtualMass, this);
  }
  void read_materials_for_contact_situation(
      IVP_Impact_Solver_Long_Term *information) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::ContactPointReadMaterials, this, information);
  }
  void p_calc_friction_s_PP(const IVP_U_Point *first,
                            const IVP_U_Point *second,
                            IVP_Impact_Solver_Long_Term *information,
                            IVP_U_Float_Point *contactDifference) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::ContactPointCalculateFrictionPointPoint, this,
        first, second, information, contactDifference);
  }
  void p_calc_friction_qr_PF(const IVP_U_Point *point,
                             const IVP_Compact_Edge *face,
                             IVP_Cache_Ledge_Point *faceCache,
                             IVP_Impact_Solver_Long_Term *information,
                             IVP_U_Float_Point *contactDifference) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::ContactPointCalculateFrictionPointFace, this,
        point, face, faceCache, information, contactDifference);
  }
  void p_calc_friction_s_PK(const IVP_U_Point *point,
                            const IVP_Compact_Edge *edge,
                            IVP_Cache_Ledge_Point *edgeCache,
                            IVP_Impact_Solver_Long_Term *information,
                            IVP_U_Float_Point *contactDifference) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::ContactPointCalculateFrictionPointEdge, this,
        point, edge, edgeCache, information, contactDifference);
  }
  void p_calc_friction_ss_KK(const IVP_Compact_Edge *firstEdge,
                             const IVP_Compact_Edge *secondEdge,
                             IVP_Cache_Ledge_Point *firstCache,
                             IVP_Cache_Ledge_Point *secondCache,
                             IVP_Impact_Solver_Long_Term *information,
                             IVP_U_Float_Point *contactDifference) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::ContactPointCalculateFrictionEdgeEdge, this,
        firstEdge, secondEdge, firstCache, secondCache, information,
        contactDifference);
  }

  IVP_FLOAT get_possible_friction_slide_way() const {
    return now_friction_pressure * real_friction_factor *
           inv_virt_mass_mindist_no_dir;
  }
  void calc_pretension(IVP_FLOAT maximumLength) {
    const IVP_DOUBLE first = span_friction_s[0];
    const IVP_DOUBLE second = span_friction_s[1];
    const IVP_DOUBLE squaredLength = first * first + second * second;
    if (squaredLength <=
        static_cast<IVP_DOUBLE>(maximumLength) * maximumLength + 1.0e-6)
      return;

    const IVP_DOUBLE inverseLength =
        IVP_Inline_Math::isqrt_float(static_cast<IVP_FLOAT>(squaredLength));
    integrated_destroyed_energy += static_cast<IVP_FLOAT>(
        (squaredLength * inverseLength - maximumLength) *
        real_friction_factor * now_friction_pressure);
    const IVP_DOUBLE retained = maximumLength * inverseLength;
    span_friction_s[0] = static_cast<IVP_FLOAT>(first * retained);
    span_friction_s[1] = static_cast<IVP_FLOAT>(second * retained);
    cp_status = IVP_CPBS_NEEDS_RECHECK;
  }
  IVP_FLOAT friction_force_local_constraint_2d(const IVP_Event_Sim *event) {
    // The retail body also contains the inlined Ballance car-wheel
    // specialization; there is no separate helper RVA to wrap safely.
    return BML::IVP::ABI::InvokeThis<IVP_FLOAT>(
        BML::IVP::ABI::Address::ContactPointFrictionConstraint2D, this, event);
  }
  void friction_force_local_constraint_1d(const IVP_Event_Sim *event) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::ContactPointFrictionConstraint1D, this, event);
  }
  IVP_U_Float_Point *friction_span(int index) {
    return &tmp_contact_info->span_friction_v[index];
  }
  const IVP_U_Float_Point *friction_span(int index) const {
    return &tmp_contact_info->span_friction_v[index];
  }
};

class IVP_Friction_Info_For_Core {
public:
  IVP_U_Vector<IVP_Contact_Point> friction_springs;
  IVP_Friction_System *l_friction_system;

  void set_all_dists_of_obj_neutral() {
    for (int index = friction_springs.len() - 1; index >= 0; --index)
      friction_springs.element_at(index)->set_friction_to_neutral();
  }
  void friction_info_insert_friction_dist(IVP_Contact_Point *contactPoint) {
    friction_springs.add(contactPoint);
  }
  void friction_info_delete_friction_dist(IVP_Contact_Point *contactPoint) {
    friction_springs.remove(contactPoint);
  }
  int dist_number() const { return friction_springs.len(); }
};

class alignas(8) IVP_Friction_Core_Pair {
  friend struct BML_IvpFrictionCorePairLayoutCheck;

private:
  union {
    IVP_U_Vector<IVP_Contact_Point> fr_dists;
  };
  // Debug-only scratch vector at retail offset +0x08. RVA 0x1D340 leaves it
  // untouched, matching IVP_U_Float_Point's intentionally trivial default
  // construction rather than requiring an opaque byte surrogate.
  IVP_U_Float_Point span_vector_sum;

public:
  IVP_Friction_Core_Pair() {
    BML::IVP::ABI::InvokeThisOr<void>(
        BML::IVP::ABI::Address::FrictionCorePairConstruct, this,
        [this] {
          ::new (static_cast<void *>(&fr_dists))
              IVP_U_Vector<IVP_Contact_Point>();
          next_ease_nr_psi = 1;
          last_impact_time_pair = IVP_Time(-1000.0);
          integrated_anti_energy = 0.0f;
        });
  }
  ~IVP_Friction_Core_Pair() {
    fr_dists.~IVP_U_Vector<IVP_Contact_Point>();
  }

  IVP_U_Vector<IVP_Contact_Point> *get_contact_points() { return &fr_dists; }
  const IVP_U_Vector<IVP_Contact_Point> *get_contact_points() const {
    return &fr_dists;
  }
  int number_of_pair_dists() {
    return BML::IVP::ABI::InvokeThis<int>(
        BML::IVP::ABI::Address::FrictionCorePairDistanceCount, this);
  }
  void add_fr_dist_obj_pairs(IVP_Contact_Point *contactPoint) {
    fr_dists.add(contactPoint);
  }
  void del_fr_dist_obj_pairs(IVP_Contact_Point *contactPoint) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionCorePairRemoveDistance, this,
        contactPoint);
  }
  int check_all_fr_mindists_to_be_valid(IVP_Friction_System *system) {
    return BML::IVP::ABI::InvokeThis<int>(
        BML::IVP::ABI::Address::FrictionCorePairCheckMindists, this, system);
  }
  void remove_energy_gained_by_real_friction() {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionCorePairRemoveGainedEnergy, this);
  }
  IVP_FLOAT get_sum_slide_way(const IVP_Event_Sim *event) {
    IVP_FLOAT sum = 0.0f;
    for (int index = fr_dists.len() - 1; index >= 0; --index)
      sum += fr_dists.element_at(index)->get_possible_friction_slide_way();
    return static_cast<IVP_FLOAT>(sum * event->delta_time * event->delta_time);
  }
  void pair_calc_friction_forces(const IVP_Event_Sim *event) {
    const IVP_FLOAT maximumLength = get_sum_slide_way(event);
    IVP_FLOAT gainedEnergy = 0.0f;
    for (int index = fr_dists.len() - 1; index >= 0; --index) {
      IVP_Contact_Point *contact = fr_dists.element_at(index);
      contact->calc_pretension(maximumLength);
      if (contact->two_friction_values != IVP_TRUE)
        gainedEnergy += contact->friction_force_local_constraint_2d(event);
      else
        contact->friction_force_local_constraint_1d(event);
    }
    if (gainedEnergy > 0.0f)
      integrated_anti_energy += gainedEnergy;
  }
  void set_friction_vectors(IVP_U_Float_Point *averageFriction) {
    for (int index = fr_dists.len() - 1; index >= 0; --index) {
      IVP_Contact_Point *contact = fr_dists.element_at(index);
      IVP_Real_Object *object = contact->get_synapse(0)->l_obj;
      const IVP_DOUBLE sign =
          object && object->get_core() == objs[0] ? 1.0 : -1.0;
      contact->span_friction_s[0] = static_cast<IVP_FLOAT>(
          averageFriction->dot_product(contact->friction_span(0)) * sign);
      contact->span_friction_s[1] = static_cast<IVP_FLOAT>(
          averageFriction->dot_product(contact->friction_span(1)) * sign);
    }
  }
  void get_average_friction_vector(IVP_U_Float_Point *averageFriction) {
    averageFriction->set_to_zero();
    const int count = fr_dists.len();
    if (count <= 0)
      return;
    for (int index = count - 1; index >= 0; --index) {
      IVP_Contact_Point *contact = fr_dists.element_at(index);
      IVP_U_Float_Point contactVector;
      contactVector.set_to_zero();
      contactVector.add_multiple(contact->friction_span(0),
                                 contact->span_friction_s[0]);
      contactVector.add_multiple(contact->friction_span(1),
                                 contact->span_friction_s[1]);
      IVP_Real_Object *object = contact->get_synapse(0)->l_obj;
      const IVP_DOUBLE sign =
          object && object->get_core() == objs[0] ? 1.0 : -1.0;
      averageFriction->add_multiple(&contactVector, sign);
    }
    averageFriction->mult(1.0 / count);
  }

  int next_ease_nr_psi;
  IVP_Time last_impact_time_pair;
  IVP_FLOAT integrated_anti_energy;
  IVP_Core *objs[2];
};

struct BML_IvpFrictionCorePairLayoutCheck {
  static constexpr std::size_t contact_points =
      offsetof(IVP_Friction_Core_Pair, fr_dists);
  static constexpr std::size_t span_vector =
      offsetof(IVP_Friction_Core_Pair, span_vector_sum);
};

// Two-body energy record used by Ballance's friction correction path. The
// complete 0xA0 layout and all public retained bodies are confirmed by
// IVP_Friction_Core_Pair::destroy_mutual_energy at RVA 0x1D2A0. In
// particular, Ballance's IVP_DOUBLE fields are ordinary MSVC 64-bit doubles;
// the legacy IDA rendering of them as long double is not used here.
class alignas(8) IVP_Mutual_Energizer {
  IVP_U_Float_Point trans_vec_world;
  IVP_U_Float_Point rot_vec_obj[2];
  IVP_DOUBLE trans_speed_potential;
  IVP_DOUBLE trans_inertia[2];
  IVP_DOUBLE inv_trans_inertia[2];
  IVP_DOUBLE rot_speed_potential;
  IVP_DOUBLE rot_inertia[2];
  IVP_DOUBLE inv_rot_inertia[2];
  IVP_DOUBLE rot_energy_potential;
  IVP_DOUBLE trans_energy_potential;

public:
  IVP_Core *core[2];
  IVP_DOUBLE whole_mutual_energy;

  static IVP_DOUBLE calc_energy_potential(IVP_DOUBLE speedPotential,
                                          IVP_DOUBLE firstMass,
                                          IVP_DOUBLE secondMass,
                                          IVP_DOUBLE firstInverseMass,
                                          IVP_DOUBLE secondInverseMass) {
    return BML::IVP::ABI::Invoke<IVP_DOUBLE>(
        BML::IVP::ABI::Address::MutualEnergizerCalculatePotential,
        speedPotential, firstMass, secondMass, firstInverseMass,
        secondInverseMass);
  }
  void init_mutual_energizer(IVP_Core *firstCore, IVP_Core *secondCore) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::MutualEnergizerInitialize, this, firstCore,
        secondCore);
  }
  void calc_energy_potential() {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::MutualEnergizerCalculate, this);
  }
  void destroy_percent_energy(IVP_DOUBLE percentEnergyToDestroy) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::MutualEnergizerDestroyPercent, this,
        percentEnergyToDestroy);
  }
};

// These two controller objects are embedded inside IVP_Friction_System by the
// retail constructor. They are exposed as borrowed subobjects; constructing a
// second owner around them or deleting them independently would violate the
// friction system's lifetime contract.
class IVP_Friction_Sys_Energy : public IVP_Controller_Independent {
public:
  IVP_Friction_System *l_friction_system;

  void core_is_going_to_be_deleted_event(IVP_Core *) override {}
  void do_simulation_controller(
      IVP_Event_Sim *event, IVP_U_Vector<IVP_Core> *coreList) override {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionEnergyControllerDoSimulation, this,
        event, coreList);
  }
  IVP_CONTROLLER_PRIORITY get_controller_priority() override {
    return IVP_CP_ENERGY_FRICTION;
  }

  // The neighboring Ballance-era header exposes this misspelling as a
  // separate non-virtual convenience method. Slot 1 remains the inherited,
  // correctly spelled IVP_Controller method in the retail seven-slot table.
  IVP_DOUBLE get_mimumum_simulation_frequency() { return 1.0; }
  ~IVP_Friction_Sys_Energy() override = default;
};

class IVP_Friction_Sys_Static : public IVP_Controller_Independent {
  void do_simulation_single_friction(IVP_Event_Sim *event) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionStaticControllerDoSingle, this, event);
  }

public:
  IVP_Friction_System *l_friction_system;

  void core_is_going_to_be_deleted_event(IVP_Core *core) override {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionStaticControllerCoreDeleted, this,
        core);
  }
  IVP_DOUBLE get_minimum_simulation_frequency() override { return 1.0; }
  void do_simulation_controller(
      IVP_Event_Sim *event, IVP_U_Vector<IVP_Core> *coreList) override {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionStaticControllerDoSimulation, this,
        event, coreList);
  }
  IVP_CONTROLLER_PRIORITY get_controller_priority() override {
    return IVP_CP_STATIC_FRICTION;
  }
  ~IVP_Friction_Sys_Static() override = default;
};

class IVP_Friction_Manager {
public:
  static IVP_Contact_Point *generate_contact_point(IVP_Mindist *mindist,
                                                   IVP_BOOL *wasSuccessful) {
    return BML::IVP::ABI::Invoke<IVP_Contact_Point *>(
        BML::IVP::ABI::Address::FrictionManagerGenerateContact, mindist,
        wasSuccessful);
  }
  static IVP_Contact_Point *get_associated_contact_point(IVP_Mindist *mindist) {
    return BML::IVP::ABI::Invoke<IVP_Contact_Point *>(
        BML::IVP::ABI::Address::FrictionManagerGetAssociatedContact, mindist);
  }
};

// A friction system is owned by the retail contact manager. Mod code may
// inspect it and invoke the retained topology operations, but must not create
// one with a foreign allocator or destroy one while contact points remain.
//
// Ballance allocates exactly 0x50 bytes. Non-trivial embedded objects live in
// unions so the retained complete constructor/destructor exclusively own their
// lifetimes; the member names and visibility remain source-compatible.
class alignas(8) IVP_Friction_System : public IVP_Controller_Dependent {
  friend class IVP_Friction_Core_Pair;
  friend class IVP_Friction_Solver;
  friend class IVP_Contact_Point;
  friend class IVP_Core;
  friend class IVP_Mindist;
  friend class IVP_Friction_Sys_Static;
  friend class IVP_Friction_Sys_Energy;
  friend class IVP_Real_Object;

  IVP_Environment *l_environment;
  union {
    IVP_Friction_Sys_Static static_fs_handle;
  };
  union {
    IVP_Friction_Sys_Energy energy_fs_handle;
  };
  IVP_Friction_System *next_friction_system;
  IVP_Friction_System *prev_friction_system;
  IVP_Contact_Point *first_friction_dist;

public:
  union {
    IVP_U_Vector<IVP_Core> cores_of_friction_system;
  };
  union {
    IVP_U_Vector<IVP_Core> moveable_cores_of_friction_system;
  };
  union {
    IVP_U_Vector<IVP_Friction_Core_Pair> fr_pairs_of_objs;
  };

private:
  std::int16_t friction_obj_number;
  std::int16_t friction_dist_number;
  std::int16_t complex_not_necessary_number;
  std::uint16_t counter_padding_;

public:
  IVP_BOOL union_find_necessary : 8;
  IVP_BOOL fr_sys_simulated : 8;
  IVP_DOUBLE sum_energy_destroyed;

  explicit IVP_Friction_System(IVP_Environment *environment) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSystemConstruct, this, environment);
  }
  ~IVP_Friction_System() override {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSystemDestruct, this);
  }

  IVP_Environment *get_environment() const { return l_environment; }

  IVP_Friction_Sys_Static *get_static_friction_controller() {
    return &static_fs_handle;
  }
  const IVP_Friction_Sys_Static *get_static_friction_controller() const {
    return &static_fs_handle;
  }
  IVP_Friction_Sys_Energy *get_energy_friction_controller() {
    return &energy_fs_handle;
  }
  const IVP_Friction_Sys_Energy *get_energy_friction_controller() const {
    return &energy_fs_handle;
  }

  IVP_U_Vector<IVP_Core> *get_cores() { return &cores_of_friction_system; }
  const IVP_U_Vector<IVP_Core> *get_cores() const {
    return &cores_of_friction_system;
  }
  IVP_U_Vector<IVP_Core> *get_moveable_cores() {
    return &moveable_cores_of_friction_system;
  }
  const IVP_U_Vector<IVP_Core> *get_moveable_cores() const {
    return &moveable_cores_of_friction_system;
  }
  IVP_U_Vector<IVP_Friction_Core_Pair> *get_friction_pairs() {
    return &fr_pairs_of_objs;
  }
  const IVP_U_Vector<IVP_Friction_Core_Pair> *get_friction_pairs() const {
    return &fr_pairs_of_objs;
  }

  int get_friction_object_count() const { return friction_obj_number; }
  int get_fr_dist_number() const { return friction_dist_number; }
  int get_complex_not_necessary_count() const {
    return complex_not_necessary_number;
  }
  IVP_Contact_Point *get_first_friction_dist() const {
    return first_friction_dist;
  }
  IVP_Contact_Point *
  get_next_friction_dist(const IVP_Contact_Point *contactPoint) const {
    return contact_link(contactPoint, 0x00u);
  }
  IVP_Contact_Point *
  get_prev_friction_dist(const IVP_Contact_Point *contactPoint) const {
    return contact_link(contactPoint, 0x04u);
  }

  IVP_BOOL is_union_find_necessary() const {
    return union_find_necessary;
  }
  void set_union_find_necessary(IVP_BOOL necessary) {
    union_find_necessary = necessary;
  }
  IVP_BOOL was_simulated() const { return fr_sys_simulated; }
  IVP_DOUBLE get_sum_energy_destroyed() const { return sum_energy_destroyed; }

  void add_core_to_system(IVP_Core *core) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSystemAddCore, this, core);
  }
  void remove_core_from_system(IVP_Core *core) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSystemRemoveCore, this, core);
  }
  void add_fr_pair(IVP_Friction_Core_Pair *pair) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSystemAddPair, this, pair);
  }
  void del_fr_pair(IVP_Friction_Core_Pair *pair) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSystemRemovePair, this, pair);
  }
  IVP_Friction_Core_Pair *get_pair_info_for_objs(IVP_Core *first,
                                                 IVP_Core *second) {
    return BML::IVP::ABI::InvokeThis<IVP_Friction_Core_Pair *>(
        BML::IVP::ABI::Address::FrictionSystemGetPairInfo, this, first, second);
  }
  IVP_Friction_Core_Pair *find_pair_of_cores(IVP_Core *first,
                                             IVP_Core *second) {
    return BML::IVP::ABI::InvokeThis<IVP_Friction_Core_Pair *>(
        BML::IVP::ABI::Address::FrictionSystemFindPair, this, first, second);
  }
  void exchange_friction_dists(IVP_Contact_Point *first,
                               IVP_Contact_Point *second) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSystemExchangeDistances, this, first,
        second);
  }
  void add_dist_to_system(IVP_Contact_Point *contactPoint) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSystemAddDistance, this, contactPoint);
  }
  void remove_dist_from_system(IVP_Contact_Point *contactPoint) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSystemRemoveDistance, this,
        contactPoint);
  }
  void delete_friction_distance(IVP_Contact_Point *contactPoint) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSystemDeleteDistance, this,
        contactPoint);
  }
  IVP_BOOL dist_removed_update_pair_info(IVP_Contact_Point *contactPoint) {
    return BML::IVP::ABI::InvokeThis<IVP_BOOL>(
        BML::IVP::ABI::Address::FrictionSystemUpdateRemovedPair, this,
        contactPoint);
  }
  void dist_added_update_pair_info(IVP_Contact_Point *contactPoint) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSystemUpdateAddedPair, this,
        contactPoint);
  }
  void fs_recalc_all_contact_points() {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSystemRecalculateAllContacts, this);
  }
  void static_fr_oversized_matrix_panic() {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSystemOversizedMatrixPanic, this);
  }
  IVP_BOOL core_is_terminal_in_fs(IVP_Core *core) {
    return BML::IVP::ABI::InvokeThis<IVP_BOOL>(
        BML::IVP::ABI::Address::FrictionSystemCoreIsTerminal, this, core);
  }
  void reorder_mindists_for_complex() {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSystemReorderMindists, this);
  }
  void do_friction_system(const IVP_Event_Sim *event) {
    BML::IVP::ABI::InvokeThis<void>(BML::IVP::ABI::Address::FrictionSystemRun,
                                    this, event);
  }
  void confirm_complex_pushes() {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSystemConfirmComplexPushes, this);
  }
  void undo_complex_pushes() {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSystemUndoComplexPushes, this);
  }
  int get_num_supposed_active_frdists() {
    return BML::IVP::ABI::InvokeThis<int>(
        BML::IVP::ABI::Address::FrictionSystemGetSupposedActiveCount, this);
  }
  void apply_real_friction(const IVP_Event_Sim *event) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSystemApplyRealFriction, this, event);
  }
  void clear_integrated_anti_energy() {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSystemClearIntegratedEnergy, this);
  }
  void remove_energy_gained_by_real_friction() {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSystemRemoveGainedEnergy, this);
  }
  void ease_friction_forces() {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSystemEaseForces, this);
  }
  void calc_friction_forces(const IVP_Event_Sim *event) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSystemCalcForces, this, event);
  }
  IVP_BOOL core_is_found_in_pairs(IVP_Core *core) const {
    const IVP_U_Vector<IVP_Friction_Core_Pair> *pairs = get_friction_pairs();
    for (int index = pairs->len() - 1; index >= 0; --index) {
      const IVP_Friction_Core_Pair *pair = pairs->element_at(index);
      if (pair->objs[0] == core || pair->objs[1] == core)
        return IVP_TRUE;
    }
    return IVP_FALSE;
  }
  void bubble_sort_dists_importance() {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSystemBubbleSortDistances, this);
  }
  IVP_DOUBLE get_max_energy_gain() {
    return BML::IVP::ABI::InvokeThis<IVP_DOUBLE>(
        BML::IVP::ABI::Address::FrictionSystemGetMaxEnergyGain, this);
  }
  IVP_DOUBLE kinetic_energy_of_hole_frs() {
    return BML::IVP::ABI::InvokeThis<IVP_DOUBLE>(
        BML::IVP::ABI::Address::FrictionSystemKineticEnergy, this);
  }
  void fusion_friction_systems(IVP_Friction_System *secondSystem) {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSystemFusion, this, secondSystem);
  }
  IVP_Core *union_find_fr_sys() {
    return BML::IVP::ABI::InvokeThis<IVP_Core *>(
        BML::IVP::ABI::Address::FrictionSystemUnionFind, this);
  }
  void split_friction_system(IVP_Core *splitFather) {
    BML::IVP::ABI::InvokeThis<void>(BML::IVP::ABI::Address::FrictionSystemSplit,
                                    this, splitFather);
  }

  // Present only in IVP builds with the distance-keeper solver enabled.
  // Ballance's retained do_friction_system has no such branch, so no retail
  // entry point is claimed here.
  void do_pushes_distance_keepers(const IVP_Event_Sim *event);

  void core_is_going_to_be_deleted_event(IVP_Core *) override {}
  IVP_DOUBLE get_minimum_simulation_frequency() override { return 1.0; }
  IVP_U_Vector<IVP_Core> *get_associated_controlled_cores() override {
    return BML::IVP::ABI::InvokeThis<IVP_U_Vector<IVP_Core> *>(
        BML::IVP::ABI::Address::FrictionSystemGetAssociatedCores, this);
  }
  void get_controlled_cores(IVP_U_Vector<IVP_Core> *cores) {
    // This obsolete neighboring-revision method assigns nullptr only to its
    // by-value parameter. The optimized Ballance operation is therefore a
    // no-op; callers should use get_associated_controlled_cores().
    (void)cores;
  }
  void reset_time(IVP_Time offset) override {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSystemResetTime, this, offset);
  }
  void do_simulation_controller(IVP_Event_Sim *event,
                                IVP_U_Vector<IVP_Core> *coreList) override {
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::FrictionSystemDoSimulation, this, event,
        coreList);
  }
  IVP_CONTROLLER_PRIORITY get_controller_priority() override {
    return IVP_CP_DYNAMIC_FRICTION;
  }

  // Debug-only neighboring declarations. No callable body survives in the
  // Ballance retail DLL; keeping declarations preserves source compatibility
  // without manufacturing release behavior.
  void debug_clean_tmp_info();
  void debug_check_system_consistency();
  void test_hole_fr_system_data();
  void print_all_dists();
  void debug_fs_out_ascii();
  void ivp_debug_fs_pointers();
  void debug_fs_after_complex();

private:
  static IVP_Contact_Point *contact_link(const IVP_Contact_Point *contactPoint,
                                         std::size_t offset) {
    if (!contactPoint)
      return nullptr;
    return *reinterpret_cast<IVP_Contact_Point *const *>(
        reinterpret_cast<const std::byte *>(contactPoint) + offset);
  }
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_Friction_System) == 0x50);
static_assert(alignof(IVP_Friction_System) == 0x08);
static_assert(offsetof(IVP_Friction_System, cores_of_friction_system) == 0x24);
static_assert(offsetof(IVP_Friction_System, moveable_cores_of_friction_system) ==
              0x2C);
static_assert(offsetof(IVP_Friction_System, fr_pairs_of_objs) == 0x34);
static_assert(offsetof(IVP_Friction_System, sum_energy_destroyed) == 0x48);
static_assert(sizeof(IVP_Contact_Point) == 0x78);
static_assert(alignof(IVP_Contact_Point) == 0x08);
static_assert(sizeof(IVP_Friction_Info_For_Core) == 0x0C);
static_assert(sizeof(IVP_Friction_Core_Pair) == 0x38);
static_assert(alignof(IVP_Friction_Core_Pair) == 0x08);
static_assert(BML_IvpFrictionCorePairLayoutCheck::contact_points == 0x00);
static_assert(BML_IvpFrictionCorePairLayoutCheck::span_vector == 0x08);
static_assert(sizeof(IVP_Mutual_Energizer) == 0xA0);
static_assert(alignof(IVP_Mutual_Energizer) == 0x08);
static_assert(offsetof(IVP_Mutual_Energizer, core) == 0x90);
static_assert(offsetof(IVP_Mutual_Energizer, whole_mutual_energy) == 0x98);
static_assert(sizeof(IVP_Friction_Sys_Energy) == 0x08);
static_assert(sizeof(IVP_Friction_Sys_Static) == 0x08);
static_assert(offsetof(IVP_Friction_Sys_Energy, l_friction_system) == 0x04);
static_assert(offsetof(IVP_Friction_Sys_Static, l_friction_system) == 0x04);
static_assert(sizeof(IVP_Impact_Solver_Long_Term) == 0xE0);
static_assert(alignof(IVP_Impact_Solver_Long_Term) == 0x08);
static_assert(offsetof(IVP_Impact_Solver_Long_Term, index_in_fs) == 0x58);
static_assert(offsetof(IVP_Impact_Solver_Long_Term, virtual_mass) == 0x70);
static_assert(offsetof(IVP_Impact_Solver_Long_Term, contact_core) == 0x78);
static_assert(offsetof(IVP_Impact_Solver_Long_Term, span_friction_v) == 0x80);
static_assert(offsetof(IVP_Impact_Solver_Long_Term, contact_point_cs) == 0xA0);
static_assert(offsetof(IVP_Impact_Solver_Long_Term, contact_cross_nomal_cs) ==
              0xC0);
static_assert(offsetof(IVP_Friction_Core_Pair, next_ease_nr_psi) == 0x18);
static_assert(offsetof(IVP_Friction_Core_Pair, last_impact_time_pair) == 0x20);
static_assert(offsetof(IVP_Friction_Core_Pair, integrated_anti_energy) == 0x28);
static_assert(offsetof(IVP_Friction_Core_Pair, objs) == 0x2C);
#endif

#endif // BML_IVP_FRICTION_H
