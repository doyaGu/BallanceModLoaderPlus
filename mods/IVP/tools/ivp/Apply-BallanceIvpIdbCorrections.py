"""Apply evidence-backed corrections to the Ballance physics_RT IDA database.

Run this only against the verified analysis database with IDA's batch
executable. The script refuses a database whose image base, listener vtable,
or function boundaries do not match the Ballance retail image. An optional
first argument exports the corrected named-function catalog as ``name<TAB>rva``.
"""

from __future__ import annotations

import os
from pathlib import Path

import ida_auto
import ida_bytes
import ida_funcs
import ida_ida
import ida_loader
import ida_name
import ida_nalt
import ida_pro
import ida_typeinf
import idautils
import idc


IMAGE_BASE = 0x10000000


CORRECTIONS = (
    (0x10016E30, "?insert_exact_mindist@IVP_Mindist_Manager@@QAEXPAVIVP_Mindist@@@Z",
     "void __thiscall corrected(IVP_Mindist_Manager *self, IVP_Mindist *mindist)",
     "Retained exact-Mindist list insertion. ECX supplies the 0x18-byte manager, RET 4 proves one pointer parameter, and the body links both embedded synapses before optionally appending car-wheel contacts."),
    (0x1001DA10, "?p_strdup@@YAPADPBD@Z",
     "char *__cdecl corrected(const char *source)",
     "Retail allocator-backed string duplication helper. The decorated free-function signature, plain RET, null fast path, strlen+1 allocation and byte copy establish cdecl const-char input and caller-owned char-pointer return."),
    (0x1001DAB0, "?p_strlen@@YAHPBD@Z",
     "int __cdecl corrected(const char *source)",
     "Retained public null-tolerant string length helper. Decorated YAH/PBD spelling proves cdecl int return and const-char input; the body returns zero for null or empty input and otherwise performs a byte scan."),
    (0x1001DAD0, "?p_strcmp@@YAHPBD0@Z",
     "int __cdecl corrected(const char *left, const char *right)",
     "Retained public null-tolerant string comparison helper. Decorated YAH/PBD0 spelling proves cdecl int return and two const-char inputs; plain RET and the explicit null branches agree with the nearby declaration."),
    (0x100200D0, "?p_calloc@@YAPADHH@Z",
     "char *__cdecl corrected(int count, int size)",
     "Retained public zeroing allocator. Decorated YAPADHH spelling proves char-pointer return and two 32-bit integer arguments; the body multiplies them, calls the retail malloc import and clears exactly that byte count."),
    (0x10020130, "?ivp_free_aligned@@YAXPAX@Z",
     "void __cdecl corrected(void *memory)",
     "Retained public aligned deallocator. Decorated YAXPAX spelling proves a void-pointer argument, not the imported void-pointer-pointer guess; the body loads the allocator backlink at memory[-4] and releases it through the retail free import."),
    (0x1002FCD0, "?ivp_rand@@YAMXZ",
     "float __cdecl corrected(void)",
     "Retained public random-number function. Decorated return code M and the neighboring public declaration establish binary32 IVP_FLOAT; the old imported double return type was ABI-wrong. The body multiplies process-global seed 0x100685B4 by 75 and returns its low 16 bits scaled by 1/65536."),
    (0x10018850, "??0?$IVP_U_BigVector@VIVP_Compact_Ledge@@@@IAE@PAPAXH@Z",
     "void *__thiscall corrected(void *self, void **preallocated_elements, int size)",
     "Retail protected IVP_U_BigVector<IVP_Compact_Ledge> complete constructor. IDA's legacy parser cannot spell the template self type; source declaration, PAPAX/H decoration, RET 8 and returned self establish the exact pointer ABI."),
    (0x100188C0, "?swap_elems@?$IVP_U_FVector@VIVP_Collision@@@@QAEXHH@Z",
     "void __thiscall corrected(void *self, int index0, int index1)",
     "Retail IVP_U_FVector<IVP_Collision>::swap_elems specialization. IDA's legacy parser cannot spell the template self type; inline source and decorated integer pair establish the ABI."),
    (0x1001ECD0, "?add@?$IVP_U_Vector@V?$IVP_Listener_Set_Active@VIVP_Core@@@@@@QAEHPAV?$IVP_Listener_Set_Active@VIVP_Core@@@@@Z",
     "int __thiscall corrected(void *self, void *listener)",
     "Retail IVP_U_Vector<IVP_Listener_Set_Active<IVP_Core>>::add specialization. Both opaque pointers have the exact nested template types named by the decoration; inline source, integer result and RET 4 establish the ABI."),
    (0x10009B80, "?init@IVP_Object@@AAEXPAVIVP_Environment@@@Z",
     "void __thiscall corrected(IVP_Object *self, IVP_Environment *environment)",
     "Retail protected Object initializer; decorated Environment pointer and RET 4 establish the ABI."),
    (0x10009D10, "?add_object@IVP_Cluster@@IAEXPAVIVP_Object@@@Z",
     "void __thiscall corrected(IVP_Cluster *self, IVP_Object *object)",
     "Retail protected Cluster object insertion; decorated Object pointer and RET 4 establish the ABI."),
    (0x10009D40, "?remove_object@IVP_Cluster@@IAEXPAVIVP_Object@@@Z",
     "void __thiscall corrected(IVP_Cluster *self, IVP_Object *object)",
     "Retail protected Cluster object removal; decorated Object pointer and RET 4 establish the ABI."),
    (0x1000A5D0, "?fire_event_object_deleted@IVP_Cluster_Manager@@QAEXPAVIVP_Event_Object@@@Z",
     "void __thiscall corrected(IVP_Cluster_Manager *self, IVP_Event_Object *event_data)",
     "Retail Cluster Manager object-deleted dispatcher; decorated event pointer and RET 4 establish the ABI."),
    (0x1000A6B0, "?fire_event_object_frozen@IVP_Cluster_Manager@@QAEXPAVIVP_Event_Object@@@Z",
     "void __thiscall corrected(IVP_Cluster_Manager *self, IVP_Event_Object *event_data)",
     "Retail Cluster Manager object-frozen dispatcher; decorated event pointer and RET 4 establish the ABI."),
    (0x1000A710, "?fire_event_object_revived@IVP_Cluster_Manager@@QAEXPAVIVP_Event_Object@@@Z",
     "void __thiscall corrected(IVP_Cluster_Manager *self, IVP_Event_Object *event_data)",
     "Retail Cluster Manager object-revived dispatcher; decorated event pointer and RET 4 establish the ABI."),
    (0x1000A960, "?get_next_real_object_in_cluster_tree@IVP_Cluster_Manager@@AAEPAVIVP_Real_Object@@PAVIVP_Object@@@Z",
     "IVP_Real_Object *__thiscall corrected(IVP_Cluster_Manager *self, IVP_Object *object)",
     "Retail protected Cluster traversal helper; decorated Object input and Real Object return plus RET 4 establish the ABI."),
    (0x1000AE00, "?remove_object@IVP_Cluster_Manager@@QAEXPAVIVP_Real_Object@@@Z",
     "void __thiscall corrected(IVP_Cluster_Manager *self, IVP_Real_Object *object)",
     "Retail Cluster Manager Real Object removal; decorated pointer and RET 4 establish the ABI."),
    (0x1000B0C0, "?get_controller_priority@IVP_Friction_System@@UAE?AW4IVP_CONTROLLER_PRIORITY@@XZ",
     "IVP_CONTROLLER_PRIORITY __thiscall corrected(IVP_Friction_System *self)",
     "Retail Friction System controller-priority virtual; decorated enum return and parameterless RET establish the ABI."),
    (0x1000B190, "?fs_recalc_all_contact_points@IVP_Friction_System@@QAEXXZ",
     "void __thiscall corrected(IVP_Friction_System *self)",
     "Retail parameterless Friction System contact-point recomputation."),
    (0x1000B210, "?dist_removed_update_pair_info@IVP_Friction_System@@QAE?AW4IVP_BOOL@@PAVIVP_Contact_Point@@@Z",
     "IVP_BOOL __thiscall corrected(IVP_Friction_System *self, IVP_Contact_Point *contact)",
     "Retail friction distance-removal pair update; decorated Contact Point input, IVP_BOOL return and RET 4 establish the ABI."),
    (0x1000B290, "?remove_dist_from_system@IVP_Friction_System@@QAEXPAVIVP_Contact_Point@@@Z",
     "void __thiscall corrected(IVP_Friction_System *self, IVP_Contact_Point *contact)",
     "Retail Friction System contact removal; decorated Contact Point pointer and RET 4 establish the ABI."),
    (0x1000B2C0, "?dist_added_update_pair_info@IVP_Friction_System@@QAEXPAVIVP_Contact_Point@@@Z",
     "void __thiscall corrected(IVP_Friction_System *self, IVP_Contact_Point *contact)",
     "Retail friction distance-addition pair update; decorated Contact Point pointer and RET 4 establish the ABI."),
    (0x1000B390, "?add_core_to_system@IVP_Friction_System@@QAEXPAVIVP_Core@@@Z",
     "void __thiscall corrected(IVP_Friction_System *self, IVP_Core *core)",
     "Retail Friction System Core insertion; decorated Core pointer and RET 4 establish the ABI."),
    (0x1000B410, "?remove_core_from_system@IVP_Friction_System@@QAEXPAVIVP_Core@@@Z",
     "void __thiscall corrected(IVP_Friction_System *self, IVP_Core *core)",
     "Retail Friction System Core removal; decorated Core pointer and RET 4 establish the ABI."),
    (0x1000B590, "?revive_cores_PSI@IVP_Environment@@AAEXXZ",
     "void __thiscall corrected(IVP_Environment *self)",
     "Retail protected Environment PSI Core-revival pass; parameterless thiscall."),
    (0x1000B6F0, "?find@IVP_Hash@@QBEPAXPBD@Z",
     "void *__thiscall corrected(const IVP_Hash *self, const char *key)",
     "Retail const Hash lookup; decorated const char key and void-pointer return plus RET 4 establish the ABI."),
    (0x1000B770, "?add@IVP_Hash@@QAEXPBDPAX@Z",
     "void __thiscall corrected(IVP_Hash *self, const char *key, void *value)",
     "Retail Hash insertion; decorated const char key, opaque value and RET 8 establish the ABI."),
    (0x1000BEF0, "?get_adhesion@IVP_Material_Simple@@UAENXZ",
     "double __thiscall corrected(IVP_Material_Simple *self)",
     "Ballance-specific Material Simple adhesion virtual; decorated x87 double return and parameterless thiscall."),
    (0x1000C310, "?p_calc_2d_distances_to_axis@@YAXPBVIVP_U_Float_Point@@0PAV1@@Z",
     "void __cdecl corrected(const IVP_U_Float_Point *point0, const IVP_U_Float_Point *point1, IVP_U_Float_Point *result)",
     "Retail free 2D axis-distance helper; decorated const-type backreference and cdecl calling convention establish three pointer parameters."),
    (0x1000D110, "?get_mem@IVP_U_Memory@@QAEPAXI@Z",
     "void *__thiscall corrected(IVP_U_Memory *self, unsigned int size)",
     "Retail transient-memory allocation helper; decorated unsigned size, opaque return and RET 4 establish the ABI."),
    (0x1000E300, "?solve_quadratic_equation_accurate@IVP_U_Point@@QAEXPBV1@@Z",
     "void __thiscall corrected(IVP_U_Point *self, const IVP_U_Point *coefficients)",
     "Retail accurate quadratic helper; decorated const same-class pointer and RET 4 establish the ABI."),
    (0x1000F870, "?calc_an_orthogonal@IVP_U_Point@@QAEXPBV1@@Z",
     "void __thiscall corrected(IVP_U_Point *self, const IVP_U_Point *source)",
     "Retail Point orthogonal-vector helper; decorated const same-class pointer and RET 4 establish the ABI."),
    (0x1000F940, "?use_buoyancy_solver@IVP_Controller_Buoyancy@@AAE?AW4IVP_BOOL@@PBVIVP_Buoyancy_Input@@PBVIVP_Template_Buoyancy@@PAVIVP_Buoyancy_Output@@PBVIVP_U_Float_Point@@H@Z",
     "IVP_BOOL __thiscall corrected(IVP_Controller_Buoyancy *self, const IVP_Buoyancy_Input *input, const IVP_Template_Buoyancy *definition, IVP_Buoyancy_Output *output, const IVP_U_Float_Point *relative_speed, int object_count)",
     "Retail private Buoyancy solver dispatch; decorated constness/order, IVP_BOOL return and RET 0x14 establish the ABI."),
    (0x1000FE50, "?do_simulation_controller@IVP_Controller_Buoyancy@@MAEXPAVIVP_Event_Sim@@PAV?$IVP_U_Vector@VIVP_Core@@@@@Z",
     "void __thiscall corrected(IVP_Controller_Buoyancy *self, IVP_Event_Sim *event_data, void *controlled_cores)",
     "Retail protected Buoyancy simulation virtual. controlled_cores is specifically IVP_U_Vector<IVP_Core>*; IDA's legacy parser cannot spell the template type. Decorated parameters and RET 8 establish the pointer ABI."),
    (0x10010400, "?get_controller_priority@IVP_Controller_Buoyancy@@MAE?AW4IVP_CONTROLLER_PRIORITY@@XZ",
     "IVP_CONTROLLER_PRIORITY __thiscall corrected(IVP_Controller_Buoyancy *self)",
     "Retail protected Buoyancy controller-priority virtual; decorated enum return and parameterless thiscall."),
    (0x10011ED0, "?add_unit_to_slot@IVP_Sim_Units_Manager@@QAEXPAVIVP_Simulation_Unit@@PAPAV2@@Z",
     "void __thiscall corrected(IVP_Sim_Units_Manager *self, IVP_Simulation_Unit *unit, IVP_Simulation_Unit **slot)",
     "Retail Simulation Units Manager slot insertion; decorated class-pointer backreference and RET 8 establish the ABI."),
    (0x10011F20, "?rem_unit_from_slot@IVP_Sim_Units_Manager@@QAEXPAVIVP_Simulation_Unit@@PAPAV2@@Z",
     "void __thiscall corrected(IVP_Sim_Units_Manager *self, IVP_Simulation_Unit *unit, IVP_Simulation_Unit **slot)",
     "Retail Simulation Units Manager slot removal; decorated class-pointer backreference and RET 8 establish the ABI."),
    (0x10011F80, "?sim_unit_revive_for_simulation@IVP_Simulation_Unit@@QAEXPAVIVP_Environment@@@Z",
     "void __thiscall corrected(IVP_Simulation_Unit *self, IVP_Environment *environment)",
     "Retail Simulation Unit revival; decorated Environment pointer and RET 4 establish the ABI."),
    (0x100120D0, "?do_sim_unit_union_find@IVP_Simulation_Unit@@QAEXXZ",
     "void __thiscall corrected(IVP_Simulation_Unit *self)",
     "Retail parameterless Simulation Unit union-find pass."),
    (0x10016410, "?get_objects@IVP_Mindist_Base@@UAEXQAPAVIVP_Real_Object@@@Z",
     "void __thiscall corrected(IVP_Mindist_Base *self, IVP_Real_Object **objects_out)",
     "Retail Mindist Base object-pair virtual; decorated output pointer and RET 4 establish the ABI."),
    (0x10016430, "?get_ledges@IVP_Mindist_Base@@UAEXQAPBVIVP_Compact_Ledge@@@Z",
     "void __thiscall corrected(IVP_Mindist_Base *self, const IVP_Compact_Ledge **ledges_out)",
     "Retail Mindist Base ledge-pair virtual; decorated pointer-to-const-pointer output and RET 4 establish the ABI."),
    (0x10016490, "?init_mindist@IVP_Mindist@@QAEXPAVIVP_Real_Object@@0PBVIVP_Compact_Edge@@1@Z",
     "void __thiscall corrected(IVP_Mindist *self, IVP_Real_Object *object0, IVP_Real_Object *object1, const IVP_Compact_Edge *edge0, const IVP_Compact_Edge *edge1)",
     "Retail Mindist initializer; decorated repeated Real Object and const Compact Edge types plus RET 0x10 establish the four-pointer ABI."),
    (0x10016630, "?update_synapse@IVP_Synapse_Real@@QAEXPBVIVP_Compact_Edge@@W4IVP_SYNAPSE_POLYGON_STATUS@@@Z",
     "void __thiscall corrected(IVP_Synapse_Real *self, const IVP_Compact_Edge *edge, IVP_SYNAPSE_POLYGON_STATUS status)",
     "Retail Real Synapse update; decorated const Compact Edge pointer, polygon-status enum and RET 8 establish the ABI."),
    (0x10016F70, "?exact_mindist_went_invalid@IVP_Mindist@@UAEXPAVIVP_Mindist_Manager@@@Z",
     "void __thiscall corrected(IVP_Mindist *self, IVP_Mindist_Manager *manager)",
     "Retail Mindist invalidation virtual; decorated manager pointer and RET 4 establish the ABI."),
    (0x100240A0, "?do_impact@IVP_Mindist@@UAEXXZ",
     "void __thiscall corrected(IVP_Mindist *self)",
     "Retail Mindist impact virtual at exact primary-vtable slot 7. The parameterless body uses ECX as the complete Mindist, revives and synchronizes both synapse objects, then enters the retained impact solver."),
    (0x10016F90, "?insert_and_recalc_exact_mindist@IVP_Mindist_Manager@@QAEXPAVIVP_Mindist@@@Z",
     "void __thiscall corrected(IVP_Mindist_Manager *self, IVP_Mindist *mindist)",
     "Retail manager exact-Mindist insertion/recalculation; decorated pointer and RET 4 establish the ABI."),
    (0x100176E0, "?enable_collision_detection_for_object@IVP_Mindist_Manager@@QAEXPAVIVP_Real_Object@@@Z",
     "void __thiscall corrected(IVP_Mindist_Manager *self, IVP_Real_Object *object)",
     "Retail manager collision-enable entry; decorated Real Object pointer and RET 4 establish the ABI."),
    (0x10017770, "?recalc_all_exact_mindists_events@IVP_Mindist_Manager@@QAEXXZ",
     "void __thiscall corrected(IVP_Mindist_Manager *self)",
     "Retail parameterless manager pass that recalculates all exact-Mindist events."),
    (0x10017850, "?recalc_all_exact_mindists@IVP_Mindist_Manager@@QAEXXZ",
     "void __thiscall corrected(IVP_Mindist_Manager *self)",
     "Retail parameterless manager pass that recalculates all exact Mindists."),
    (0x10017D10, "?hull_limit_exceeded_event@IVP_Synapse@@MAEXPAVIVP_Hull_Manager@@M@Z",
     "void __thiscall corrected(IVP_Synapse *self, IVP_Hull_Manager *manager, float intrusion_value)",
     "Retail protected Synapse hull-limit virtual; decorated Hull Manager pointer, float value and RET 8 establish the ABI."),
    (0x10017D30, "?hull_manager_is_going_to_be_deleted_event@IVP_Synapse@@MAEXPAVIVP_Hull_Manager@@@Z",
     "void __thiscall corrected(IVP_Synapse *self, IVP_Hull_Manager *manager)",
     "Retail protected Synapse hull-manager deletion virtual; decorated pointer and RET 4 establish the ABI."),
    (0x10018040, "?create_cp_in_advance_pretension@IVP_Mindist@@QAEXPAVIVP_Real_Object@@M@Z",
     "void __thiscall corrected(IVP_Mindist *self, IVP_Real_Object *object, float gap_length)",
     "Retail advance-pretension contact creation; decorated Real Object pointer, float argument and RET 8 establish the ABI."),
    (0x10018230, "?remove_exact_mindist@IVP_Mindist_Manager@@QAEXPAVIVP_Mindist@@@Z",
     "void __thiscall corrected(IVP_Mindist_Manager *self, IVP_Mindist *mindist)",
     "Retail manager exact-Mindist removal; decorated pointer and RET 4 establish the ABI."),
    (0x10018390, "?remove_hull_mindist@IVP_Mindist_Manager@@QAEXPAVIVP_Mindist@@@Z",
     "void __thiscall corrected(IVP_Mindist_Manager *self, IVP_Mindist *mindist)",
     "Retail manager hull-Mindist removal; decorated pointer and RET 4 establish the ABI."),
    (0x10018660, "?mindist_entered_phantom@IVP_Mindist_Manager@@SAXPAVIVP_Mindist@@@Z",
     "void __cdecl corrected(IVP_Mindist *mindist)",
     "Retail static phantom-entry dispatcher; decorated static signature establishes one Mindist pointer argument."),
    (0x100186A0, "?mindist_left_phantom@IVP_Mindist_Manager@@SAXPAVIVP_Mindist@@@Z",
     "void __cdecl corrected(IVP_Mindist *mindist)",
     "Retail static phantom-exit dispatcher; decorated static signature establishes one Mindist pointer argument."),
    (0x10019A80, "?get_sorted_synapse@IVP_Mindist@@QBEPAVIVP_Synapse_Real@@H@Z",
     "IVP_Synapse_Real *__thiscall corrected(const IVP_Mindist *self, int index)",
     "Retail const sorted-Synapse accessor; decorated Real Synapse return, integer index and RET 4 establish the ABI."),
    (0x10016D80, "?calc_hash_index@IVP_MM_CMP@@SAHPAVIVP_MM_CMP_Key@@@Z",
     "int __cdecl corrected(IVP_MM_CMP_Key *key)",
     "Retail static Mindist-key hash. Decorated static integer signature and the retained ledge-pointer mixing body establish the ABI."),
    (0x10016DB0, "?calc_hash_index@IVP_MM_CMP@@SAHPAVIVP_Collision@@PAVIVP_MM_CMP_Key@@@Z",
     "int __cdecl corrected(IVP_Collision *collision, IVP_MM_CMP_Key *reference_key)",
     "Retail static collision hash overload; decorated parameters and collision ledge extraction agree with the neighboring comparator."),
    (0x10016DF0, "?are_equal@IVP_MM_CMP@@SA?AW4IVP_BOOL@@PAVIVP_Collision@@PAVIVP_MM_CMP_Key@@@Z",
     "IVP_BOOL __cdecl corrected(IVP_Collision *collision, IVP_MM_CMP_Key *search_key)",
     "Retail static Mindist comparator; decorated IVP_BOOL return and two pointer parameters match the retained two-ledge comparison."),
    (0x100176A0, "?calc_hash_index@IVP_OO_CMP@@SAHPAVIVP_Collision@@PAVIVP_Real_Object@@@Z",
     "int __cdecl corrected(IVP_Collision *collision, IVP_Real_Object *reference_object)",
     "Retail static object-object collision hash overload; decorated integer return and two pointer arguments establish the cdecl ABI."),
    (0x1001C780, "?get_pair_info_for_objs@IVP_Friction_System@@QAEPAVIVP_Friction_Core_Pair@@PAVIVP_Core@@0@Z",
     "IVP_Friction_Core_Pair *__thiscall corrected(IVP_Friction_System *self, IVP_Core *core0, IVP_Core *core1)",
     "Retail Friction System pair lookup/creation; decorated return and repeated Core pointer plus RET 8 establish the ABI."),
    (0x1001C7F0, "?union_find_fr_sys@IVP_Friction_System@@QAEPAVIVP_Core@@XZ",
     "IVP_Core *__thiscall corrected(IVP_Friction_System *self)",
     "Retail parameterless Friction System union-find entry with a decorated Core pointer return."),
    (0x1001C8A0, "?split_friction_system@IVP_Friction_System@@QAEXPAVIVP_Core@@@Z",
     "void __thiscall corrected(IVP_Friction_System *self, IVP_Core *split_father)",
     "Retail Friction System split entry; decorated Core pointer and RET 4 agree with the recursive union-find caller."),
    (0x1001CC20, "?check_all_fr_mindists_to_be_valid@IVP_Friction_Core_Pair@@QAEHPAVIVP_Friction_System@@@Z",
     "int __thiscall corrected(IVP_Friction_Core_Pair *self, IVP_Friction_System *system)",
     "Retail pair validity pass; decorated integer return, Friction System pointer and RET 4 establish the ABI."),
    (0x1001D380, "?find_pair_of_cores@IVP_Friction_System@@QAEPAVIVP_Friction_Core_Pair@@PAVIVP_Core@@0@Z",
     "IVP_Friction_Core_Pair *__thiscall corrected(IVP_Friction_System *self, IVP_Core *core0, IVP_Core *core1)",
     "Retail Friction System non-creating pair lookup; decorated return and repeated Core pointer plus RET 8 establish the ABI."),
    (0x1001D600, "?get_associated_controlled_cores@IVP_Friction_System@@UAEPAV?$IVP_U_Vector@VIVP_Core@@@@XZ",
     "void *__thiscall corrected(IVP_Friction_System *self)",
     "Retail Friction System controlled-core virtual. The return is specifically IVP_U_Vector<IVP_Core>*; IDA's legacy parser cannot spell the template type, while the three-byte LEA/RET body fixes the pointer ABI."),
    (0x1001D750, "?do_simulation_controller@IVP_Friction_System@@UAEXPAVIVP_Event_Sim@@PAV?$IVP_U_Vector@VIVP_Core@@@@@Z",
     "void __thiscall corrected(IVP_Friction_System *self, IVP_Event_Sim *event_data, void *controlled_cores)",
     "Retail Friction System simulation virtual. controlled_cores is specifically IVP_U_Vector<IVP_Core>*; decorated parameters and RET 8 fix the ABI despite IDA's template parser limitation."),
    (0x1001D830, "?get_material_index@IVP_Synapse_Friction@@QBEHXZ",
     "int __thiscall corrected(const IVP_Synapse_Friction *self)",
     "Retail const Friction Synapse material-index query; decorated integer return and parameterless RET establish the ABI."),
    (0x1001D840, "?is_same_as@IVP_Synapse_Friction@@QBE?AW4IVP_BOOL@@PBVIVP_Synapse_Real@@@Z",
     "IVP_BOOL __thiscall corrected(const IVP_Synapse_Friction *self, const IVP_Synapse_Real *other)",
     "Retail const Friction Synapse topology comparison; decorated IVP_BOOL return, const Real Synapse pointer and RET 4 establish the ABI."),
    (0x1001A5C0, "?minimize_on_other_side@IVP_Compact_Ledge_Solver@@SAPBVIVP_Compact_Edge@@PBV2@PBVIVP_U_Point@@@Z",
     "const IVP_Compact_Edge *__cdecl corrected(const IVP_Compact_Edge *edge, const IVP_U_Point *partner_position)",
     "Retail static opposite-side convex traversal; decorated const Edge return and two const pointer parameters establish the ABI."),
    (0x100202E0, "?calc_bounding_box@IVP_Compact_Ledge_Solver@@SAXPBVIVP_Compact_Ledge@@PAVIVP_U_Point@@1@Z",
     "void __cdecl corrected(const IVP_Compact_Ledge *ledge, IVP_U_Point *minimum_out, IVP_U_Point *maximum_out)",
     "Retail static compact-ledge bounds calculation; decorated const input and repeated mutable Point outputs establish the ABI."),
    (0x10020480, "?get_all_ledges@IVP_Compact_Ledge_Solver@@SAXPBVIVP_Compact_Surface@@PAV?$IVP_U_BigVector@VIVP_Compact_Ledge@@@@@Z",
     "void __cdecl corrected(const IVP_Compact_Surface *surface, void *ledges_out)",
     "Retail static compact-surface traversal. ledges_out is specifically IVP_U_BigVector<IVP_Compact_Ledge>*; IDA's legacy parser cannot spell the template type, while the decorated signature fixes the pointer ABI."),
    (0x10020710, "?calc_pos_other_space@IVP_Compact_Ledge_Solver@@SAXPBVIVP_Compact_Edge@@PAVIVP_Cache_Ledge_Point@@1PAVIVP_U_Point@@@Z",
     "void __cdecl corrected(const IVP_Compact_Edge *edge, IVP_Cache_Ledge_Point *source_cache, IVP_Cache_Ledge_Point *target_cache, IVP_U_Point *result)",
     "Retail static cross-object position transform; decorated type backreferences establish the four-pointer order."),
    (0x10020810, "?transform_vec_other_space@IVP_Compact_Ledge_Solver@@SAXPBVIVP_U_Point@@PAVIVP_Cache_Ledge_Point@@1PAV2@@Z",
     "void __cdecl corrected(const IVP_U_Point *vector, IVP_Cache_Ledge_Point *source_cache, IVP_Cache_Ledge_Point *target_cache, IVP_U_Point *result)",
     "Retail static cross-object vector transform; decorated const input, cache pair and Point output establish the ABI."),
    (0x100208F0, "?transform_pos_other_space@IVP_Compact_Ledge_Solver@@SAXPBVIVP_U_Float_Point@@PAVIVP_Cache_Ledge_Point@@1PAVIVP_U_Point@@@Z",
     "void __cdecl corrected(const IVP_U_Float_Point *position, IVP_Cache_Ledge_Point *source_cache, IVP_Cache_Ledge_Point *target_cache, IVP_U_Point *result)",
     "Retail static float-position cross-object transform; decorated parameter sequence establishes the ABI."),
    (0x10020A10, "?calc_unscaled_s_val_K_space@IVP_Compact_Ledge_Solver@@SAXPBVIVP_Compact_Ledge@@PBVIVP_Compact_Edge@@PBVIVP_U_Point@@PAVIVP_Unscaled_S_Result@@@Z",
     "void __cdecl corrected(const IVP_Compact_Ledge *ledge, const IVP_Compact_Edge *edge, const IVP_U_Point *position, IVP_Unscaled_S_Result *result)",
     "Retail static unscaled edge-coordinate kernel; decorated const inputs and mutable result establish the ABI."),
    (0x10020AD0, "?calc_unscaled_qr_vals_F_space@IVP_Compact_Ledge_Solver@@SAXPBVIVP_Compact_Ledge@@PBVIVP_Compact_Edge@@PBVIVP_U_Point@@PAVIVP_Unscaled_QR_Result@@@Z",
     "void __cdecl corrected(const IVP_Compact_Ledge *ledge, const IVP_Compact_Edge *triangle_edge, const IVP_U_Point *position, IVP_Unscaled_QR_Result *result)",
     "Retail static unscaled triangle-coordinate kernel; decorated const inputs and mutable result establish the ABI."),
    (0x10020DA0, "?calc_quad_distance_edge_edge@IVP_KK_Input@@QAENXZ",
     "double __thiscall corrected(IVP_KK_Input *self)",
     "Retail parameterless edge-pair squared-distance member; decorated N return and x87 result path establish double rather than integer."),
    (0x10020E60, "?calc_unscaled_KK_vals@IVP_Compact_Ledge_Solver@@SA?AW4IVP_RETURN_TYPE@@ABVIVP_KK_Input@@PAVIVP_Unscaled_KK_Result@@@Z",
     "IVP_RETURN_TYPE __cdecl corrected(const IVP_KK_Input *input, IVP_Unscaled_KK_Result *result)",
     "Retail static edge-edge coordinate kernel; decorated const-reference input is pointer ABI on x86 and mutable result follows it."),
    (0x10021210, "?calc_hesse_object@IVP_Compact_Ledge_Solver@@SAXPBVIVP_Compact_Edge@@PBVIVP_Compact_Ledge@@PAVIVP_U_Hesse@@@Z",
     "void __cdecl corrected(const IVP_Compact_Edge *edge, const IVP_Compact_Ledge *ledge, IVP_U_Hesse *hesse_out)",
     "Retail static object-space plane calculation; decorated two const geometry inputs and mutable Hesse output establish the ABI."),
    (0x10021280, "?calc_hesse_vec_object_not_normized@IVP_Compact_Ledge_Solver@@SAXPBVIVP_Compact_Edge@@PBVIVP_Compact_Ledge@@PAVIVP_U_Point@@@Z",
     "void __cdecl corrected(const IVP_Compact_Edge *edge, const IVP_Compact_Ledge *ledge, IVP_U_Point *vector_out)",
     "Retail static double-point unnormalized normal calculation; decorated overload type establishes the output ABI."),
    (0x10021350, "?calc_hesse_vec_object_not_normized@IVP_Compact_Ledge_Solver@@SAXPBVIVP_Compact_Edge@@PBVIVP_Compact_Ledge@@PAVIVP_U_Float_Point@@@Z",
     "void __cdecl corrected(const IVP_Compact_Edge *edge, const IVP_Compact_Ledge *ledge, IVP_U_Float_Point *vector_out)",
     "Retail static float-point unnormalized normal calculation; decorated overload type establishes the output ABI."),
    (0x10021420, "?quad_dist_edge_to_point_K_space@IVP_Compact_Ledge_Solver@@SANPBVIVP_Compact_Ledge@@PBVIVP_Compact_Edge@@PBVIVP_U_Point@@@Z",
     "double __cdecl corrected(const IVP_Compact_Ledge *ledge, const IVP_Compact_Edge *edge, const IVP_U_Point *position)",
     "Retail static edge-to-point squared distance; decorated N return and x87 result path establish double."),
    (0x100216F0, "?calc_qlen_PP_P_space@IVP_Compact_Ledge_Solver@@SANPBVIVP_Compact_Ledge@@PBVIVP_Compact_Edge@@PBVIVP_U_Point@@@Z",
     "double __cdecl corrected(const IVP_Compact_Ledge *ledge, const IVP_Compact_Edge *point_edge, const IVP_U_Point *position)",
     "Retail static point-point squared distance; decorated N return fixes the x87 double ABI."),
    (0x10021740, "?calc_qlen_PK_K_space@IVP_Compact_Ledge_Solver@@SANPBVIVP_U_Point@@PBVIVP_Compact_Ledge@@PBVIVP_Compact_Edge@@@Z",
     "double __cdecl corrected(const IVP_U_Point *point, const IVP_Compact_Ledge *ledge, const IVP_Compact_Edge *edge)",
     "Retail static point-to-edge squared distance; decorated N return and const parameter order establish the ABI."),
    (0x100217D0, "?give_world_coords_AT@IVP_Compact_Ledge_Solver@@SAXPBVIVP_Compact_Edge@@PAVIVP_Cache_Ledge_Point@@PAVIVP_U_Point@@@Z",
     "void __cdecl corrected(const IVP_Compact_Edge *edge, IVP_Cache_Ledge_Point *cache, IVP_U_Point *world_position_out)",
     "Retail static compact-vertex world transform; decorated input/cache/output sequence establishes the ABI."),
    (0x10021800, "?calc_qlen_KK@IVP_Compact_Ledge_Solver@@SANPBVIVP_Compact_Edge@@0PAVIVP_Cache_Ledge_Point@@1@Z",
     "double __cdecl corrected(const IVP_Compact_Edge *edge0, const IVP_Compact_Edge *edge1, IVP_Cache_Ledge_Point *cache0, IVP_Cache_Ledge_Point *cache1)",
     "Retail static edge-edge squared distance; decorated repeated Edge/cache types and N return establish four pointers plus x87 double result."),
    (0x100196F0, "?check_loop_hash@IVP_Mindist_Minimize_Solver@@IAE?AW4IVP_BOOL@@W4IVP_SYNAPSE_POLYGON_STATUS@@PBVIVP_Compact_Edge@@01@Z",
     "IVP_BOOL __thiscall corrected(IVP_Mindist_Minimize_Solver *self, IVP_SYNAPSE_POLYGON_STATUS status0, const IVP_Compact_Edge *edge0, IVP_SYNAPSE_POLYGON_STATUS status1, const IVP_Compact_Edge *edge1)",
     "Retail protected minimizer loop detector; decorated enum/const-edge repetition and RET 0x10 establish the four-argument member ABI."),
    (0x10019790, "?pierce_mindist@IVP_Mindist_Minimize_Solver@@QAEXXZ",
     "void __thiscall corrected(IVP_Mindist_Minimize_Solver *self)",
     "Retail parameterless Mindist piercing pass; public instance decoration and plain RET establish the ABI."),
    (0x10019AA0, "?p_minimize_FF@IVP_Mindist_Minimize_Solver@@IAE?AW4IVP_MRC_TYPE@@PBVIVP_Compact_Edge@@0PAVIVP_Cache_Ledge_Point@@1@Z",
     "IVP_MRC_TYPE __thiscall corrected(IVP_Mindist_Minimize_Solver *self, const IVP_Compact_Edge *edge0, const IVP_Compact_Edge *edge1, IVP_Cache_Ledge_Point *cache0, IVP_Cache_Ledge_Point *cache1)",
     "Retail protected face-face minimizer; decorated repeated Edge/cache types, enum return and RET 0x10 establish the ABI."),
    (0x10019FF0, "?minimize_default_poly_poly@IVP_Mindist_Minimize_Solver@@KA?AW4IVP_MRC_TYPE@@PAV1@@Z",
     "IVP_MRC_TYPE __cdecl corrected(IVP_Mindist_Minimize_Solver *solver)",
     "Retail protected static default polygon minimizer; decorated static enum return and one solver pointer establish cdecl ABI."),
    (0x1001A350, "?minimize_swapped_poly_poly@IVP_Mindist_Minimize_Solver@@KA?AW4IVP_MRC_TYPE@@PAV1@@Z",
     "IVP_MRC_TYPE __cdecl corrected(IVP_Mindist_Minimize_Solver *solver)",
     "Retail protected static swapped polygon minimizer; decorated static enum return and one solver pointer establish cdecl ABI."),
    (0x1001A6A0, "?minimize_illegal@IVP_Mindist_Minimize_Solver@@KA?AW4IVP_MRC_TYPE@@PAV1@@Z",
     "IVP_MRC_TYPE __cdecl corrected(IVP_Mindist_Minimize_Solver *solver)",
     "Retail protected static illegal-case minimizer; decorated static enum return and one solver pointer establish cdecl ABI."),
    (0x10020150, "?init_mem_transaction_usage@IVP_U_Memory@@QAEXPADH@Z",
     "void __thiscall corrected(IVP_U_Memory *self, char *external_memory, int size)",
     "Retail transaction-memory initializer; decorated mutable char buffer, integer size and RET 8 establish the ABI."),
    (0x10020210, "?neuer_sp_block@IVP_U_Memory@@QAEPADI@Z",
     "char *__thiscall corrected(IVP_U_Memory *self, unsigned int size)",
     "Retail scratchpad block allocator; decorated char-pointer return, unsigned size and RET 4 establish the ABI."),
    (0x10020270, "?init_mem@IVP_U_Memory@@QAEXXZ",
     "void __thiscall corrected(IVP_U_Memory *self)",
     "Retail parameterless scratchpad-memory reset entry."),
    (0x10022500, "?get_relative_speed_vector@IVP_Impact_Solver@@AAEXXZ",
     "void __thiscall corrected(IVP_Impact_Solver *self)",
     "Retail private Impact Solver relative-speed preparation; parameterless instance decoration and RET establish the ABI."),
    (0x10022580, "?get_world_push_direction@IVP_Impact_Solver@@AAEXXZ",
     "void __thiscall corrected(IVP_Impact_Solver *self)",
     "Retail private Impact Solver world push-direction preparation; parameterless instance decoration and RET establish the ABI."),
    (0x100228C0, "?confirm_impact@IVP_Impact_Solver@@AAEXH@Z",
     "void __thiscall corrected(IVP_Impact_Solver *self, int core_index)",
     "Retail private Impact Solver commit for one core; decorated integer argument and RET 4 establish the ABI."),
    (0x10022950, "?undo_push@IVP_Impact_Solver@@AAEXXZ",
     "void __thiscall corrected(IVP_Impact_Solver *self)",
     "Retail private Impact Solver push rollback; parameterless instance decoration and RET establish the ABI."),
    (0x10022A20, "?do_rescue_push@IVP_Impact_Solver@@AAEXPAVIVP_U_Float_Point@@W4IVP_BOOL@@@Z",
     "void __thiscall corrected(IVP_Impact_Solver *self, IVP_U_Float_Point *push_direction, IVP_BOOL panic_mode)",
     "Retail private rescue-push path; decorated mutable Float Point, IVP_BOOL and RET 8 establish the ABI."),
    (0x10023870, "?delay_of_impact@IVP_Impact_Solver@@AAEXH@Z",
     "void __thiscall corrected(IVP_Impact_Solver *self, int delayed_core_index)",
     "Retail private per-core impact delay; decorated integer argument and RET 4 establish the ABI."),
    (0x100238F0, "?clear_change_values_cores@IVP_Impact_Solver@@AAEXXZ",
     "void __thiscall corrected(IVP_Impact_Solver *self)",
     "Retail private Impact Solver temporary-core-state reset; parameterless instance decoration establishes the ABI."),
    (0x10023B90, "?calc_virt_masses_impact_solver@IVP_Impact_Solver@@AAEXPBVIVP_U_Float_Point@@@Z",
     "void __thiscall corrected(IVP_Impact_Solver *self, const IVP_U_Float_Point *world_normal)",
     "Retail private impact virtual-mass calculation; decorated const Float Point pointer and RET 4 establish the ABI."),
    (0x10023C80, "?estimate_push_impulse@IVP_Impact_Solver@@AAENXZ",
     "double __thiscall corrected(IVP_Impact_Solver *self)",
     "Retail private impulse estimator; decorated N return and x87 result path establish parameterless double-return ABI."),
    (0x10023FA0, "?get_material_info@IVP_Contact_Point@@QAEXQAPAVIVP_Material@@@Z",
     "void __thiscall corrected(IVP_Contact_Point *self, IVP_Material **materials_out)",
     "Retail Contact Point two-material query; decorated pointer-to-pointer output and RET 4 establish the ABI."),
    (0x100242C0, "?impact_system_check_start_pair@IVP_Impact_System@@AAEXPAVIVP_Friction_Core_Pair@@PAVIVP_Contact_Point@@@Z",
     "void __thiscall corrected(IVP_Impact_System *self, IVP_Friction_Core_Pair *start_pair, IVP_Contact_Point *contact)",
     "Retail private Impact System start-pair validation; decorated pair/contact pointers and RET 8 establish the ABI."),
    (0x10024450, "?add_pair_to_impact_system@IVP_Impact_System@@AAEXPAVIVP_Friction_Core_Pair@@@Z",
     "void __thiscall corrected(IVP_Impact_System *self, IVP_Friction_Core_Pair *pair)",
     "Retail private Impact System pair insertion; decorated pointer and RET 4 establish the ABI."),
    (0x10024480, "?synchronize_core_for_impact_system@IVP_Impact_System@@AAEXPAVIVP_Core@@@Z",
     "void __thiscall corrected(IVP_Impact_System *self, IVP_Core *core)",
     "Retail private Impact System Core synchronization; decorated Core pointer and RET 4 establish the ABI."),
    (0x100244A0, "?add_pushed_core_with_pairs_except@IVP_Impact_System@@AAEXPAVIVP_Core@@PAVIVP_Friction_Core_Pair@@@Z",
     "void __thiscall corrected(IVP_Impact_System *self, IVP_Core *core, IVP_Friction_Core_Pair *excluded_pair)",
     "Retail private pushed-Core expansion; decorated Core/pair parameters and RET 8 establish the ABI."),
    (0x10024530, "?pair_is_already_in_system@IVP_Impact_System@@AAE?AW4IVP_BOOL@@PAVIVP_Friction_Core_Pair@@@Z",
     "IVP_BOOL __thiscall corrected(IVP_Impact_System *self, IVP_Friction_Core_Pair *pair)",
     "Retail private Impact System pair-membership query; decorated IVP_BOOL return, pair pointer and RET 4 establish the ABI."),
    (0x10024700, "?invalidate_impact_mindists@IVP_Impact_System@@AAEXPAVIVP_Core@@@Z",
     "void __thiscall corrected(IVP_Impact_System *self, IVP_Core *core)",
     "Retail private Impact System Mindist invalidation for one Core; decorated pointer and RET 4 establish the ABI."),
    (0x10024740, "?get_world_direction_second_friction@IVP_Impact_Solver@@QAEXPAVIVP_Contact_Point@@@Z",
     "void __thiscall corrected(IVP_Impact_Solver *self, IVP_Contact_Point *contact)",
     "Retail Impact Solver secondary-friction direction calculation; decorated Contact Point pointer and RET 4 establish the ABI."),
    (0x10024AA0, "?recalc_all_affected_cores@IVP_Impact_System@@AAEXXZ",
     "void __thiscall corrected(IVP_Impact_System *self)",
     "Retail private Impact System affected-Core recomputation; parameterless instance decoration establishes the ABI."),
    (0x10024C60, "?ivp_core_get_surface_speed_os@@YAXPAVIVP_Core@@PAVIVP_Real_Object@@PBVIVP_U_Float_Point@@PAV3@@Z",
     "void __cdecl corrected(IVP_Core *core, IVP_Real_Object *object, const IVP_U_Float_Point *position_os, IVP_U_Float_Point *speed_os_out)",
     "Retail object-space Core surface-speed helper; decorated cdecl Core/Object/const-input/output sequence establishes the ABI."),
    (0x1002A9C0, "?get_value@IVP_3D_Solver_PF_COLL@@UAENPAVIVP_U_Matrix@@0@Z",
     "double __thiscall corrected(IVP_3D_Solver_PF_COLL *self, IVP_U_Matrix *matrix_a, IVP_U_Matrix *matrix_b)",
     "Retail PF collision value virtual; decorated double return, repeated Matrix pointers and RET 8 establish the ABI."),
    (0x1002AAE0, "?get_value@IVP_3D_Solver_VEC_PARALLEL_AREA@@UAENPAVIVP_U_Matrix@@0@Z",
     "double __thiscall corrected(IVP_3D_Solver_VEC_PARALLEL_AREA *self, IVP_U_Matrix *matrix_a, IVP_U_Matrix *matrix_b)",
     "Retail vector-parallel-area value virtual; decorated double return, repeated Matrix pointers and RET 8 establish the ABI."),
    (0x1002AB30, "?get_value@IVP_3D_Solver_DISTANCE_OF_TWO_POINTS@@UAENPAVIVP_U_Matrix@@0@Z",
     "double __thiscall corrected(IVP_3D_Solver_DISTANCE_OF_TWO_POINTS *self, IVP_U_Matrix *matrix_a, IVP_U_Matrix *matrix_b)",
     "Retail two-point-distance value virtual; decorated double return, repeated Matrix pointers and RET 8 establish the ABI."),
    (0x1002ABC0, "?get_value@IVP_3D_Solver_S_VALS@@UAENPAVIVP_U_Matrix@@0@Z",
     "double __thiscall corrected(IVP_3D_Solver_S_VALS *self, IVP_U_Matrix *matrix_a, IVP_U_Matrix *matrix_b)",
     "Retail edge-coordinate value virtual; decorated double return, repeated Matrix pointers and RET 8 establish the ABI."),
    (0x1002AC40, "?get_value@IVP_3D_Solver_PK_KK@@UAENPAVIVP_U_Matrix@@0@Z",
     "double __thiscall corrected(IVP_3D_Solver_PK_KK *self, IVP_U_Matrix *matrix_a, IVP_U_Matrix *matrix_b)",
     "Retail point-edge/edge-edge value virtual; decorated double return, repeated Matrix pointers and RET 8 establish the ABI."),
    (0x1002AD90, "?get_value@IVP_3D_Solver_PK_COLL@@UAENPAVIVP_U_Matrix@@0@Z",
     "double __thiscall corrected(IVP_3D_Solver_PK_COLL *self, IVP_U_Matrix *matrix_a, IVP_U_Matrix *matrix_b)",
     "Retail point-edge collision value virtual; decorated double return, repeated Matrix pointers and RET 8 establish the ABI."),
    (0x1002AE80, "?get_value@IVP_3D_Solver_PF_NPF@@UAENPAVIVP_U_Matrix@@0@Z",
     "double __thiscall corrected(IVP_3D_Solver_PF_NPF *self, IVP_U_Matrix *matrix_a, IVP_U_Matrix *matrix_b)",
     "Retail point-face/negative-plane value virtual; decorated double return, repeated Matrix pointers and RET 8 establish the ABI."),
    (0x1002B530, "?get_value@IVP_3D_Solver_KK_COLL@@UAENPAVIVP_U_Matrix@@0@Z",
     "double __thiscall corrected(IVP_3D_Solver_KK_COLL *self, IVP_U_Matrix *matrix_k, IVP_U_Matrix *matrix_l)",
     "Retail edge-edge collision value virtual; decorated double return, repeated Matrix pointers and RET 8 establish the ABI."),
    (0x1002B620, "?get_value@IVP_3D_Solver_KK_PARALLEL@@UAENPAVIVP_U_Matrix@@0@Z",
     "double __thiscall corrected(IVP_3D_Solver_KK_PARALLEL *self, IVP_U_Matrix *matrix_k, IVP_U_Matrix *matrix_l)",
     "Retail parallel-edge value virtual; decorated double return, repeated Matrix pointers and RET 8 establish the ABI."),
    (0x1002AED0, "?calc_next_event_PF@IVP_Mindist_Event_Solver@@IAEXPBVIVP_Compact_Edge@@0PAVIVP_Cache_Ledge_Point@@1@Z",
     "void __thiscall corrected(IVP_Mindist_Event_Solver *self, const IVP_Compact_Edge *point_edge, const IVP_Compact_Edge *face_edge, IVP_Cache_Ledge_Point *point_cache, IVP_Cache_Ledge_Point *face_cache)",
     "Retail protected point-face event solver; decorated repeated Edge/cache types and RET 0x10 establish the ABI."),
    (0x1002B300, "?p_init@IVP_U_Matrix_Cache@@AAEXPAVIVP_Cache_Object@@@Z",
     "void __thiscall corrected(IVP_U_Matrix_Cache *self, IVP_Cache_Object *cache_object)",
     "Retail private Matrix Cache initializer; decorated Cache Object pointer and RET 4 establish the ABI."),
    (0x10037790, "?calc_nullstelle@IVP_3D_Solver@@IAE?AVIVP_Time@@V2@0NNNPAVIVP_Real_Object@@1@Z",
     "IVP_Time __thiscall corrected(IVP_3D_Solver *self, IVP_Time t0, IVP_Time t1, double value, double v0, double v1, IVP_Real_Object *solver_a, IVP_Real_Object *solver_b)",
     "Retail protected continuous-collision root refinement. The decorated record return and RET 0x34 establish two by-value IVP_Time arguments, three doubles, two object pointers, and the hidden IVP_Time result pointer."),
    (0x10037910, "?find_first_t_for_value_max_dev@IVP_3D_Solver@@QAE?AW4IVP_BOOL@@NVIVP_Time@@0HPAVIVP_U_Matrix_Cache@@1PANPAV3@@Z",
     "IVP_BOOL __thiscall corrected(IVP_3D_Solver *self, double value, IVP_Time t_now, IVP_Time t_max, int t_now_cache_index, IVP_U_Matrix_Cache *cache_a, IVP_U_Matrix_Cache *cache_b, double *value_at_now, IVP_Time *time_out)",
     "Retail maximum-deviation time-of-impact search. Decorated values, RET 0x2C, Matrix Cache field accesses and the slot-zero callback jointly establish its member ABI."),
    (0x10037B30, "?find_first_t_for_value_coll@IVP_3D_Solver@@QAE?AW4IVP_BOOL@@NNVIVP_Time@@0PAVIVP_U_Matrix_Cache@@1PANPAV3@@Z",
     "IVP_BOOL __thiscall corrected(IVP_3D_Solver *self, double value, double absolute_min_value, IVP_Time t_now, IVP_Time t_max, IVP_U_Matrix_Cache *cache_a, IVP_U_Matrix_Cache *cache_b, double *value_at_now, IVP_Time *time_out)",
     "Retail collision-aware time search. Decorated values, RET 0x30 and its call to the maximum-deviation path establish the exact parameter order and member ABI."),
    (0x1002B370, "?calc_next_event_BF@IVP_Mindist_Event_Solver@@IAEXPBVIVP_Compact_Edge@@PAVIVP_Cache_Object@@PAVIVP_Cache_Ledge_Point@@@Z",
     "void __thiscall corrected(IVP_Mindist_Event_Solver *self, const IVP_Compact_Edge *face_edge, IVP_Cache_Object *ball_cache, IVP_Cache_Ledge_Point *face_cache)",
     "Retail protected ball-face event solver; decorated Edge/Object-cache/Ledge-cache order and RET 0x0C establish the ABI."),
    (0x1002B690, "?calc_next_event_KK@IVP_Mindist_Event_Solver@@IAEXPBVIVP_Compact_Edge@@0PAVIVP_Cache_Ledge_Point@@1@Z",
     "void __thiscall corrected(IVP_Mindist_Event_Solver *self, const IVP_Compact_Edge *edge0, const IVP_Compact_Edge *edge1, IVP_Cache_Ledge_Point *cache0, IVP_Cache_Ledge_Point *cache1)",
     "Retail protected edge-edge event solver; decorated repeated Edge/cache types and RET 0x10 establish the ABI."),
    (0x1002BCF0, "?calc_next_event_PP@IVP_Mindist_Event_Solver@@IAEXPBVIVP_Compact_Edge@@0PAVIVP_Cache_Ledge_Point@@1@Z",
     "void __thiscall corrected(IVP_Mindist_Event_Solver *self, const IVP_Compact_Edge *point0, const IVP_Compact_Edge *point1, IVP_Cache_Ledge_Point *cache0, IVP_Cache_Ledge_Point *cache1)",
     "Retail protected point-point event solver; decorated repeated Edge/cache types and RET 0x10 establish the ABI."),
    (0x1002C260, "?calc_next_event_BP@IVP_Mindist_Event_Solver@@IAEXPAVIVP_Ball@@PBVIVP_Compact_Edge@@PAVIVP_Cache_Object@@PAVIVP_Cache_Ledge_Point@@@Z",
     "void __thiscall corrected(IVP_Mindist_Event_Solver *self, IVP_Ball *ball, const IVP_Compact_Edge *point_edge, IVP_Cache_Object *ball_cache, IVP_Cache_Ledge_Point *point_cache)",
     "Retail protected ball-point event solver; decorated Ball/Edge/cache sequence and RET 0x10 establish the ABI."),
    (0x1002C670, "?calc_next_event_BB@IVP_Mindist_Event_Solver@@IAEXPAVIVP_Cache_Object@@0@Z",
     "void __thiscall corrected(IVP_Mindist_Event_Solver *self, IVP_Cache_Object *cache0, IVP_Cache_Object *cache1)",
     "Retail protected ball-ball event solver; decorated repeated Cache Object pointers and RET 8 establish the ABI."),
    (0x1002C820, "?calc_next_event_PK@IVP_Mindist_Event_Solver@@IAEXPBVIVP_Compact_Edge@@0PAVIVP_Cache_Ledge_Point@@1@Z",
     "void __thiscall corrected(IVP_Mindist_Event_Solver *self, const IVP_Compact_Edge *point_edge, const IVP_Compact_Edge *line_edge, IVP_Cache_Ledge_Point *point_cache, IVP_Cache_Ledge_Point *line_cache)",
     "Retail protected point-edge event solver; decorated repeated Edge/cache types and RET 0x10 establish the ABI."),
    (0x1002D000, "?calc_next_event_BK@IVP_Mindist_Event_Solver@@IAEXPAVIVP_Ball@@PBVIVP_Compact_Edge@@PAVIVP_Cache_Object@@PAVIVP_Cache_Ledge_Point@@@Z",
     "void __thiscall corrected(IVP_Mindist_Event_Solver *self, IVP_Ball *ball, const IVP_Compact_Edge *line_edge, IVP_Cache_Object *ball_cache, IVP_Cache_Ledge_Point *line_cache)",
     "Retail protected ball-edge event solver; decorated Ball/Edge/cache sequence and RET 0x10 establish the ABI."),
    (0x1002D4D0, "?next_event_B_POLY@IVP_Mindist_Event_Solver@@KAXPAV1@@Z",
     "void __cdecl corrected(IVP_Mindist_Event_Solver *solver)",
     "Retail protected static ball-polygon event dispatcher; decorated static signature fixes one solver pointer."),
    (0x1002D5F0, "?next_event_BB@IVP_Mindist_Event_Solver@@KAXPAV1@@Z",
     "void __cdecl corrected(IVP_Mindist_Event_Solver *solver)",
     "Retail protected static ball-ball event dispatcher; decorated static signature fixes one solver pointer."),
    (0x1002D6C0, "?next_event_default_poly_poly@IVP_Mindist_Event_Solver@@KAXPAV1@@Z",
     "void __cdecl corrected(IVP_Mindist_Event_Solver *solver)",
     "Retail protected static default polygon event dispatcher; decorated static signature fixes one solver pointer."),
    (0x1002D850, "?next_event_illegal@IVP_Mindist_Event_Solver@@KAXPAV1@@Z",
     "void __cdecl corrected(IVP_Mindist_Event_Solver *solver)",
     "Retail protected static illegal event combination handler; decorated static signature fixes one solver pointer."),
    (0x1002DC50, "??_GIVP_OV_Element@@UAEPAXI@Z",
     "void *__thiscall corrected(IVP_OV_Element *self, unsigned int flags)",
     "Retail OV Element scalar deleting destructor in final-vtable slot 4. It enters the complete destructor at RVA 0x2DC70, conditionally calls the retail operator-delete thunk for flags bit zero, returns self and uses RET 4."),
    (0x1002DC70, "??1IVP_OV_Element@@UAE@XZ",
     "void __thiscall corrected(IVP_OV_Element *self)",
     "[BML direct complete destructor] Retail OV Element complete destructor. The public API routes this body directly; it removes the hull-manager event, notifies every environment collision delegator, removes the element from the OV tree and destroys the embedded collision fvector without freeing the outer object."),
    (0x1002DD00, "?add_to_hull_manager@IVP_OV_Element@@QAEXPAVIVP_Hull_Manager@@N@Z",
     "void __thiscall corrected(IVP_OV_Element *self, IVP_Hull_Manager *manager, double hull_time)",
     "Binary-attributed retail OV Element scheduler. The two Mindist Manager callsites, Hull Manager field use, 8-byte x87 argument and RET 0x0C establish one manager pointer followed by IVP_DOUBLE hull time; it inserts once and updates thereafter."),
    (0x1002DDA0, "?get_type@IVP_OV_Element@@MAE?AW4IVP_HULL_ELEM_TYPE@@XZ",
     "IVP_HULL_ELEM_TYPE __thiscall corrected(IVP_OV_Element *self)",
     "Retail protected OV Element hull-type virtual; decorated enum return and parameterless RET establish the ABI."),
    (0x1002DDB0, "?hull_manager_is_going_to_be_deleted_event@IVP_OV_Element@@MAEXPAVIVP_Hull_Manager@@@Z",
     "void __thiscall corrected(IVP_OV_Element *self, IVP_Hull_Manager *manager)",
     "Binary-attributed retail OV Element hull-manager teardown callback in final-vtable slot 2. It dispatches the scalar deleting destructor for self; RET 4 establishes the single manager-pointer argument."),
    (0x1002DDC0, "?hull_limit_exceeded_event@IVP_OV_Element@@MAEXPAVIVP_Hull_Manager@@M@Z",
     "void __thiscall corrected(IVP_OV_Element *self, IVP_Hull_Manager *manager, float hull_time)",
     "Retail protected OV Element hull-limit callback; decorated manager pointer, float time and RET 8 establish the ABI."),
    (0x1002DDE0, "?add_oo_collision@IVP_OV_Element@@QAEXPAVIVP_Collision@@@Z",
     "void __thiscall corrected(IVP_OV_Element *self, IVP_Collision *collision)",
     "Retail OV Element object-object collision insertion; decorated pointer and RET 4 establish the ABI."),
    (0x1002DE30, "?remove_oo_collision@IVP_OV_Element@@QAEXPAVIVP_Collision@@@Z",
     "void __thiscall corrected(IVP_OV_Element *self, IVP_Collision *collision)",
     "Retail OV Element object-object collision removal; decorated pointer and RET 4 establish the ABI."),
    (0x1002E150, "?log_base2@IVP_OV_Tree_Manager@@ABEHN@Z",
     "int __thiscall corrected(const IVP_OV_Tree_Manager *self, double value)",
     "Retail private const OV Tree logarithm helper; decorated double argument, integer return and RET 8 establish the ABI."),
    (0x1002E180, "?box_contains_box@IVP_OV_Tree_Manager@@ABE?AW4IVP_BOOL@@PBUIVP_OV_Node_Data@@PBVIVP_OV_Node@@H@Z",
     "IVP_BOOL __thiscall corrected(const IVP_OV_Tree_Manager *self, const IVP_OV_Node_Data *master_data, const IVP_OV_Node *sub_node, int raster_level_difference)",
     "Retail private const OV containment predicate; decorated struct/node/int parameters and RET 0x0C establish the ABI."),
    (0x1002E4B0, "?box_overlaps_with_box@IVP_OV_Tree_Manager@@ABE?AW4IVP_BOOL@@PBVIVP_OV_Node@@0H@Z",
     "IVP_BOOL __thiscall corrected(const IVP_OV_Tree_Manager *self, const IVP_OV_Node *large_node, const IVP_OV_Node *small_node, int raster_level_difference)",
     "Retail private const OV overlap predicate; decorated repeated const Node pointers, integer level and RET 0x0C establish the ABI."),
    (0x1002E560, "?find_smallest_box@IVP_OV_Tree_Manager@@ABEPAVIVP_OV_Node@@PBV2@0@Z",
     "IVP_OV_Node *__thiscall corrected(const IVP_OV_Tree_Manager *self, const IVP_OV_Node *master_node, const IVP_OV_Node *sub_node)",
     "Retail private const OV tree descent; decorated mutable Node return, two const Node inputs and RET 8 establish the ABI."),
    (0x1002E5B0, "?connect_boxes@IVP_OV_Tree_Manager@@AAEXPAVIVP_OV_Node@@0@Z",
     "void __thiscall corrected(IVP_OV_Tree_Manager *self, IVP_OV_Node *node, IVP_OV_Node *new_node)",
     "Retail private OV node connection; decorated repeated mutable Node pointers and RET 8 establish the ABI."),
    (0x1002E7B0, "?expand_tree@IVP_OV_Tree_Manager@@AAEXPBVIVP_OV_Node@@@Z",
     "void __thiscall corrected(IVP_OV_Tree_Manager *self, const IVP_OV_Node *new_node)",
     "Retail private OV tree expansion; decorated const Node pointer and RET 4 establish the ABI."),
    (0x1002E9B0, "?collect_subbox_collision_partners@IVP_OV_Tree_Manager@@AAEXPBVIVP_OV_Element@@PBVIVP_OV_Node@@@Z",
     "void __thiscall corrected(IVP_OV_Tree_Manager *self, const IVP_OV_Element *center_element, const IVP_OV_Node *node)",
     "Retail private recursive OV collision-partner collection; decorated const Element/Node pointers and RET 8 establish the ABI."),
    (0x1002EA70, "?collect_collision_partners@IVP_OV_Tree_Manager@@AAEXPBVIVP_OV_Element@@PBVIVP_OV_Node@@1@Z",
     "void __thiscall corrected(IVP_OV_Tree_Manager *self, const IVP_OV_Element *center_element, const IVP_OV_Node *master_node, const IVP_OV_Node *new_node)",
     "Retail private OV collision-partner traversal; decorated const Element and repeated Node pointers plus RET 0x0C establish the ABI."),
    (0x1002EEC0, "?remove_ov_element@IVP_OV_Tree_Manager@@QAEXPAVIVP_OV_Element@@@Z",
     "void __thiscall corrected(IVP_OV_Tree_Manager *self, IVP_OV_Element *element)",
     "Retail OV Tree element removal; decorated mutable Element pointer and RET 4 establish the ABI."),
    (0x1002F3B0, "?object_is_removed_from_collision_detection@IVP_Collision_Delegator_Root_Mindist@@UAEXPAVIVP_Real_Object@@@Z",
     "void __thiscall corrected(IVP_Collision_Delegator_Root_Mindist *self, IVP_Real_Object *object)",
     "Retail Root Mindist delegator object-removal virtual; decorated Real Object pointer and RET 4 establish the ABI."),
    (0x1002F3E0, "?environment_is_going_to_be_deleted_event@IVP_Collision_Delegator_Root_Mindist@@UAEXPAVIVP_Environment@@@Z",
     "void __thiscall corrected(IVP_Collision_Delegator_Root_Mindist *self, IVP_Environment *environment)",
     "Retail Root Mindist delegator environment-deletion virtual; decorated Environment pointer and RET 4 establish the ABI."),
    (0x1002F3F0, "?collision_is_going_to_be_deleted_event@IVP_Collision_Delegator_Root_Mindist@@UAEXPAVIVP_Collision@@@Z",
     "void __thiscall corrected(IVP_Collision_Delegator_Root_Mindist *self, IVP_Collision *collision)",
     "Retail Root Mindist delegator collision-deletion virtual; decorated Collision pointer and RET 4 establish the ABI."),
    (0x1002F430, "?delegate_collisions_for_object@IVP_Collision_Delegator_Root_Mindist@@UAEPAVIVP_Collision@@PAVIVP_Real_Object@@0@Z",
     "IVP_Collision *__thiscall corrected(IVP_Collision_Delegator_Root_Mindist *self, IVP_Real_Object *object0, IVP_Real_Object *object1)",
     "Retail Root Mindist collision factory virtual; decorated Collision return, repeated Real Object inputs and RET 8 establish the ABI."),
    (0x1002FE50, "?get_single_convex@IVP_SurfaceManager_Ball@@UBEPBVIVP_Compact_Ledge@@XZ",
     "const IVP_Compact_Ledge *__thiscall corrected(const IVP_SurfaceManager_Ball *self)",
     "Retail const Ball Surface Manager single-ledge virtual; decorated const Ledge return and plain RET establish the ABI."),
    (0x1002FE60, "?get_mass_center@IVP_SurfaceManager_Ball@@UBEXPAVIVP_U_Float_Point@@@Z",
     "void __thiscall corrected(const IVP_SurfaceManager_Ball *self, IVP_U_Float_Point *center_out)",
     "Retail const Ball Surface Manager mass-center virtual; decorated Float Point output and RET 4 establish the ABI."),
    (0x1002FE80, "?get_radius_and_radius_dev_to_given_center@IVP_SurfaceManager_Ball@@UBEXPBVIVP_U_Float_Point@@PAM1@Z",
     "void __thiscall corrected(const IVP_SurfaceManager_Ball *self, const IVP_U_Float_Point *center, float *radius_out, float *deviation_out)",
     "Retail const Ball Surface Manager radius query; decorated const center and repeated float outputs plus RET 0x0C establish the ABI."),
    (0x1002FF00, "?get_all_terminal_ledges@IVP_SurfaceManager_Ball@@UAEXPAV?$IVP_U_BigVector@VIVP_Compact_Ledge@@@@@Z",
     "void __thiscall corrected(IVP_SurfaceManager_Ball *self, void *ledges_out)",
     "Retail Ball Surface Manager ledge enumeration virtual. ledges_out is specifically IVP_U_BigVector<IVP_Compact_Ledge>*; IDA's legacy parser cannot spell the template type, while RET 4 fixes the pointer ABI."),
    (0x1002FF40, "?insert_all_ledges_hitting_ray@IVP_SurfaceManager_Ball@@UAEXPAVIVP_Ray_Solver@@PAVIVP_Real_Object@@@Z",
     "void __thiscall corrected(IVP_SurfaceManager_Ball *self, IVP_Ray_Solver *ray_solver, IVP_Real_Object *object)",
     "Retail Ball Surface Manager ray-insertion virtual; decorated Ray Solver/Object pointers and RET 8 establish the ABI."),
    (0x10030540, "?do_impact@IVP_Mindist_Recursive@@UAEXXZ",
     "void __thiscall corrected(IVP_Mindist_Recursive *self)",
     "Retail Recursive Mindist primary-vtable slot-7 impact override. The parameterless body uses the complete-object ECX and falls back to IVP_Mindist::do_impact when recursion is terminal."),
    (0x10030710, "?exact_mindist_went_invalid@IVP_Mindist_Recursive@@UAEXPAVIVP_Mindist_Manager@@@Z",
     "void __thiscall corrected(IVP_Mindist_Recursive *self, IVP_Mindist_Manager *manager)",
     "Retail Recursive Mindist primary-vtable slot-6 invalidation override. The manager pointer and RET 4 establish the ABI."),
    (0x100307F0, "?delete_all_children@IVP_Mindist_Recursive@@AAEXXZ",
     "void __thiscall corrected(IVP_Mindist_Recursive *self)",
     "Retail private Recursive Mindist child teardown. It iterates the collision FVector at +0x90 and invokes each child's deleting-destructor slot."),
    (0x100309C0, "?collision_is_going_to_be_deleted_event@IVP_Mindist_Recursive@@EAEXPAVIVP_Collision@@@Z",
     "void __thiscall corrected(IVP_Collision_Delegator *adjusted_self, IVP_Collision *collision)",
     "Retail private Recursive Mindist secondary-base callback. ECX points at the IVP_Collision_Delegator subobject at complete object +0x88; its +0x08/+0x0A/+0x0C accesses are the collision FVector at complete +0x90/+0x92/+0x94. RET 4 establishes the Collision pointer ABI."),
    (0x10030A30, "?recheck_recursive_childs@IVP_Mindist_Recursive@@AAEXN@Z",
     "void __thiscall corrected(IVP_Mindist_Recursive *self, double hull_distance_intra_object)",
     "Retail private Recursive Mindist child recreation helper. Decorated IVP_DOUBLE N, qword stack loads and RET 8 establish the binary64 argument."),
    (0x10030AF0, "?invalid_mindist_went_exact@IVP_Mindist_Recursive@@AAEXXZ",
     "void __thiscall corrected(IVP_Mindist_Recursive *self)",
     "Retail private Recursive Mindist invalid-to-exact transition; parameterless instance decoration establishes the ABI."),
    (0x10030B20, "?rec_hull_limit_exceeded_event@IVP_Mindist_Recursive@@QAEXXZ",
     "void __thiscall corrected(IVP_Mindist_Recursive *self)",
     "Retail Recursive Mindist hull-limit recursion entry; parameterless public instance decoration establishes the ABI."),
    (0x10030C60, "?p_minimize_PF@IVP_Mindist_Minimize_Solver@@IAE?AW4IVP_MRC_TYPE@@PBVIVP_Compact_Edge@@0PAVIVP_Cache_Ledge_Point@@1@Z",
     "IVP_MRC_TYPE __thiscall corrected(IVP_Mindist_Minimize_Solver *self, const IVP_Compact_Edge *point_edge, const IVP_Compact_Edge *face_edge, IVP_Cache_Ledge_Point *point_cache, IVP_Cache_Ledge_Point *face_cache)",
     "Retail protected point-face minimizer; decorated repeated Edge/cache types, enum return and RET 0x10 establish the ABI."),
    (0x10030D50, "?p_minimize_BF@IVP_Mindist_Minimize_Solver@@IAE?AW4IVP_MRC_TYPE@@PAVIVP_Cache_Ball@@PBVIVP_Compact_Edge@@PAVIVP_Cache_Ledge_Point@@@Z",
     "IVP_MRC_TYPE __thiscall corrected(IVP_Mindist_Minimize_Solver *self, IVP_Cache_Ball *ball_cache, const IVP_Compact_Edge *face_edge, IVP_Cache_Ledge_Point *face_cache)",
     "Retail protected ball-face minimizer; decorated cache/Edge/cache order, enum return and RET 0x0C establish the ABI."),
    (0x10030F80, "?p_minimize_Leave_PF@IVP_Mindist_Minimize_Solver@@IAE?AW4IVP_MRC_TYPE@@PBVIVP_Compact_Edge@@PBVIVP_U_Point@@0PAVIVP_Cache_Ledge_Point@@2@Z",
     "IVP_MRC_TYPE __thiscall corrected(IVP_Mindist_Minimize_Solver *self, const IVP_Compact_Edge *point_edge, const IVP_U_Point *point_in_face_space, const IVP_Compact_Edge *face_edge, IVP_Cache_Ledge_Point *point_cache, IVP_Cache_Ledge_Point *face_cache)",
     "Retail protected point-leaves-face minimizer; decorated five-pointer sequence, enum return and RET 0x14 establish the ABI."),
    (0x10031440, "?p_minimize_KK@IVP_Mindist_Minimize_Solver@@IAE?AW4IVP_MRC_TYPE@@PBVIVP_Compact_Edge@@0PAVIVP_Cache_Ledge_Point@@1@Z",
     "IVP_MRC_TYPE __thiscall corrected(IVP_Mindist_Minimize_Solver *self, const IVP_Compact_Edge *edge0, const IVP_Compact_Edge *edge1, IVP_Cache_Ledge_Point *cache0, IVP_Cache_Ledge_Point *cache1)",
     "Retail protected edge-edge minimizer; decorated repeated Edge/cache types, enum return and RET 0x10 establish the ABI."),
    (0x100317A0, "?p_minimize_Leave_KK@IVP_Mindist_Minimize_Solver@@IAE?AW4IVP_MRC_TYPE@@PBVIVP_Compact_Edge@@0ABVIVP_KK_Input@@PBVIVP_Unscaled_KK_Result@@PAVIVP_Cache_Ledge_Point@@3@Z",
     "IVP_MRC_TYPE __thiscall corrected(IVP_Mindist_Minimize_Solver *self, const IVP_Compact_Edge *edge0, const IVP_Compact_Edge *edge1, const IVP_KK_Input *input, const IVP_Unscaled_KK_Result *result, IVP_Cache_Ledge_Point *cache0, IVP_Cache_Ledge_Point *cache1)",
     "Retail protected edge-edge exit minimizer; the const-reference input is pointer ABI on x86, and the decorated six-pointer sequence plus RET 0x18 establish the signature."),
    (0x10032050, "?p_minimize_BP@IVP_Mindist_Minimize_Solver@@IAE?AW4IVP_MRC_TYPE@@PAVIVP_Cache_Ball@@PBVIVP_Compact_Edge@@PAVIVP_Cache_Ledge_Point@@@Z",
     "IVP_MRC_TYPE __thiscall corrected(IVP_Mindist_Minimize_Solver *self, IVP_Cache_Ball *ball_cache, const IVP_Compact_Edge *point_edge, IVP_Cache_Ledge_Point *point_cache)",
     "Retail protected ball-point minimizer; decorated cache/Edge/cache order, enum return and RET 0x0C establish the ABI."),
    (0x10032350, "?p_minimize_PP@IVP_Mindist_Minimize_Solver@@IAE?AW4IVP_MRC_TYPE@@PBVIVP_Compact_Edge@@0PAVIVP_Cache_Ledge_Point@@1@Z",
     "IVP_MRC_TYPE __thiscall corrected(IVP_Mindist_Minimize_Solver *self, const IVP_Compact_Edge *point0, const IVP_Compact_Edge *point1, IVP_Cache_Ledge_Point *cache0, IVP_Cache_Ledge_Point *cache1)",
     "Retail protected point-point minimizer; decorated repeated Edge/cache types, enum return and RET 0x10 establish the ABI."),
    (0x10032860, "?p_minimize_BK@IVP_Mindist_Minimize_Solver@@IAE?AW4IVP_MRC_TYPE@@PAVIVP_Cache_Ball@@PBVIVP_Compact_Edge@@PAVIVP_Cache_Ledge_Point@@@Z",
     "IVP_MRC_TYPE __thiscall corrected(IVP_Mindist_Minimize_Solver *self, IVP_Cache_Ball *ball_cache, const IVP_Compact_Edge *line_edge, IVP_Cache_Ledge_Point *line_cache)",
     "Retail protected ball-edge minimizer; decorated cache/Edge/cache order, enum return and RET 0x0C establish the ABI."),
    (0x10032910, "?p_minimize_Leave_BK@IVP_Mindist_Minimize_Solver@@IAE?AW4IVP_MRC_TYPE@@PAVIVP_Cache_Ball@@PBVIVP_Compact_Edge@@PAVIVP_Cache_Ledge_Point@@@Z",
     "IVP_MRC_TYPE __thiscall corrected(IVP_Mindist_Minimize_Solver *self, IVP_Cache_Ball *ball_cache, const IVP_Compact_Edge *line_edge, IVP_Cache_Ledge_Point *line_cache)",
     "Retail protected ball-leaves-edge minimizer; decorated cache/Edge/cache order, enum return and RET 0x0C establish the ABI."),
    (0x10032DA0, "?p_minimize_PK@IVP_Mindist_Minimize_Solver@@IAE?AW4IVP_MRC_TYPE@@PBVIVP_Compact_Edge@@0PAVIVP_Cache_Ledge_Point@@1@Z",
     "IVP_MRC_TYPE __thiscall corrected(IVP_Mindist_Minimize_Solver *self, const IVP_Compact_Edge *point_edge, const IVP_Compact_Edge *line_edge, IVP_Cache_Ledge_Point *point_cache, IVP_Cache_Ledge_Point *line_cache)",
     "Retail protected point-edge minimizer; decorated repeated Edge/cache types, enum return and RET 0x10 establish the ABI."),
    (0x10032E60, "?p_minimize_Leave_PK@IVP_Mindist_Minimize_Solver@@IAE?AW4IVP_MRC_TYPE@@PBVIVP_Compact_Edge@@0PAVIVP_Cache_Ledge_Point@@1@Z",
     "IVP_MRC_TYPE __thiscall corrected(IVP_Mindist_Minimize_Solver *self, const IVP_Compact_Edge *point_edge, const IVP_Compact_Edge *line_edge, IVP_Cache_Ledge_Point *point_cache, IVP_Cache_Ledge_Point *line_cache)",
     "Retail protected point-leaves-edge minimizer; decorated repeated Edge/cache types, enum return and RET 0x10 establish the ABI."),
    (0x1001DEE0, "?rehash@IVP_VHash_Store@@AAEXH@Z",
     "void __thiscall corrected(IVP_VHash_Store *self, int new_size)",
     "Retail private variable-hash store rebuild; source declaration, decorated integer argument and RET 4 establish the ABI."),
    (0x10024F80, "?compute_buoyancy_values_for_one_ball@IVP_Buoyancy_Solver@@AAEXABHABM1PBVIVP_U_Float_Hesse@@PBVIVP_U_Float_Point@@@Z",
     "void __thiscall corrected(IVP_Buoyancy_Solver *self, const int *decision, const float *distance, const float *radius, const IVP_U_Float_Hesse *surface_os, const IVP_U_Float_Point *center_os)",
     "Retail private ball buoyancy calculation. The three source const references are pointer ABI on x86; source definition, decorated backreferences and RET 0x14 establish the signature."),
    (0x100250E0, "?compute_dampening_values_for_one_ball@IVP_Buoyancy_Solver@@AAEXABHABM1PBVIVP_U_Float_Point@@2PBVIVP_U_Float_Hesse@@322@Z",
     "void __thiscall corrected(IVP_Buoyancy_Solver *self, const int *decision, const float *distance, const float *radius, const IVP_U_Float_Point *center_os, const IVP_U_Float_Point *relative_speed_os, const IVP_U_Float_Hesse *speed_plane_os, const IVP_U_Float_Hesse *surface_os, const IVP_U_Float_Point *point0_os, const IVP_U_Float_Point *point1_os)",
     "Retail private ball damping calculation. Source const references are pointer ABI on x86; source parameter order, decorated type backreferences and RET 0x24 establish all nine explicit arguments."),
    (0x10026F10, "?compute_values_for_one_ball@IVP_Buoyancy_Solver@@AAEXPBVIVP_Real_Object@@PBVIVP_U_Float_Hesse@@PBVIVP_U_Float_Point@@@Z",
     "void __thiscall corrected(IVP_Buoyancy_Solver *self, const IVP_Real_Object *object, const IVP_U_Float_Hesse *surface_os, const IVP_U_Float_Point *relative_speed_os)",
     "Retail private per-ball buoyancy dispatcher; source definition, decorated three const pointers and RET 0x0C establish the ABI."),
    (0x100270C0, "?compute_disection_points_with_ball@IVP_Buoyancy_Solver@@AAEHPBVIVP_U_Float_Hesse@@0PBVIVP_U_Float_Point@@ABMPAV3@3@Z",
     "int __thiscall corrected(IVP_Buoyancy_Solver *self, const IVP_U_Float_Hesse *plane0_os, const IVP_U_Float_Hesse *plane1_os, const IVP_U_Float_Point *center_os, const float *radius, IVP_U_Float_Point *point0_os, IVP_U_Float_Point *point1_os)",
     "Retail private plane-intersection/ball dissection helper. The source radius reference is pointer ABI; decoration, integer result and RET 0x18 establish the signature."),
    (0x10027430, "?compute_values_for_one_triangle@IVP_Buoyancy_Solver@@AAEXPAVIVP_Real_Object@@PBVIVP_Compact_Triangle@@PBVIVP_U_Float_Hesse@@PBVIVP_U_Float_Point@@PBVIVP_Compact_Ledge@@@Z",
     "void __thiscall corrected(IVP_Buoyancy_Solver *self, IVP_Real_Object *object, const IVP_Compact_Triangle *triangle, const IVP_U_Float_Hesse *surface_os, const IVP_U_Float_Point *surface_point, const IVP_Compact_Ledge *ledge)",
     "Retail private per-triangle buoyancy calculation; source definition, decorated five-pointer order and RET 0x14 establish the ABI."),
    (0x100275D0, "?compute_volumes_and_centers_for_one_pyramid@IVP_Buoyancy_Solver@@AAEXPAVIVP_Real_Object@@QAPBVIVP_U_Float_Point@@QBMABH33PBV3@@Z",
     "void __thiscall corrected(IVP_Buoyancy_Solver *self, IVP_Real_Object *object, const IVP_U_Float_Point **triangle_points, const float *distances, const int *decision, const int *positive_index, const int *negative_index, const IVP_U_Float_Point *surface_point)",
     "Retail inlined private pyramid-volume helper retained out of line. Source arrays/references decay to pointers; decorated backreferences and RET 0x1C establish all seven explicit parameters."),
    (0x10027BC0, "?compute_rotation_and_translation_values_for_one_triangle@IVP_Buoyancy_Solver@@AAEXPAVIVP_Real_Object@@PBVIVP_Compact_Triangle@@QAPBVIVP_U_Float_Point@@PBVIVP_Compact_Ledge@@QBMABH55@Z",
     "void __thiscall corrected(IVP_Buoyancy_Solver *self, IVP_Real_Object *object, const IVP_Compact_Triangle *triangle, const IVP_U_Float_Point **triangle_points, const IVP_Compact_Ledge *ledge, const float *distances, const int *decision, const int *positive_index, const int *negative_index)",
     "Retail inlined private triangle damping helper retained out of line. Source arrays/references decay to pointers; decorated backreferences and RET 0x20 establish all eight explicit parameters."),
    (0x100281D0, "?compute_values_for_one_ledge@IVP_Buoyancy_Solver@@AAEXPAVIVP_Real_Object@@PBVIVP_Compact_Ledge@@PBVIVP_U_Float_Hesse@@PBVIVP_U_Float_Point@@@Z",
     "void __thiscall corrected(IVP_Buoyancy_Solver *self, IVP_Real_Object *object, const IVP_Compact_Ledge *ledge, const IVP_U_Float_Hesse *surface_os, const IVP_U_Float_Point *surface_point)",
     "Retail private per-ledge buoyancy traversal; source definition, decorated four-pointer order and RET 0x10 establish the ABI."),
    (0x100339B0, "?dot_product4@IVP_U_Point_4@@QAENABV1@@Z",
     "double __thiscall corrected(IVP_U_Point_4 *self, const IVP_U_Point_4 *other)",
     "Retail four-component dot product. The source const reference is pointer ABI on x86; decoration and x87 double return establish the signature."),
    (0x100339E0, "?set_line_wise_mult4@IVP_U_Point_4@@QAEXPBV1@0@Z",
     "void __thiscall corrected(IVP_U_Point_4 *self, const IVP_U_Point_4 *left, const IVP_U_Point_4 *right)",
     "Retail four-component element-wise multiply; repeated const Point-4 pointer decoration and RET 8 establish the ABI."),
    (0x100342C0, "?mult_active_x_for_accel@IVP_Linear_Constraint_Solver@@AAEXXZ",
     "void __thiscall corrected(IVP_Linear_Constraint_Solver *self)",
     "Retail private active-variable acceleration multiply; parameterless instance decoration establishes the ABI."),
    (0x10034340, "?mult_x_with_full_A_minus_b@IVP_Linear_Constraint_Solver@@AAEXXZ",
     "void __thiscall corrected(IVP_Linear_Constraint_Solver *self)",
     "Retail private full residual calculation; parameterless instance decoration establishes the ABI."),
    (0x100343A0, "?numerical_stability_ok@IVP_Linear_Constraint_Solver@@AAE?AW4IVP_BOOL@@XZ",
     "IVP_BOOL __thiscall corrected(IVP_Linear_Constraint_Solver *self)",
     "Retail private numerical-stability predicate; decorated IVP_BOOL return and plain RET establish the ABI."),
    (0x10034420, "?alloc_memory@IVP_Linear_Constraint_Solver@@AAEXPAVIVP_U_Memory@@@Z",
     "void __thiscall corrected(IVP_Linear_Constraint_Solver *self, IVP_U_Memory *memory)",
     "Retail private solver workspace allocation; decorated Memory pointer and RET 4 establish the ABI."),
    (0x10034870, "?decrement_sub_solver@IVP_Linear_Constraint_Solver@@AAEXH@Z",
     "void __thiscall corrected(IVP_Linear_Constraint_Solver *self, int sub_position)",
     "Retail private LU sub-solver decrement; decorated integer argument and RET 4 establish the ABI."),
    (0x100348B0, "?increment_sub_solver@IVP_Linear_Constraint_Solver@@AAEXXZ",
     "void __thiscall corrected(IVP_Linear_Constraint_Solver *self)",
     "Retail private LU sub-solver increment; parameterless instance decoration establishes the ABI."),
    (0x10034990, "?do_a_little_random_permutation@IVP_Linear_Constraint_Solver@@AAEXXZ",
     "void __thiscall corrected(IVP_Linear_Constraint_Solver *self)",
     "Retail private anti-stagnation variable permutation; parameterless instance decoration establishes the ABI."),
    (0x10034A40, "?move_not_necessary_actives_to_inactives@IVP_Linear_Constraint_Solver@@AAEXXZ",
     "void __thiscall corrected(IVP_Linear_Constraint_Solver *self)",
     "Retail private active-set reduction pass; parameterless instance decoration establishes the ABI."),
    (0x10034AA0, "?increase_step_count@IVP_Linear_Constraint_Solver@@AAEXPAH@Z",
     "void __thiscall corrected(IVP_Linear_Constraint_Solver *self, int *step_count)",
     "Retail private step-counter increment; decorated integer pointer and RET 4 establish the ABI."),
    (0x10034AB0, "?solve_lc@IVP_Linear_Constraint_Solver@@AAE?AW4IVP_RETURN_TYPE@@XZ",
     "IVP_RETURN_TYPE __thiscall corrected(IVP_Linear_Constraint_Solver *self)",
     "Retail private active-set solve loop; decorated IVP_RETURN_TYPE result and plain RET establish the ABI."),
    (0x10034F10, "?exchange_lcs_variables@IVP_Linear_Constraint_Solver@@AAEXHH@Z",
     "void __thiscall corrected(IVP_Linear_Constraint_Solver *self, int first_index, int second_index)",
     "Retail private constraint-variable exchange; decorated two integers and RET 8 establish the ABI."),
    (0x10034FF0, "?get_fdirection@IVP_Linear_Constraint_Solver@@AAE?AW4IVP_RETURN_TYPE@@XZ",
     "IVP_RETURN_TYPE __thiscall corrected(IVP_Linear_Constraint_Solver *self)",
     "Retail private feasible-direction solve; decorated IVP_RETURN_TYPE result and plain RET establish the ABI."),
    (0x100351C0, "?setup_l_u_solver@IVP_Linear_Constraint_Solver@@AAE?AW4IVP_RETURN_TYPE@@XZ",
     "IVP_RETURN_TYPE __thiscall corrected(IVP_Linear_Constraint_Solver *self)",
     "Retail private incremental-LU setup; decorated IVP_RETURN_TYPE result and plain RET establish the ABI."),
    (0x10035250, "?lcs_bubble_sort_x_vals@IVP_Linear_Constraint_Solver@@AAEXXZ",
     "void __thiscall corrected(IVP_Linear_Constraint_Solver *self)",
     "Retail private active-value ordering pass; parameterless instance decoration establishes the ABI."),
    (0x100352C0, "?get_values_when_setup@IVP_Linear_Constraint_Solver@@AAEXXZ",
     "void __thiscall corrected(IVP_Linear_Constraint_Solver *self)",
     "Retail private post-setup value extraction; parameterless instance decoration establishes the ABI."),
    (0x10035380, "?startup_setup@IVP_Linear_Constraint_Solver@@AAEXH@Z",
     "void __thiscall corrected(IVP_Linear_Constraint_Solver *self, int initial_active_count)",
     "Retail private initial active-set setup; decorated integer argument and RET 4 establish the ABI."),
    (0x100354E0, "?full_setup@IVP_Linear_Constraint_Solver@@AAE?AW4IVP_RETURN_TYPE@@XZ",
     "IVP_RETURN_TYPE __thiscall corrected(IVP_Linear_Constraint_Solver *self)",
     "Retail private full solver rebuild; decorated IVP_RETURN_TYPE result and plain RET establish the ABI."),
    (0x10035680, "?move_variable_to_end@IVP_Linear_Constraint_Solver@@AAEXH@Z",
     "void __thiscall corrected(IVP_Linear_Constraint_Solver *self, int variable_index)",
     "Retail private variable-ordering helper; decorated integer argument and RET 4 establish the ABI."),
    (0x100356B0, "?full_setup_test_ranges@IVP_Linear_Constraint_Solver@@AAEHXZ",
     "int __thiscall corrected(IVP_Linear_Constraint_Solver *self)",
     "Retail private range-validation pass; decorated integer return and plain RET establish the ABI."),
    (0x10036C20, "?exchange_friction_dists@IVP_Friction_System@@QAEXPAVIVP_Contact_Point@@0@Z",
     "void __thiscall corrected(IVP_Friction_System *self, IVP_Contact_Point *first, IVP_Contact_Point *second)",
     "Retail Friction System queue exchange; source definition, repeated Contact Point pointer decoration and RET 8 establish the ABI."),
    (0x100370B0, "?min_added_at_index@IVP_U_Min_Hash@@AAEXPAVIVP_U_Min_Hash_Elem@@H@Z",
     "void __thiscall corrected(IVP_U_Min_Hash *self, IVP_U_Min_Hash_Elem *element, int index)",
     "Retail private minimum-hash insertion update; source definition, decorated pointer/integer order and RET 8 establish the ABI."),
    (0x10037100, "?min_removed_at_index@IVP_U_Min_Hash@@AAEXPAVIVP_U_Min_Hash_Elem@@H@Z",
     "void __thiscall corrected(IVP_U_Min_Hash *self, IVP_U_Min_Hash_Elem *element, int index)",
     "Retail private minimum-hash removal update; source definition, decorated pointer/integer order and RET 8 establish the ABI."),
    (0x10037610, "?get_environment@IVP_Constraint@@IAEPAVIVP_Environment@@XZ",
     "IVP_Environment *__thiscall corrected(IVP_Constraint *self)",
     "Retail protected Constraint environment accessor; source definition and decorated Environment pointer return establish the ABI."),
    (0x10037D90, "?node_to_index@IVP_ov_tree_hash@@IAEHPAVIVP_OV_Node@@@Z",
     "int __thiscall corrected(IVP_ov_tree_hash *self, IVP_OV_Node *node)",
     "Retail protected OV-tree hash helper; source definition, decorated node pointer, integer return and RET 4 establish the ABI."),
    (0x10037DD0, "?compare@IVP_ov_tree_hash@@MBE?AW4IVP_BOOL@@PAX0@Z",
     "IVP_BOOL __thiscall corrected(const IVP_ov_tree_hash *self, void *element0, void *element1)",
     "Retail protected const OV-tree comparator virtual; source declaration, repeated opaque pointers, enum return and RET 8 establish the ABI."),
    (0x10037E20, "?hull_limit_exceeded_event@IVP_Synapse_OO@@UAEXPAVIVP_Hull_Manager@@M@Z",
     "void __thiscall corrected(IVP_Synapse_OO *self, IVP_Hull_Manager *manager, float event_time)",
     "Retail Synapse OO hull-limit callback; IVP_HTIME is float in this image, matching decoration M, source definition and RET 8."),
    (0x10037F50, "?hull_manager_is_going_to_be_deleted_event@IVP_Synapse_OO@@UAEXPAVIVP_Hull_Manager@@@Z",
     "void __thiscall corrected(IVP_Synapse_OO *self, IVP_Hull_Manager *manager)",
     "Retail Synapse OO hull-manager deletion callback; source definition, decorated manager pointer and RET 4 establish the ABI."),
    (0x10037F80, "?get_type@IVP_Synapse_OO@@UAE?AW4IVP_HULL_ELEM_TYPE@@XZ",
     "IVP_HULL_ELEM_TYPE __thiscall corrected(IVP_Synapse_OO *self)",
     "Retail Synapse OO hull-element discriminator; inline source body and decorated enum return establish the ABI."),
    (0x10037FF0, "?hull_manager_is_going_to_be_deleted_event@IVP_OO_Watcher@@QAEXXZ",
     "void __thiscall corrected(IVP_OO_Watcher *self)",
     "Retail OO Watcher owner-deletion handler; source definition and parameterless instance decoration establish the ABI."),
    (0x100381D0, "?collision_is_going_to_be_deleted_event@IVP_OO_Watcher@@UAEXPAVIVP_Collision@@@Z",
     "void __thiscall corrected(IVP_OO_Watcher *self, IVP_Collision *collision)",
     "Retail OO Watcher collision-deletion callback; source definition, decorated Collision pointer and RET 4 establish the ABI."),
    (0x10038240, "?get_objects@IVP_OO_Watcher@@MAEXQAPAVIVP_Real_Object@@@Z",
     "void __thiscall corrected(IVP_OO_Watcher *self, IVP_Real_Object **objects_out)",
     "Retail protected OO Watcher object-pair query; the source two-element output array decays to the decorated pointer-to-pointer ABI."),
    (0x10038260, "?get_ledges@IVP_OO_Watcher@@MAEXQAPBVIVP_Compact_Ledge@@@Z",
     "void __thiscall corrected(IVP_OO_Watcher *self, const IVP_Compact_Ledge **ledges_out)",
     "Retail protected OO Watcher ledge-pair query; the source two-element output array decays to the decorated pointer-to-const-pointer ABI."),
    (0x10038590, "?add_ledge_tree_to_convex_hull@IVP_SurfaceBuilder_Ledge_Soup@@IAEXAAVIVP_Compact_Recursive@@PAVIVV_Sphere@@@Z",
     "void __thiscall corrected(IVP_SurfaceBuilder_Ledge_Soup *self, IVP_Compact_Recursive *recursive_hull, IVV_Sphere *node)",
     "Retail protected Ledge Soup hull traversal. The source non-const reference is pointer ABI on x86; decoration and RET 8 establish both explicit parameters."),
    (0x100385E0, "?build_root_convex_hull@IVP_SurfaceBuilder_Ledge_Soup@@IAEXXZ",
     "void __thiscall corrected(IVP_SurfaceBuilder_Ledge_Soup *self)",
     "Retail protected Ledge Soup root-hull build; source definition and parameterless instance decoration establish the ABI."),
    (0x100386B0, "?cleanup@IVP_SurfaceBuilder_Ledge_Soup@@IAEXXZ",
     "void __thiscall corrected(IVP_SurfaceBuilder_Ledge_Soup *self)",
     "Retail protected Ledge Soup temporary-state cleanup; source definition and parameterless instance decoration establish the ABI."),
    (0x10038750, "?ledges_to_boxes_and_spheres@IVP_SurfaceBuilder_Ledge_Soup@@IAEXXZ",
     "void __thiscall corrected(IVP_SurfaceBuilder_Ledge_Soup *self)",
     "Retail protected Ledge Soup primitive-bound generation; source definition and parameterless instance decoration establish the ABI."),
    (0x10038A70, "?cluster_spheres_topdown_mediancut@IVP_SurfaceBuilder_Ledge_Soup@@IAEXN@Z",
     "void __thiscall corrected(IVP_SurfaceBuilder_Ledge_Soup *self, double threshold_increase)",
     "Retail protected top-down sphere clustering; source IVP_DOUBLE parameter, decorated N argument and RET 8 establish the ABI."),
    (0x10039600, "?allocate_compact_surface@IVP_SurfaceBuilder_Ledge_Soup@@IAEPAVIVP_Compact_Surface@@XZ",
     "IVP_Compact_Surface *__thiscall corrected(IVP_SurfaceBuilder_Ledge_Soup *self)",
     "Retail protected Compact Surface allocator. Decoration and the returned aligned allocation establish the pointer result; writes cover the 0x30 header boundary, packed byte size +0x1C, ledgetree root +0x20 and zero tail +0x24/+0x28/+0x2C."),
    (0x10039820, "?recompile_point_indizes_of_compact_ledge@IVP_SurfaceBuilder_Ledge_Soup@@IAEHPAVIVP_Compact_Ledge@@PAD@Z",
     "int __thiscall corrected(IVP_SurfaceBuilder_Ledge_Soup *self, IVP_Compact_Ledge *source_ledge, char *destination)",
     "Retail protected compact-ledge point-index rewrite; source definition, decorated pointer pair, integer result and RET 8 establish the ABI."),
    (0x10039A70, "?build_ledgetree@IVP_SurfaceBuilder_Ledge_Soup@@IAEPAVIVP_Compact_Ledgetree_Node@@PAVIVV_Sphere@@@Z",
     "IVP_Compact_Ledgetree_Node *__thiscall corrected(IVP_SurfaceBuilder_Ledge_Soup *self, IVV_Sphere *node)",
     "Retail protected one-argument Ledge Soup tree builder overload; source declaration, decorated Sphere pointer and RET 4 establish the ABI."),
    (0x10039B30, "?insert_radius_in_compact_surface@IVP_SurfaceBuilder_Ledge_Soup@@IAEXXZ",
     "void __thiscall corrected(IVP_SurfaceBuilder_Ledge_Soup *self)",
     "Retail protected Compact Surface mass-property finalization; source definition and parameterless instance decoration establish the ABI. Writes cover mass center +0x00, rotation inertia +0x0C, radius +0x18 and the low deviation byte at +0x1C."),
    (0x10039BD0, "?create_compact_ledgetree@IVP_SurfaceBuilder_Ledge_Soup@@IAE?AW4IVP_RETURN_TYPE@@XZ",
     "IVP_RETURN_TYPE __thiscall corrected(IVP_SurfaceBuilder_Ledge_Soup *self)",
     "Retail protected compact Ledge Tree creation; source definition and decorated IVP_RETURN_TYPE result establish the ABI."),
    (0x10039C30, "?get_offset_from_pointlist@IVP_SurfaceBuilder_Pointsoup@@KAHPAVIVP_Template_Point@@HPAVIVP_U_Point@@@Z",
     "int __cdecl corrected(IVP_Template_Point *points, int length, IVP_U_Point *point)",
     "Retail protected static Point Soup lookup; source definition and decorated pointer/integer/pointer sequence establish the cdecl ABI."),
    (0x10039C80, "?get_offset_from_lineslist@IVP_SurfaceBuilder_Pointsoup@@KAHPAVIVP_Template_Line@@HHHPAD@Z",
     "int __cdecl corrected(IVP_Template_Line *lines, int length, int point0, int point1, char *reverse_out)",
     "Retail protected static Point Soup edge lookup; source definition and decorated five-argument sequence establish the cdecl ABI."),
    (0x10039CF0, "?planes_to_template@IVP_SurfaceBuilder_Pointsoup@@KAPAVIVP_Template_Polygon@@PAV?$IVP_U_Vector@VIVP_U_Point@@@@PAV?$IVP_U_Vector@VIVP_SurMan_PS_Plane@@@@@Z",
     "IVP_Template_Polygon *__cdecl corrected(void *points, void *planes)",
     "Retail protected static Point Soup converter. The arguments are specifically IVP_U_Vector<IVP_U_Point>* and IVP_U_Vector<IVP_SurMan_PS_Plane>*; IDA's legacy parser cannot spell those template types, while the decoration fixes their pointer ABI."),
    (0x1003AEE0, "?add_compact_ledge@IVP_Compact_Recursive@@QAEXPBVIVP_Compact_Ledge@@@Z",
     "void __thiscall corrected(IVP_Compact_Recursive *self, const IVP_Compact_Ledge *ledge)",
     "Retail Compact Recursive ledge insertion; source definition, decorated const Ledge pointer and RET 4 establish the ABI."),
    (0x1003AF10, "?build_convex_hull@IVP_Compact_Recursive@@AAEXXZ",
     "void __thiscall corrected(IVP_Compact_Recursive *self)",
     "Retail private Compact Recursive convex-hull build; source definition and parameterless instance decoration establish the ABI."),
    (0x1003B110, "?set_rekursive_convex_hull@IVP_Compact_Recursive@@AAEXXZ",
     "void __thiscall corrected(IVP_Compact_Recursive *self)",
     "Retail private recursive hull-link pass; source definition and parameterless instance decoration establish the ABI."),
    (0x1003B450, "?set@IVP_U_Float_Point@@QAEXMMM@Z_0",
     "void __thiscall corrected(IVP_U_Float_Point *self, float x, float y, float z)",
     "Retail compiler-cloned Float Point setter retained inside recursive-hull code; the original decorated M arguments and RET 0x0C establish three floats."),
    (0x1003B470, "?set_edge@Edge_Key@?1??set_rekursive_convex_hull@IVP_Compact_Recursive@@AAEXXZ@QAEXHH@Z",
     "void __thiscall corrected(void *self, int point0, int point1)",
     "Retail local Edge_Key method inside set_rekursive_convex_hull; its local class has no stable public spelling, but decoration and RET 8 establish the self pointer plus two integers."),
    (0x1003B480, "?compile@IVP_Compact_Recursive@@QAEPAVIVP_Compact_Ledge@@XZ",
     "IVP_Compact_Ledge *__thiscall corrected(IVP_Compact_Recursive *self)",
     "Retail Compact Recursive compilation entry; source definition and decorated Compact Ledge pointer return establish the ABI."),
    (0x1003B4B0, "?point_to_index@IVP_I_FPoint_VHash@@IAEHPAVIVP_U_Float_Point@@@Z",
     "int __thiscall corrected(IVP_I_FPoint_VHash *self, IVP_U_Float_Point *point)",
     "Retail protected float-point hash helper; source definition, decorated point pointer, integer return and RET 4 establish the ABI."),
    (0x1003B4F0, "?compare@IVP_I_FPoint_VHash@@MBE?AW4IVP_BOOL@@PAX0@Z",
     "IVP_BOOL __thiscall corrected(const IVP_I_FPoint_VHash *self, void *element0, void *element1)",
     "Retail protected const float-point comparator virtual; source definition, opaque pointer pair, enum return and RET 8 establish the ABI."),
    (0x1003B550, "?integrate_triangle@IVP_Compact_Ledge_Mass_Center_Solver@@QAEXPBVIVP_Compact_Ledge@@PBVIVP_Compact_Triangle@@HHH@Z",
     "void __thiscall corrected(IVP_Compact_Ledge_Mass_Center_Solver *self, const IVP_Compact_Ledge *ledge, const IVP_Compact_Triangle *triangle, int x, int y, int z)",
     "Retail mass/inertia triangle integration; source definition, decorated pointer pair plus three integers and RET 0x14 establish the ABI."),
    (0x1003B8E0, "?integrate_triangle@IVP_Compact_Ledge_Find_Mass_Center@@QAEXPBVIVP_Compact_Ledge@@PBVIVP_Compact_Triangle@@@Z",
     "void __thiscall corrected(IVP_Compact_Ledge_Find_Mass_Center *self, const IVP_Compact_Ledge *ledge, const IVP_Compact_Triangle *triangle)",
     "Retail mass-center triangle accumulation; source definition, decorated const pointer pair and RET 8 establish the ABI."),
    (0x1003BAB0, "?integrate_ledge@IVP_Compact_Ledge_Find_Mass_Center@@QAEXPBVIVP_Compact_Ledge@@@Z",
     "void __thiscall corrected(IVP_Compact_Ledge_Find_Mass_Center *self, const IVP_Compact_Ledge *ledge)",
     "Retail mass-center ledge accumulation; source definition, decorated const Ledge pointer and RET 4 establish the ABI."),
    (0x1003BAF0, "?integrate_ledges@IVP_Compact_Ledge_Find_Mass_Center@@QAEXPAV?$IVP_U_BigVector@VIVP_Compact_Ledge@@@@@Z",
     "void __thiscall corrected(IVP_Compact_Ledge_Find_Mass_Center *self, void *ledges)",
     "Retail mass-center multi-ledge accumulation. ledges is specifically IVP_U_BigVector<IVP_Compact_Ledge>*; IDA's legacy parser cannot spell the template type, while source and decoration fix the pointer ABI."),
    (0x1003BB20, "?find_center_given_xyz@IVP_Rot_Inertia_Solver@@SAXPAV?$IVP_U_BigVector@VIVP_Compact_Ledge@@@@HHHPBVIVP_U_Matrix@@PAN22@Z",
     "void __cdecl corrected(void *ledges, int x, int y, int z, const IVP_U_Matrix *transform, double *center_out, double *volume_out, double *inertia_out)",
     "Retail static rotation-inertia integration. ledges is specifically IVP_U_BigVector<IVP_Compact_Ledge>*; source order and decoration establish the remaining three ints, matrix and three double outputs."),
    (0x1003C720, "?convert_template_to_ledge@IVP_SurfaceBuilder_Polygon_Convex@@KAPAVIVP_Compact_Ledge@@PAVIVP_Template_Polygon@@@Z",
     "IVP_Compact_Ledge *__cdecl corrected(IVP_Template_Polygon *definition)",
     "Retail static convex polygon-template converter; source definition and decorated input/output pointer types establish the cdecl ABI."),
    (0x1003C760, "?convert_templateledgepolygonsoup_to_ledge@IVP_SurfaceBuilder_Polygon_Convex@@KAPAVIVP_Compact_Ledge@@PAVIVP_Template_Ledge_Polygon_Soup@@@Z",
     "IVP_Compact_Ledge *__cdecl corrected(IVP_Template_Ledge_Polygon_Soup *definition)",
     "Retail static polygon-soup ledge-template converter; source definition and decorated input/output pointer types establish the cdecl ABI."),
    (0x10047170, "?calc_hesse@IVP_Triangle@@QAEXXZ",
     "void __thiscall corrected(IVP_Triangle *self)",
     "Retail Triangle plane-equation update; source definition and parameterless instance decoration establish the ABI."),
    (0x100471A0, "?other_side@IVP_Tri_Edge@@QAEPAV1@XZ",
     "IVP_Tri_Edge *__thiscall corrected(IVP_Tri_Edge *self)",
     "Retail triangle-edge opposite lookup; source definition and decorated same-class pointer return establish the ABI."),
    (0x10047260, "?point_lies_to_the_left@P_Sur_2D_Line@@QAEHPAVIVP_U_Point@@@Z",
     "int __thiscall corrected(P_Sur_2D_Line *self, IVP_U_Point *point)",
     "Retail 2D line sidedness test; source definition, decorated Point pointer, integer return and RET 4 establish the ABI."),
    (0x10047300, "?dist_to_point@P_Sur_2D_Line@@QAENPAVIVP_U_Point@@@Z",
     "double __thiscall corrected(P_Sur_2D_Line *self, IVP_U_Point *point)",
     "Retail 2D line normalized-distance query; source definition, decorated Point pointer and x87 double return establish the ABI."),
    (0x10047370, "?hesse_dist_to_point@P_Sur_2D_Line@@QAENPAVIVP_U_Point@@@Z",
     "double __thiscall corrected(P_Sur_2D_Line *self, IVP_U_Point *point)",
     "Retail 2D line Hesse-distance query; source definition, decorated Point pointer and x87 double return establish the ABI."),
    (0x10047390, "?point_lies_in_interval@P_Sur_2D_Line@@QAEHPAVIVP_U_Point@@@Z",
     "int __thiscall corrected(P_Sur_2D_Line *self, IVP_U_Point *point)",
     "Retail 2D line interval test; source definition, decorated Point pointer, integer return and RET 4 establish the ABI."),
    (0x10047760, "??0P_Sur_2D@@QAE@PAVIVP_Object_Polygon_Tetra@@PAVIVP_Template_Surface@@@Z",
     "P_Sur_2D *__thiscall corrected(P_Sur_2D *self, IVP_Object_Polygon_Tetra *tetra, IVP_Template_Surface *surface)",
     "Retail complete 2D surface constructor; source definition, decorated two-pointer tail, RET 8 and returned self establish the ABI."),
    (0x10048150, "?ivp_check_for_opposite@@YAXPAVIVP_Hash@@PAVIVP_Poly_Point@@1PAVIVP_Tri_Edge@@@Z",
     "void __cdecl corrected(IVP_Hash *hash, IVP_Poly_Point *point0, IVP_Poly_Point *point1, IVP_Tri_Edge *edge)",
     "Retail free opposite-edge linker; source definition and decorated four-pointer sequence establish the cdecl ABI."),
    (0x100482D0, "?set@IVP_Poly_Surface@@QAEXPAVIVP_Template_Surface@@PAVIVP_Object_Polygon_Tetra@@@Z",
     "void __thiscall corrected(IVP_Poly_Surface *self, IVP_Template_Surface *surface, IVP_Object_Polygon_Tetra *tetra)",
     "Retail Poly Surface initializer; source definition, decorated two-pointer order and RET 8 establish the ABI."),
    (0x10048320, "?make_triangles@IVP_Object_Polygon_Tetra@@QAEPBDXZ",
     "const char *__thiscall corrected(IVP_Object_Polygon_Tetra *self)",
     "Retail polygon-to-triangle builder; source IVP_ERROR_STRING result and decorated const-char pointer return establish the ABI."),
    (0x10048810, "?free_triangles@IVP_Object_Polygon_Tetra@@AAEXXZ",
     "void __thiscall corrected(IVP_Object_Polygon_Tetra *self)",
     "Retail private triangle teardown; source definition and parameterless instance decoration establish the ABI."),
    (0x10048880, "?p_link_triangle_self@@YAXPAVIVP_Triangle@@@Z",
     "void __cdecl corrected(IVP_Triangle *triangle)",
     "Retail free triangle self-link helper; source definition and decorated Triangle pointer establish the cdecl ABI."),
    (0x100488B0, "?add_edge_into_point_to_edge_hash@IVP_Object_Polygon_Tetra@@AAEXPAVIVP_Tri_Edge@@@Z",
     "void __thiscall corrected(IVP_Object_Polygon_Tetra *self, IVP_Tri_Edge *edge)",
     "Retail private point-to-edge hash insertion; source definition, decorated edge pointer and RET 4 establish the ABI."),
    (0x10048960, "?generate_double_triangle@IVP_Object_Polygon_Tetra@@SAPAVIVP_Triangle@@PAVIVP_Poly_Point@@00@Z",
     "IVP_Triangle *__cdecl corrected(IVP_Poly_Point *point0, IVP_Poly_Point *point1, IVP_Poly_Point *point2)",
     "Retail static double-sided triangle factory; source definition and decorated repeated Point pointers establish the cdecl ABI."),
    (0x10048A60, "?insert_pierce_info@IVP_Object_Polygon_Tetra@@QAEXXZ",
     "void __thiscall corrected(IVP_Object_Polygon_Tetra *self)",
     "Retail polygon tetra pierce-metadata pass; source definition and parameterless instance decoration establish the ABI."),
    (0x10048ED0, "?generate_compact_ledge@IVP_Compact_Ledge_Generator@@QAEXPAE@Z",
     "void __thiscall corrected(IVP_Compact_Ledge_Generator *self, unsigned char *memory)",
     "Retail Compact Ledge emission entry; source uchar pointer, decorated unsigned-char pointer and RET 4 establish the ABI."),
    (0x1000A4B0, "??0IVP_Cluster_Manager@@QAE@PAVIVP_Environment@@@Z",
     "IVP_Cluster_Manager *__thiscall corrected(IVP_Cluster_Manager *self, IVP_Environment *environment)",
     "Retail Cluster Manager complete constructor. Decorated PAV parameter, callsites and returned ECX-derived EAX establish the typed environment pointer and constructor return ABI."),
    (0x1000B670, "??0IVP_Hash@@QAE@HHPAX@Z",
     "IVP_Hash *__thiscall corrected(IVP_Hash *self, int bucket_count, int key_size, void *not_found_value)",
     "Retail Hash complete constructor. The decorated HHPAX tail and RET 0x0C establish two ints plus the opaque sentinel pointer; machine writes fix bucket_count at self+4, key_size at self+0 and not_found_value at self+8."),
    (0x1000D560, "??0IVP_Core_Fast_PSI@@QAE@XZ",
     "IVP_Core_Fast_PSI *__thiscall corrected(IVP_Core_Fast_PSI *self)",
     "Retail parameterless Fast PSI Core complete constructor; it initializes the compact derived state and returns self in EAX."),
    (0x10010310, "??0IVP_Controller_Buoyancy@@AAE@PAV?$IVP_Attacher_To_Cores@VIVP_Controller_Buoyancy@@@@PAVIVP_Core@@@Z",
     "IVP_Controller_Buoyancy *__thiscall corrected(IVP_Controller_Buoyancy *self, void *attacher_buoyancy, IVP_Core *core)",
     "Retail private Buoyancy Controller complete constructor. The first void pointer is specifically IVP_Attacher_To_Cores<IVP_Controller_Buoyancy>; IDA's legacy declaration parser cannot spell that template type. The decorated template pointer, Core pointer, RET 8 and field accesses fix both explicit parameters."),
    (0x100144D0, "??0IVP_Actuator_Spring_Active@@IAE@PAVIVP_Environment@@PAVIVP_Template_Spring@@@Z",
     "IVP_Actuator_Spring_Active *__thiscall corrected(IVP_Actuator_Spring_Active *self, IVP_Environment *environment, IVP_Template_Spring *definition)",
     "Retail protected Active Spring complete constructor. The decorated two-pointer tail and RET 8 match the environment and mutable spring template callsites."),
    (0x100186E0, "??0IVP_Mindist_Manager@@QAE@PAVIVP_Environment@@@Z",
     "IVP_Mindist_Manager *__thiscall corrected(IVP_Mindist_Manager *self, IVP_Environment *environment)",
     "Retail Mindist Manager complete constructor. The sole PAV parameter, environment backlink writes and RET 4 establish the signature."),
    (0x10018710, "??1IVP_Mindist_Manager@@QAE@XZ",
     "void __thiscall corrected(IVP_Mindist_Manager *self)",
     "[BML direct complete destructor] Retail Mindist Manager complete destructor. It deletes both owned Mindist chains through their retail scalar deleting destructors, frees the wheel-lookahead vector with the DLL allocator and does not delete the outer manager."),
    (0x10020140, "??1IVP_U_Memory@@QAE@XZ",
     "void __thiscall corrected(IVP_U_Memory *self)",
     "[BML direct complete destructor] Retail simulation-memory complete destructor. The five-byte entry jumps to free_mem, which releases every DLL-allocated arena block except caller-owned external storage and does not delete the outer 0x14-byte owner."),
    (0x10020260, "??0IVP_U_Memory@@QAE@XZ",
     "IVP_U_Memory *__thiscall corrected(IVP_U_Memory *self)",
     "Retail parameterless simulation-memory complete constructor. It initializes the 0x14 owner and returns self; the imported integer type was stale."),
    (0x10020C60, "??0IVP_KK_Input@@QAE@PBVIVP_Compact_Edge@@0PAVIVP_Cache_Ledge_Point@@1@Z",
     "IVP_KK_Input *__thiscall corrected(IVP_KK_Input *self, const IVP_Compact_Edge *edge_k, const IVP_Compact_Edge *edge_l, IVP_Cache_Ledge_Point *cache_k, IVP_Cache_Ledge_Point *cache_l)",
     "Retail edge-edge input complete constructor. Decorated repeated-type backreferences and RET 0x10 establish two const edges followed by two mutable caches."),
    (0x10024DB0, "??0IVP_Buoyancy_Solver@@QAE@PAVIVP_Core@@PAVIVP_Controller_Buoyancy@@PBVIVP_Template_Buoyancy@@PBVIVP_U_Float_Point@@@Z",
     "IVP_Buoyancy_Solver *__thiscall corrected(IVP_Buoyancy_Solver *self, IVP_Core *core, IVP_Controller_Buoyancy *controller, const IVP_Template_Buoyancy *definition, const IVP_U_Float_Point *relative_speed)",
     "Retail Buoyancy Solver complete constructor. Four decorated pointer parameters, solver field initialization and RET 0x10 fix constness and order."),
    (0x1002D8E0, "??0IVP_Range_Manager@@QAE@PAVIVP_Environment@@W4IVP_BOOL@@@Z",
     "IVP_Range_Manager *__thiscall corrected(IVP_Range_Manager *self, IVP_Environment *environment, IVP_BOOL delete_on_environment_delete)",
     "Retail Range Manager complete constructor. Decorated Environment/IVP_BOOL parameters and RET 8 replace the untyped integer prototype."),
    (0x1002DC00, "??0IVP_OV_Element@@QAE@PAVIVP_Real_Object@@@Z",
     "IVP_OV_Element *__thiscall corrected(IVP_OV_Element *self, IVP_Real_Object *object)",
     "Retail OV Element complete constructor. The sole Real Object pointer, backlink write and RET 4 replace the imported WORD/int type."),
    (0x1002DEA0, "??0IVP_OV_Node@@QAE@XZ",
     "IVP_OV_Node *__thiscall corrected(IVP_OV_Node *self)",
     "Retail parameterless OV Node complete constructor. It initializes the compact node/vector state and returns self, not int."),
    (0x1002DFF0, "??0IVP_OV_Tree_Manager@@QAE@XZ",
     "IVP_OV_Tree_Manager *__thiscall corrected(IVP_OV_Tree_Manager *self)",
     "Retail parameterless OV Tree Manager complete constructor. Its large owner initialization returns self; the imported double-pointer type was stale."),
    (0x1002F5A0, "??0IVP_Collision_Delegator_Root_Mindist@@QAE@XZ",
     "IVP_Collision_Delegator_Root_Mindist *__thiscall corrected(IVP_Collision_Delegator_Root_Mindist *self)",
     "Retail parameterless Root Mindist delegator complete constructor. Verified vptr installation identifies the concrete owner and returned self ABI."),
    (0x1002FD00, "??0IVP_Polygon@@IAE@PAVIVP_Cluster@@PAVIVP_SurfaceManager@@PBVIVP_Template_Real_Object@@PBVIVP_U_Quat@@PBVIVP_U_Point@@@Z",
     "IVP_Polygon *__thiscall corrected(IVP_Polygon *self, IVP_Cluster *cluster, IVP_SurfaceManager *surface_manager, const IVP_Template_Real_Object *definition, const IVP_U_Quat *rotation, const IVP_U_Point *position)",
     "Retail protected Polygon complete constructor. Decorated pointer sequence, Real Object base call and RET 0x14 establish the five explicit parameters and typed return."),
    (0x10030010, "??0IVP_Ball@@IAE@PAVIVP_Cluster@@PBVIVP_Template_Ball@@PBVIVP_Template_Real_Object@@PBVIVP_U_Quat@@PBVIVP_U_Point@@@Z",
     "IVP_Ball *__thiscall corrected(IVP_Ball *self, IVP_Cluster *cluster, const IVP_Template_Ball *ball_definition, const IVP_Template_Real_Object *object_definition, const IVP_U_Quat *rotation, const IVP_U_Point *position)",
     "Retail protected Ball complete constructor. Decorated const pointer sequence, base-constructor call and RET 0x14 replace the all-integer imported prototype."),
    (0x10030860, "??0IVP_Mindist_Recursive@@QAE@PAVIVP_Environment@@PAVIVP_Collision_Delegator@@@Z",
     "IVP_Mindist_Recursive *__thiscall corrected(IVP_Mindist_Recursive *self, IVP_Environment *environment, IVP_Collision_Delegator *delegator)",
     "Retail Recursive Mindist complete constructor. Two PAV parameters, dual-base vptr writes and RET 8 establish the exact owner and argument types."),
    (0x10033510, "??0IVP_Compact_Edge@@QAE@XZ",
     "IVP_Compact_Edge *__thiscall corrected(IVP_Compact_Edge *self)",
     "Retail parameterless Compact Edge complete constructor. It initializes the packed four-byte edge and returns its concrete self pointer."),
    (0x10037300, "??0IVP_U_Plain@@QAE@PBVIVP_U_Hesse@@@Z",
     "IVP_U_Plain *__thiscall corrected(IVP_U_Plain *self, const IVP_U_Hesse *plane)",
     "Retail Plain complete constructor. Decorated const Hesse pointer, copied plane fields and RET 4 establish the signature."),
    (0x10038000, "??0IVP_OO_Watcher@@QAE@PAVIVP_Collision_Delegator@@PAVIVP_Real_Object@@1@Z",
     "IVP_OO_Watcher *__thiscall corrected(IVP_OO_Watcher *self, IVP_Collision_Delegator *delegator, IVP_Real_Object *object0, IVP_Real_Object *object1)",
     "Retail OO Watcher complete constructor. Decorated Real Object type backreference, subobject vptr writes and RET 0x0C replace the integer prototype."),
    (0x10038490, "??0IVP_Template_Surbuild_LedgeSoup@@QAE@XZ",
     "IVP_Template_Surbuild_LedgeSoup *__thiscall corrected(IVP_Template_Surbuild_LedgeSoup *self)",
     "Retail parameterless Ledge Soup build template complete constructor; concrete returned self replaces the imported DWORD pointer."),
    (0x1003AE80, "??0IVP_Compact_Recursive@@QAE@XZ",
     "IVP_Compact_Recursive *__thiscall corrected(IVP_Compact_Recursive *self)",
     "Retail parameterless Compact Recursive complete constructor. It initializes the 0x0C compact recursion descriptor and returns self."),
    (0x1003B510, "??0IVP_Compact_Ledge_Mass_Center_Solver@@QAE@PBVIVP_U_Matrix@@@Z",
     "IVP_Compact_Ledge_Mass_Center_Solver *__thiscall corrected(IVP_Compact_Ledge_Mass_Center_Solver *self, const IVP_U_Matrix *transform)",
     "Retail mass-center solver complete constructor. Decorated const Matrix pointer, field copies and RET 4 establish the signature."),
    (0x1003B530, "??0IVP_Compact_Ledge_Find_Mass_Center@@QAE@XZ",
     "IVP_Compact_Ledge_Find_Mass_Center *__thiscall corrected(IVP_Compact_Ledge_Find_Mass_Center *self)",
     "Retail parameterless mass-center accumulator complete constructor. It initializes the 0x20 solver state and returns self."),
    (0x1003C080, "??0IVP_SurfaceBuilder_Polygon_Convex@@IAE@PAVIVP_Template_Polygon@@@Z",
     "IVP_SurfaceBuilder_Polygon_Convex *__thiscall corrected(IVP_SurfaceBuilder_Polygon_Convex *self, IVP_Template_Polygon *definition)",
     "Retail protected convex Polygon builder complete constructor. Decorated mutable Template Polygon pointer and RET 4 replace the DWORD/int prototype."),
    (0x1003C150, "??0IVP_SurfaceBuilder_Polygon_Convex@@IAE@PAVIVP_Template_Ledge_Polygon_Soup@@@Z",
     "IVP_SurfaceBuilder_Polygon_Convex *__thiscall corrected(IVP_SurfaceBuilder_Polygon_Convex *self, IVP_Template_Ledge_Polygon_Soup *definition)",
     "Retail protected convex Ledge Soup builder overload. Decorated mutable template pointer and RET 4 establish the overload ABI."),
    (0x100481B0, "??0IVP_Object_Polygon_Tetra@@QAE@PAVIVP_Template_Polygon@@@Z",
     "IVP_Object_Polygon_Tetra *__thiscall corrected(IVP_Object_Polygon_Tetra *self, IVP_Template_Polygon *definition)",
     "Retail tetra polygon owner complete constructor. Decorated mutable Template Polygon pointer, allocation field writes and RET 4 establish the signature."),
    (0x10048900, "??0IVP_Triangle@@QAE@XZ",
     "IVP_Triangle *__thiscall corrected(IVP_Triangle *self)",
     "Retail parameterless Triangle complete constructor. It installs the verified one-slot Triangle vtable, initializes scalar state and returns self."),
    (0x10009830, "??1IVP_Real_Object@@MAE@XZ",
     "void __thiscall corrected(IVP_Real_Object *self)",
     "Retail complete Real Object destructor. It tears down the phantom controller, hull manager, callbacks, cache backlink and Core links, then enters the Object complete destructor; no outer delete occurs."),
    (0x1000A260, "??1IVP_Real_Object_Fast@@UAE@XZ",
     "void __thiscall corrected(IVP_Real_Object_Fast *self)",
     "Retail complete Fast Real Object destructor. The SEH cleanup and direct calls prove Hull Manager Base and Object subobject destruction with no explicit stack argument."),
    (0x1000AB00, "??1IVP_Cluster_Manager@@QAE@XZ",
     "void __thiscall corrected(IVP_Cluster_Manager *self)",
     "Retail complete Cluster Manager destructor. It releases the three owned manager pointers through their verified vtable deletion slots and returns without deleting self."),
    (0x1000B0F0, "??1IVP_Controller@@UAE@XZ",
     "void __thiscall corrected(IVP_Controller *self)",
     "Folded resource-free IVP_Controller complete destructor. Its sole write restores the verified Controller vtable; the imported Triangle owner was stale."),
    (0x1000B6A0, "??1IVP_Hash@@QAE@XZ",
     "void __thiscall corrected(IVP_Hash *self)",
     "[BML direct complete destructor] Retail complete hash destructor. It frees every chained element and the bucket array through the retail CRT boundary, without deleting the outer object."),
    (0x10010430, "??1IVP_Controller_Buoyancy@@EAE@XZ",
     "void __thiscall corrected(IVP_Controller_Buoyancy *self)",
     "Retail complete private Buoyancy Controller destructor. It frees the owned buffer, unregisters from its attacher/Core, and restores the Controller base vptr."),
    (0x10013920, "??1IVP_Draw_Vector_Debug@@QAE@XZ",
     "void __thiscall corrected(IVP_Draw_Vector_Debug *self)",
     "Retail complete debug-vector destructor. It frees the optional copied text at +0x4C and clears the pointer; no outer delete occurs."),
    (0x10013940, "?delete_draw_vector_debug@IVP_Environment@@QAEXXZ",
     "void __thiscall corrected(IVP_Environment *self)",
     "Retail Environment debug-vector list teardown. It walks +0x168, frees each optional text through the retail CRT, invokes the complete node destructor, deletes each node through the retail operator delete and clears the head."),
    (0x100148D0, "??1IVP_Collision_Filter@@UAE@XZ",
     "void __thiscall corrected(IVP_Collision_Filter *self)",
     "[BML direct complete destructor] Resource-free Collision Filter complete destructor. The public API routes this exact body as the one-pass base layer; its single vptr write targets the verified three-slot abstract base table."),
    (0x100148E0, "??_GIVP_Collision_Filter@@UAEPAXI@Z",
     "void *__thiscall corrected(IVP_Collision_Filter *self, unsigned int flags)",
     "Collision Filter scalar-deleting destructor in base vtable slot 2: complete destruction, flags-bit delete, returned self and RET 4."),
    (0x10014A80, "??1IVP_CFEP_Hash@@UAE@XZ",
     "void __thiscall corrected(IVP_CFEP_Hash *self)",
     "Retail complete exclusive-pair hash destructor. It releases stored pair objects then enters the typed VHash complete destructor."),
    (0x10014C10, "??1IVP_Collision_Filter_Exclusive_Pair@@UAE@XZ",
     "void __thiscall corrected(IVP_Collision_Filter_Exclusive_Pair *self)",
     "Retail complete Exclusive Pair filter destructor. It deletes the owned CFEP hash through vtable slot 1 and enters the Collision Filter base destructor."),
    (0x10014F70, "??1IVP_PerformanceCounter@@UAE@XZ",
     "void __thiscall corrected(IVP_PerformanceCounter *self)",
     "[BML direct complete destructor] Resource-free Performance Counter complete destructor. The public API routes this exact body as the one-pass base layer; its single vptr write targets the verified six-slot abstract base table."),
    (0x10014F80, "??_GIVP_PerformanceCounter@@UAEPAXI@Z",
     "void *__thiscall corrected(IVP_PerformanceCounter *self, unsigned int flags)",
     "Performance Counter scalar-deleting destructor in base vtable slot 5. It restores the base vptr, tests flags bit zero, returns self and RET 4; the std::locale::facet import was false."),
    (0x100159D0, "??1IVP_U_Active_Value@@UAE@XZ",
     "void __thiscall corrected(IVP_U_Active_Value *self)",
     "[BML direct complete destructor] Retail complete Active Value destructor. The public API routes this exact body after derived member cleanup; it restores the class vptr, frees the optional owned name, clears the pointer and does not delete self."),
    (0x100161D0, "??1IVP_Synapse@@UAE@XZ",
     "void __thiscall corrected(IVP_Synapse *self)",
     "[BML direct complete destructor] Resource-free Synapse complete destructor used as the public one-pass Synapse layer and as the element destructor for embedded Mindist synapses; its vptr write identifies the base table, not Triangle."),
    (0x100161E0, "??_GIVP_Synapse@@UAEPAXI@Z",
     "void *__thiscall corrected(IVP_Synapse *self, unsigned int flags)",
     "Synapse scalar-deleting destructor in base vtable slot 4. It restores the Synapse vptr, tests flags bit zero, returns self and RET 4; the std::locale::facet import was false."),
    (0x10016220, "??1IVP_Mindist_Base@@UAE@XZ",
     "void __thiscall corrected(IVP_Mindist_Base *self)",
     "Retail complete Mindist Base destructor. Its vector-destructor iterator applies the typed Synapse complete destructor to two 0x1C embedded elements."),
    (0x100162F0, "??1IVP_Mindist@@UAE@XZ",
     "void __thiscall corrected(IVP_Mindist *self)",
     "Retail complete Mindist destructor. It unregisters the collision state, removes both ledge references and delegates both embedded Synapses before restoring the base vptr."),
    (0x1001DB80, "??1IVP_VHash@@UAE@XZ",
     "void __thiscall corrected(IVP_VHash *self)",
     "Retail complete VHash destructor. It restores the VHash vptr and frees its owned table only when the packed ownership byte is zero."),
    (0x1002E0C0, "??_GIVP_ov_tree_hash@@UAEPAXI@Z",
     "void *__thiscall corrected(IVP_ov_tree_hash *self, unsigned int flags)",
     "Retail IVP_ov_tree_hash scalar-deleting destructor. Final vtable 0x10063A24 slot 1 calls complete destructor 0x10037D80, conditionally releases the outer allocation through the retail operator delete, returns self and RET 4."),
    (0x1002DEF0, "??1IVP_OV_Node@@AAE@XZ",
     "void __thiscall corrected(IVP_OV_Node *self)",
     "[BML direct complete destructor] Retail complete private OV Node destructor. It detaches from its parent, recursively deletes children and frees both compact vectors without deleting self; the compatibility class preserves the private access boundary."),
    (0x1002E0E0, "??1IVP_OV_Tree_Manager@@QAE@XZ",
     "void __thiscall corrected(IVP_OV_Tree_Manager *self)",
     "[BML direct complete destructor] Retail complete OV Tree Manager destructor. It deletes the optional owned helper via its verified IVP_ov_tree_hash vtable and destroys the embedded search node at +0x288; no outer delete occurs in this body."),
    (0x1002F570, "??1IVP_Collision_Delegator@@UAE@XZ",
     "void __thiscall corrected(IVP_Collision_Delegator *self)",
     "[BML direct complete destructor] Resource-free Collision Delegator complete destructor. The public API routes this exact body once from derived chains; the vptr target is shared by the verified Root Mindist and OO Watcher base subobjects."),
    (0x1002F580, "??_GIVP_Collision_Delegator@@UAEPAXI@Z",
     "void *__thiscall corrected(IVP_Collision_Delegator *self, unsigned int flags)",
     "Collision Delegator scalar-deleting destructor in base vtable slot 1. It restores the base vptr, tests flags bit zero, returns self and RET 4."),
    (0x1002F5C0, "??_GIVP_Collision_Delegator_Root_Mindist@@UAEPAXI@Z",
     "void *__thiscall corrected(IVP_Collision_Delegator_Root_Mindist *self, unsigned int flags)",
     "Root Mindist scalar-deleting destructor in its five-slot concrete vtable. The trivial derived destructor folds directly into the Collision Delegator base destructor before the flags-bit retail operator delete."),
    (0x1002FF80, "??1IVP_SurfaceManager_Ball@@UAE@XZ",
     "void __thiscall corrected(IVP_SurfaceManager_Ball *self)",
     "Retail complete Ball Surface Manager destructor. It frees the aligned compact-surface allocation then enters the Surface Manager complete destructor."),
    (0x10030910, "??1IVP_Mindist_Recursive@@UAE@XZ",
     "void __thiscall corrected(IVP_Mindist_Recursive *self)",
     "Retail complete Recursive Mindist destructor. It tears down the secondary Collision Delegator base and collision vector, then enters the typed Mindist destructor."),
    (0x10037D80, "??1IVP_ov_tree_hash@@UAE@XZ",
     "void __thiscall corrected(IVP_ov_tree_hash *self)",
     "Retail IVP_ov_tree_hash complete destructor. It restores final vtable 0x10063A24 and tail-enters IVP_VHash::~IVP_VHash; the retained body and virtual base destructor establish the concrete owner."),
    (0x10037F60, "??1IVP_Synapse_OO@@EAE@XZ",
     "void __thiscall corrected(IVP_Synapse_OO *self)",
     "Retail complete private OO Synapse destructor. It restores its vptr and removes the cached Min List element using the stored owner and index."),
    (0x10038100, "??1IVP_OO_Watcher@@UAE@XZ",
     "void __thiscall corrected(IVP_OO_Watcher *self)",
     "Retail complete OO Watcher destructor. It deletes delegated collisions, unregisters from its object, destroys two Synapse OO elements and both vectors, and restores both base vptrs."),
    (0x100482E0, "??1IVP_Object_Polygon_Tetra@@QAE@XZ",
     "void __thiscall corrected(IVP_Object_Polygon_Tetra *self)",
     "Retail complete tetra polygon owner destructor. It frees triangles and the two owned point/index allocations without deleting self."),
    (0x10048950, "??1IVP_Triangle@@UAE@XZ",
     "void __thiscall corrected(IVP_Triangle *self)",
     "Resource-free Triangle complete destructor. Constructor and destructor both install the same verified one-slot Triangle vtable."),
    (0x10048930, "??_GIVP_Triangle@@UAEPAXI@Z",
     "void *__thiscall corrected(IVP_Triangle *self, unsigned int flags)",
     "Triangle scalar-deleting destructor in its sole vtable slot: complete destruction, flags-bit delete, returned self and RET 4."),
    (0x10060C70, "??1IVP_BetterDebugmanager@@UAE@XZ",
     "void __thiscall corrected(IVP_BetterDebugmanager *self)",
     "[BML direct complete destructor] Resource-free Better Debug Manager complete destructor. The public API routes this exact body directly; its sole write restores the verified two-slot manager vtable."),
    (0x10009810, "??_GIVP_Real_Object@@MAEPAXI@Z_0",
     "void *__thiscall corrected(IVP_Real_Object *self, unsigned int flags)",
     "Retail Real Object scalar-deleting destructor copy: calls the complete destructor, tests flags bit zero, returns self and RET 4."),
    (0x10009B40, "??_GIVP_Object@@UAEPAXI@Z_0",
     "void *__thiscall corrected(IVP_Object *self, unsigned int flags)",
     "Retail Object scalar-deleting destructor copy: complete destructor plus flags-bit operator delete, returned self and RET 4."),
    (0x10009C60, "??_GIVP_Cluster@@UAEPAXI@Z",
     "void *__thiscall corrected(IVP_Cluster *self, unsigned int flags)",
     "Retail Cluster scalar-deleting destructor: complete destructor plus flags-bit operator delete, returned self and RET 4."),
    (0x1000A230, "??_GIVP_Object@@UAEPAXI@Z",
     "void *__thiscall corrected(IVP_Object *self, unsigned int flags)",
     "Second retained Object scalar-deleting destructor copy with the same complete-destructor, flags-bit delete, returned-self and RET-4 ABI."),
    (0x10010410, "??_GIVP_Controller_Buoyancy@@EAEPAXI@Z",
     "void *__thiscall corrected(IVP_Controller_Buoyancy *self, unsigned int flags)",
     "Retail private Buoyancy Controller scalar-deleting destructor; machine code confirms complete destruction, flags bit zero, returned self and RET 4."),
    (0x10014A60, "??_GIVP_CFEP_Hash@@UAEPAXI@Z",
     "void *__thiscall corrected(IVP_CFEP_Hash *self, unsigned int flags)",
     "Retail exclusive-pair hash scalar-deleting destructor; complete destruction, flags-bit delete, returned self and RET 4 reject the imported int/char type."),
    (0x100159B0, "??_GIVP_U_Active_Value@@UAEPAXI@Z",
     "void *__thiscall corrected(IVP_U_Active_Value *self, unsigned int flags)",
     "Retail Active Value scalar-deleting destructor; complete destruction, flags-bit delete, returned self and RET 4 establish the MSVC deletion ABI."),
    (0x10016200, "??_GIVP_Mindist_Base@@UAEPAXI@Z",
     "void *__thiscall corrected(IVP_Mindist_Base *self, unsigned int flags)",
     "Retail Mindist Base scalar-deleting destructor; complete destruction, flags-bit delete, returned self and RET 4 establish the MSVC deletion ABI."),
    (0x100162D0, "??_GIVP_Mindist@@UAEPAXI@Z",
     "void *__thiscall corrected(IVP_Mindist *self, unsigned int flags)",
     "Retail Mindist scalar-deleting destructor; complete destruction, flags-bit delete, returned self and RET 4 establish the MSVC deletion ABI."),
    (0x1001DB60, "??_GIVP_VHash@@UAEPAXI@Z",
     "void *__thiscall corrected(IVP_VHash *self, unsigned int flags)",
     "Retail VHash scalar-deleting destructor; complete destruction, flags-bit delete, returned self and RET 4 reject the imported int/char type."),
    (0x1002FDA0, "??_GIVP_Real_Object@@MAEPAXI@Z",
     "void *__thiscall corrected(IVP_Real_Object *self, unsigned int flags)",
     "Second retained Real Object scalar-deleting destructor copy; its local complete-destructor thunk, flags-bit delete, returned self and RET 4 fix the ABI."),
    (0x1002FF60, "??_GIVP_SurfaceManager_Ball@@UAEPAXI@Z",
     "void *__thiscall corrected(IVP_SurfaceManager_Ball *self, unsigned int flags)",
     "Retail Ball Surface Manager scalar-deleting destructor; complete destruction, flags-bit delete, returned self and RET 4 establish the MSVC deletion ABI."),
    (0x100308F0, "??_GIVP_Mindist_Recursive@@UAEPAXI@Z",
     "void *__thiscall corrected(IVP_Mindist_Recursive *self, unsigned int flags)",
     "Retail Recursive Mindist scalar-deleting destructor; complete destruction, flags-bit delete, returned self and RET 4 establish the MSVC deletion ABI."),
    (0x10037F90, "??_GIVP_Synapse_OO@@EAEPAXI@Z",
     "void *__thiscall corrected(IVP_Synapse_OO *self, unsigned int flags)",
     "Retail private OO Synapse scalar-deleting destructor; complete destruction, flags-bit delete, returned self and RET 4 establish the MSVC deletion ABI."),
    (0x100380E0, "??_GIVP_OO_Watcher@@UAEPAXI@Z",
     "void *__thiscall corrected(IVP_OO_Watcher *self, unsigned int flags)",
     "Retail OO Watcher scalar-deleting destructor; complete destruction, flags-bit delete, returned self and RET 4 establish the MSVC deletion ABI."),
    (0x10014940,
     "??1IVP_Collision_Filter_Coll_Group_Ident@@UAE@XZ",
     "void __thiscall corrected(IVP_Collision_Filter_Coll_Group_Ident *self)",
     "Retail complete collision-group filter destructor. It restores the "
     "derived vptr and tail-calls the resource-free Collision Filter base "
     "destructor; no outer allocation is released here."),
    (0x10014950,
     "??_GIVP_Collision_Filter_Coll_Group_Ident@@UAEPAXI@Z",
     "void *__thiscall corrected(IVP_Collision_Filter_Coll_Group_Ident *self, unsigned int flags)",
     "Retail scalar-deleting destructor in vtable slot 2. It calls RVA "
     "0x14940 and conditionally invokes operator delete from flags bit zero; "
     "RET 4 proves a full 32-bit flags argument, not char."),
    (0x10014BF0,
     "??_GIVP_Collision_Filter_Exclusive_Pair@@UAEPAXI@Z",
     "void *__thiscall corrected(IVP_Collision_Filter_Exclusive_Pair *self, unsigned int flags)",
     "Retail scalar-deleting destructor. The decorated name, returned self, "
     "flags-bit test and RET 4 prove the typed pointer return and unsigned "
     "32-bit deletion flags; the imported char prototype was wrong."),
    (0x10014CE0,
     "??1IVP_Meta_Collision_Filter@@UAE@XZ",
     "void __thiscall corrected(IVP_Meta_Collision_Filter *self)",
     "Retail complete Meta Collision Filter destructor. It releases and "
     "clears the filter vector at +0x08, then enters the resource-free base "
     "Collision Filter destructor without freeing self."),
    (0x10014D20,
     "??_GIVP_Meta_Collision_Filter@@UAEPAXI@Z",
     "void *__thiscall corrected(IVP_Meta_Collision_Filter *self, unsigned int flags)",
     "Retail scalar-deleting destructor in vtable slot 2. It calls the "
     "complete destructor at RVA 0x14CE0, returns self, and conditionally "
     "frees the object from flags bit zero; RET 4 rejects the old char ABI."),
    (0x10010650,
     "??1IVP_Attacher_To_Cores_Buoyancy@@UAE@XZ",
     "void __thiscall corrected(IVP_Attacher_To_Cores_Buoyancy *self)",
     "Retail complete Buoyancy-attacher destructor reached by vtable slot 3. "
     "It unregisters self from the active Core set, destroys the embedded "
     "IVP_VHash_Store at +0x04, and does not free the outer allocation."),
    (0x10010790,
     "??_GIVP_Attacher_To_Cores_Buoyancy@@UAEPAXI@Z",
     "void *__thiscall corrected(IVP_Attacher_To_Cores_Buoyancy *self, unsigned int flags)",
     "Retail scalar-deleting destructor in Buoyancy-attacher vtable slot 3. "
     "It calls the complete destructor at RVA 0x10650 and conditionally calls "
     "operator delete when flags bit zero is set; RET 4 fixes the flags ABI."),
    (0x100284D0,
     "ivp_constraint_local_initialize_from_template",
     "void __thiscall corrected(IVP_Constraint_Local *self, const IVP_Template_Constraint *definition)",
     "Internal retained initializer called by the complete Local constructor "
     "after its base, anchors and identity mappings are established. RET 4 and "
     "the sole stack argument confirm the template-pointer member ABI."),
    (0x10020100,
     "?ivp_malloc_aligned@@YAPAXHH@Z",
     "void *__cdecl corrected(int size, int alignment)",
     "Exact aligned allocator used by the reconstructed Grid builder. The "
     "decorated name and callers establish a pointer return and two 32-bit "
     "integer arguments."),
    (0x10021550,
     "?calc_qlen_PF_F_space@IVP_Compact_Ledge_Solver@@SANPBVIVP_Compact_Ledge@@PBVIVP_Compact_Triangle@@PBVIVP_U_Point@@@Z",
     "double __cdecl corrected(const IVP_Compact_Ledge *ledge, const IVP_Compact_Triangle *triangle, const IVP_U_Point *point)",
     "Exact static point-to-triangle squared-distance kernel used by the "
     "reconstructed Grid manager. The decorated name fixes the double return "
     "and three const pointer arguments; callers clean 12 stack bytes."),
    (0x1000C5F0, "?abort_all_async_pushes@IVP_Core@@QAEXXZ",
     "void __thiscall corrected(IVP_Core *self)",
     "Exact public Core entry; decorated name and zero-byte ret establish the no-argument void member signature."),
    (0x10011C70, "?add_core_controller@IVP_Core@@QAEXPAVIVP_Controller@@@Z",
     "void __thiscall corrected(IVP_Core *self, IVP_Controller *controller)",
     "Exact public Core controller insertion entry with one pointer argument."),
    (0x1000D9B0, "?add_friction_info@IVP_Core@@QAEXPAVIVP_Friction_Info_For_Core@@@Z",
     "void __thiscall corrected(IVP_Core *self, IVP_Friction_Info_For_Core *info)",
     "Exact public Core friction backlink insertion entry with one typed pointer."),
    (0x1000C390, "?calc_virt_mass@IVP_Core@@QBENPBVIVP_U_Float_Point@@0@Z",
     "double __thiscall corrected(const IVP_Core *self, const IVP_U_Float_Point *point, const IVP_U_Float_Point *direction)",
     "Exact const Core virtual-mass query; decorated double return and two const point pointers."),
    (0x1000CBD0, "?commit_all_async_pushes@IVP_Core@@QAEXXZ",
     "void __thiscall corrected(IVP_Core *self)",
     "Exact public Core async-push commit entry with no explicit arguments."),
    (0x1000AF90, "?fire_event_object_frozen@IVP_Core@@QAEXXZ",
     "void __thiscall corrected(IVP_Core *self)",
     "Exact public Core frozen-listener dispatch entry."),
    (0x1000C0B0, "?get_diff_surface_speed_of_two_cores@IVP_Core@@SAXPBV1@0PBVIVP_U_Float_Point@@1PAV2@@Z",
     "void __cdecl corrected(const IVP_Core *core0, const IVP_Core *core1, const IVP_U_Float_Point *point0, const IVP_U_Float_Point *point1, IVP_U_Float_Point *result)",
     "Exact static Core relative-surface-speed helper; decorated static signature fixes five pointer arguments."),
    (0x1000C480, "?get_energy_on_test@IVP_Core@@QAENPBVIVP_U_Float_Point@@0@Z",
     "double __thiscall corrected(IVP_Core *self, const IVP_U_Float_Point *speed, const IVP_U_Float_Point *rot_speed)",
     "Exact Core test-energy query with a decorated double return and two const vector inputs."),
    (0x1000C810, "?get_surface_speed@IVP_Core@@QBEXPBVIVP_U_Float_Point@@PAV2@@Z",
     "void __thiscall corrected(const IVP_Core *self, const IVP_U_Float_Point *position, IVP_U_Float_Point *speed_out)",
     "Exact const Core surface-speed query with input position and output vector."),
    (0x1001D4D0, "?grow_friction_system@IVP_Core@@QAE?AW4IVP_BOOL@@XZ",
     "IVP_BOOL __thiscall corrected(IVP_Core *self)",
     "Exact Core friction-system growth entry with public IVP_BOOL return."),
    (0x1000D9A0, "?moveable_core_has_friction_info@IVP_Core@@QAEPAVIVP_Friction_Info_For_Core@@XZ",
     "IVP_Friction_Info_For_Core *__thiscall corrected(IVP_Core *self)",
     "Exact Core movable-friction backlink query."),
    (0x1000C8B0, "?push_core@IVP_Core@@QAEXPBVIVP_U_Float_Point@@00@Z",
     "void __thiscall corrected(IVP_Core *self, const IVP_U_Float_Point *position, const IVP_U_Float_Point *impulse, const IVP_U_Float_Point *angular_impulse)",
     "Exact Core push entry; decorated signature supplies three const float-point arguments."),
    (0x10011C00, "?rem_core_controller@IVP_Core@@QAEXPAVIVP_Controller@@@Z",
     "void __thiscall corrected(IVP_Core *self, IVP_Controller *controller)",
     "Exact public Core controller removal entry with one pointer argument."),
    (0x1000AEA0, "?revive_simulation_core@IVP_Core@@QAE?AW4IVP_BOOL@@XZ",
     "IVP_BOOL __thiscall corrected(IVP_Core *self)",
     "Exact Core simulation revival entry with public IVP_BOOL return."),
    (0x1000C4F0, "?set_radius@IVP_Core@@QAEXMM@Z",
     "void __thiscall corrected(IVP_Core *self, float upper_limit_radius, float max_surface_deviation)",
     "Exact Core radius setter; decorated name and ret 8 confirm two float values."),
    (0x1000CA80, "?test_push_core@IVP_Core@@QBEXPBVIVP_U_Float_Point@@00PAV2@1@Z",
     "void __thiscall corrected(const IVP_Core *self, const IVP_U_Float_Point *position, const IVP_U_Float_Point *impulse, const IVP_U_Float_Point *angular_impulse, IVP_U_Float_Point *speed_out, IVP_U_Float_Point *rot_speed_out)",
     "Exact const Core push predictor with three inputs and two output vectors."),
    (0x1000D140, "?undo_synchronize_rot_z@IVP_Core@@QAEXXZ",
     "void __thiscall corrected(IVP_Core *self)",
     "Exact Core rotation-synchronization undo entry."),
    (0x1000DA50, "?unlink_friction_info@IVP_Core@@QAEXPAVIVP_Friction_Info_For_Core@@@Z",
     "void __thiscall corrected(IVP_Core *self, IVP_Friction_Info_For_Core *info)",
     "Exact Core friction backlink removal entry with one typed pointer."),
    (
        0x10014E00,
        "?reset_and_print_performance_counters@IVP_PerformanceCounter_Simple@@UAEXVIVP_Time@@@Z",
        "void __thiscall corrected(IVP_PerformanceCounter_Simple *self, IVP_Time current_time)",
        "Retained Windows performance-window reset. The public IVP_Time is "
        "passed by value as eight stack bytes and the method returns void.",
    ),
    (
        0x10014F60,
        "?environment_is_going_to_be_deleted@IVP_PerformanceCounter_Simple@@UAEXPAVIVP_Environment@@@Z",
        "void __thiscall corrected(IVP_PerformanceCounter_Simple *self, IVP_Environment *environment)",
        "Retained environment callback dispatches the scalar deleting "
        "destructor and returns void with one environment pointer argument.",
    ),
    (
        0x10014FC0,
        "??_GIVP_PerformanceCounter_Simple@@UAEPAXI@Z",
        "void *__thiscall corrected(IVP_PerformanceCounter_Simple *self, unsigned int flags)",
        "Sixth slot of retail vtable 0x100636B0. This is the MSVC scalar "
        "deleting destructor; the imported char flag and void owner were stale.",
    ),
    (
        0x10014FE0,
        "?pcount@IVP_PerformanceCounter_Simple@@UAEXW4IVP_PERFORMANCE_ELEMENT@@@Z",
        "void __thiscall corrected(IVP_PerformanceCounter_Simple *self, IVP_PERFORMANCE_ELEMENT element)",
        "Retained Windows timing-bucket transition. It samples the performance "
        "counter, updates the selected bucket and returns void; the imported "
        "__int64 return was contradicted by the public vtable slot and RET 4.",
    ),
    (
        0x10015050,
        "?start_pcount@IVP_PerformanceCounter_Simple@@UAEXXZ",
        "void __thiscall corrected(IVP_PerformanceCounter_Simple *self)",
        "Retained start hook writes IVP_PE_PSI_START at +0x10 and returns void.",
    ),
    (
        0x10014900,
        "?check_objects_for_collision_detection@IVP_Collision_Filter_Coll_Group_Ident@@UAE?AW4IVP_BOOL@@PAVIVP_Real_Object@@0@Z",
        "IVP_BOOL __thiscall corrected(IVP_Collision_Filter_Coll_Group_Ident *self, IVP_Real_Object *object0, IVP_Real_Object *object1)",
        "Retained group-identifier collision filter. It compares the two "
        "Real Object no-collision identifiers and returns the public IVP_BOOL; "
        "both object arguments and the enum return are confirmed by the body.",
    ),
    (
        0x10014990,
        "?environment_will_be_deleted@IVP_Collision_Filter_Coll_Group_Ident@@UAEXPAVIVP_Environment@@@Z",
        "void __thiscall corrected(IVP_Collision_Filter_Coll_Group_Ident *self, IVP_Environment *environment)",
        "Retained environment-deletion callback. The environment parameter is "
        "part of the three-slot public filter ABI and the body returns void; "
        "the imported IVP_BOOL return was stale.",
    ),
    (
        0x10014B70,
        "?environment_will_be_deleted@IVP_Collision_Filter_Exclusive_Pair@@UAEXPAVIVP_Environment@@@Z",
        "void __thiscall corrected(IVP_Collision_Filter_Exclusive_Pair *self, IVP_Environment *environment)",
        "Retained exclusive-pair environment callback. It enters the deleting "
        "destructor route for self and has the public void/environment signature.",
    ),
    (
        0x10014CA0,
        "?check_objects_for_collision_detection@IVP_Meta_Collision_Filter@@UAE?AW4IVP_BOOL@@PAVIVP_Real_Object@@0@Z",
        "IVP_BOOL __thiscall corrected(IVP_Meta_Collision_Filter *self, IVP_Real_Object *object0, IVP_Real_Object *object1)",
        "Retained meta-filter dispatch. It walks the embedded filter vector, "
        "calls slot zero with both Real Objects and combines IVP_BOOL results.",
    ),
    (
        0x10014D90,
        "?environment_will_be_deleted@IVP_Meta_Collision_Filter@@UAEXPAVIVP_Environment@@@Z",
        "void __thiscall corrected(IVP_Meta_Collision_Filter *self, IVP_Environment *environment)",
        "Retained meta-filter environment callback. It forwards the typed "
        "environment through slot one, removes children and optionally deletes "
        "self; the imported generic integer prototype was stale.",
    ),
    (
        0x10013240,
        "?set_delta_PSI_time@IVP_Environment@@QAEXN@Z",
        "void __thiscall corrected(IVP_Environment *self, double seconds)",
        "Retained fixed-PSI setter. It copies the 8-byte argument to +0xc0, "
        "stores its double reciprocal at +0xc8 and returns void with RET 8; "
        "the old __int64 return and double-pointer owner were stale.",
    ),
    (
        0x10013650,
        "?fire_object_is_removed_from_collision_detection@IVP_Environment@@QAEXPAVIVP_Real_Object@@@Z",
        "void __thiscall corrected(IVP_Environment *self, IVP_Real_Object *object)",
        "Iterates collision_delegator_roots at Environment +0x158 and invokes "
        "root-delegator slot 2. The imported object-revived name and event "
        "parameter are contradicted by both field access and vtable target.",
    ),
    (
        0x10013680,
        "?set_gravity@IVP_Environment@@QAEXPAVIVP_U_Point@@@Z",
        "void __thiscall corrected(IVP_Environment *self, IVP_U_Point *gravity)",
        "Retained environment gravity setter. It copies the point to +0xd0, "
        "stores its float length at +0xf0 and forwards the same input to the "
        "standard gravity controller; RET 4 and decorated X prove void.",
    ),
    (
        0x100189F0,
        "??1IVP_Cache_Object_Manager@@QAE@XZ",
        "void __thiscall corrected(IVP_Cache_Object_Manager *self)",
        "Retained complete Cache Object Manager destructor. It walks every "
        "0xd0-byte entry, invalidates any attached Real Object backlink, "
        "frees the contiguous buffer and clears the owner pointer.",
    ),
    (
        0x1000DAE0,
        "?isqrt_float@IVP_Inline_Math@@SAMM@Z",
        "float __cdecl corrected(float squared_length)",
        "Retained static fast inverse-square-root implementation. The M "
        "decorated return/parameter codes and x87 ST0 result prove float; "
        "the old imported int return discarded the scalar ABI.",
    ),
    (
        0x1000DB80,
        "?isqrt_double@IVP_Inline_Math@@SANN@Z",
        "double __cdecl corrected(double squared_length)",
        "Retained static fast inverse-square-root implementation. The N "
        "decorated return/parameter codes, 8-byte stack input and x87 ST0 "
        "result prove double; the old imported int return was stale.",
    ),
    (
        0x1000BD70,
        "?get_adhesion@IVP_Material_Manager@@UAENPAUIVP_Contact_Situation@@@Z",
        "double __thiscall corrected(IVP_Material_Manager *self, IVP_Contact_Situation *contact)",
        "Ballance-only Material Manager virtual query. It dispatches slot 3 "
        "on both materials at contact +0x50/+0x54, adds the two doubles and "
        "returns through the x87 double convention.",
    ),
    (
        0x1000BE30,
        "?get_material_by_index@IVP_Material_Manager@@UAEPAVIVP_Material@@PBVIVP_U_Point@@H@Z",
        "IVP_Material *__thiscall corrected(IVP_Material_Manager *self, const IVP_U_Point *world_position, int material_index)",
        "Retained default material lookup. RET 8 confirms both public "
        "arguments; the lazily allocated 0x30 IVP_Material_Simple singleton "
        "is returned for every position and index.",
    ),
    (
        0x1002F5E0,
        "??0IVP_Anomaly_Limits@@QAE@W4IVP_BOOL@@@Z",
        "IVP_Anomaly_Limits *__thiscall corrected(IVP_Anomaly_Limits *self, IVP_BOOL delete_on_environment_delete)",
        "Retained complete Anomaly Limits constructor. It installs the "
        "two-slot retail vtable and writes the delete flag plus all three "
        "limits through +0x10, proving the complete 0x14 layout.",
    ),
    (
        0x1002F660,
        "??0IVP_Anomaly_Manager@@QAE@W4IVP_BOOL@@@Z",
        "IVP_Anomaly_Manager *__thiscall corrected(IVP_Anomaly_Manager *self, IVP_BOOL delete_on_environment_delete)",
        "Previously anonymous complete Anomaly Manager constructor. It "
        "installs the seven-slot retail vtable and writes the deletion flag "
        "at +0x04, proving the complete 0x08 layout.",
    ),
    (
        0x1002F170,
        "??1IVP_Time_Manager@@QAE@XZ",
        "void __thiscall corrected(IVP_Time_Manager *self)",
        "Retained complete Time Manager destructor. It deletes every event "
        "still queued, frees the event manager, destroys and frees the "
        "owned Min List, then clears both owner pointers.",
    ),
    (
        0x1002F1D0,
        "?insert_event@IVP_Time_Manager@@QAEXPAVIVP_Time_Event@@VIVP_Time@@@Z",
        "void __thiscall corrected(IVP_Time_Manager *self, IVP_Time_Event *event, IVP_Time time)",
        "Retained Time Manager queue insertion. RET 0x0c covers the event "
        "pointer and one 8-byte by-value IVP_Time; the returned Min List "
        "handle is stored in IVP_Time_Event::index at +0x04.",
    ),
    (
        0x1002F200,
        "?remove_event@IVP_Time_Manager@@QAEXPAVIVP_Time_Event@@@Z",
        "void __thiscall corrected(IVP_Time_Manager *self, IVP_Time_Event *event)",
        "Retained Time Manager queue removal. It passes the +0x04 event "
        "index to IVP_U_Min_List::remove_minlist_elem and returns void with "
        "RET 4; removal intentionally leaves the cached event index intact.",
    ),
    (
        0x1002F250,
        "?event_loop@IVP_Time_Manager@@QAEXPAVIVP_Environment@@VIVP_Time@@@Z",
        "void __thiscall corrected(IVP_Time_Manager *self, IVP_Environment *environment, IVP_Time until_time)",
        "Retained Time Manager event loop. It forwards self, environment "
        "and both dwords of the by-value IVP_Time through the event-manager "
        "vtable and returns with RET 0x0c; the old int return was stale.",
    ),
    (
        0x100160E0,
        "??0IVP_Mindist_Base@@QAE@PAVIVP_Collision_Delegator@@@Z",
        "IVP_Mindist_Base *__thiscall corrected(IVP_Mindist_Base *self, IVP_Collision_Delegator *delegator)",
        "Complete retained Mindist Base constructor. Its +0x08 delegator and "
        "+0x0c/+0x10 index writes expose the IVP_Collision prefix; the old "
        "integer this/argument import discarded both public owner types.",
    ),
    (
        0x10016290,
        "??0IVP_Mindist@@QAE@PAVIVP_Environment@@PAVIVP_Collision_Delegator@@@Z",
        "IVP_Mindist *__thiscall corrected(IVP_Mindist *self, IVP_Environment *environment, IVP_Collision_Delegator *delegator)",
        "Complete retained Mindist constructor. It forwards the delegator to "
        "the corrected base constructor and uses the environment for manager "
        "registration; both old integer arguments were stale imports.",
    ),
    (
        0x1002F2A0,
        "?env_set_current_time@IVP_Time_Manager@@QAEXPAVIVP_Environment@@VIVP_Time@@@Z",
        "void __thiscall corrected(IVP_Time_Manager *self, IVP_Environment *environment, IVP_Time time)",
        "Retained Time Manager forwarding entry. The decorated X return and "
        "absence of a defined EAX result prove void; RET 0x0c covers the "
        "environment pointer and 8-byte by-value IVP_Time.",
    ),
    (
        0x1001CBC0,
        "?reset_time@IVP_Friction_System@@UAEXVIVP_Time@@@Z",
        "void __thiscall corrected(IVP_Friction_System *self, IVP_Time offset)",
        "Virtual Friction System time rebase. It forwards the two dwords of "
        "one by-value IVP_Time to every contact and returns with RET 8; the "
        "old int(int,double) import lost both owner and void return.",
    ),
    (
        0x10012A50,
        "?init_freeze_manager@IVP_Freeze_Manager@@QAEXXZ",
        "void __thiscall corrected(IVP_Freeze_Manager *self)",
        "Retained one-field initializer. It writes float 0.3 at +0x00; the "
        "old imported _DWORD pointer lost the public owner type.",
    ),
    (
        0x10009C80,
        "??0IVP_Cluster@@IAE@PAVIVP_Environment@@@Z",
        "IVP_Cluster *__thiscall corrected(IVP_Cluster *self, IVP_Environment *environment)",
        "Retained root-cluster constructor. It calls the IVP_Object root "
        "constructor, installs the one-slot Cluster table, clears the child "
        "head at +0x1c and writes IVP_CLUSTER at +0x04.",
    ),
    (
        0x10009CB0,
        "??1IVP_Cluster@@UAE@XZ",
        "void __thiscall corrected(IVP_Cluster *self)",
        "Retained complete Cluster destructor. It repeatedly invokes the "
        "current child's deleting-destructor slot until Object destruction "
        "has unlinked the child head, then calls IVP_Object::~IVP_Object.",
    ),
    (
        0x1001A740,
        "??0IVP_Hull_Manager_Base@@QAE@XZ",
        "IVP_Hull_Manager_Base *__thiscall corrected(IVP_Hull_Manager_Base *self)",
        "Complete constructor, not a member-init helper: it initializes the "
        "gradient prefix and constructs the embedded IVP_U_Min_List at +0x20.",
    ),
    (
        0x1001A7A0,
        "??1IVP_Hull_Manager_Base@@QAE@XZ",
        "void __thiscall corrected(IVP_Hull_Manager_Base *self)",
        "Complete destructor. Its call to RVA 0x30170 destroys the embedded "
        "IVP_U_Min_List at +0x20, so API wrappers must not destroy it twice.",
    ),
    (
        0x100300F0,
        "??0IVP_U_Min_List@@QAE@H@Z",
        "IVP_U_Min_List *__thiscall corrected(IVP_U_Min_List *self, int size)",
        "Previously anonymous retained Min List constructor. RET 4 confirms "
        "the public int parameter; the body stores the low 16-bit capacity, "
        "allocates size*0x10 bytes and initializes the free chain.",
    ),
    (
        0x10030170,
        "??1IVP_U_Min_List@@QAE@XZ",
        "void __thiscall corrected(IVP_U_Min_List *self)",
        "Previously anonymous retained Min List destructor. It frees the "
        "element buffer held at +0x04.",
    ),
    (
        0x100108A0,
        "??1IVP_Controller_Phantom@@QAE@XZ",
        "void __thiscall corrected(IVP_Controller_Phantom *self)",
        "Previously anonymous complete Phantom destructor. Both callers "
        "invoke it before operator delete. It notifies listeners in reverse "
        "order, clears IVP_Real_Object::controller_phantom at +0x1c, and "
        "owns all embedded/container teardown through offset +0x34.",
    ),
    (
        0x10010D00,
        "??0IVP_Controller_Phantom@@IAE@PAVIVP_Real_Object@@PBVIVP_Template_Phantom@@@Z",
        "IVP_Controller_Phantom *__thiscall corrected(IVP_Controller_Phantom *self, IVP_Real_Object *object, const IVP_Template_Phantom *configuration)",
        "Protected complete Phantom constructor used by "
        "IVP_Real_Object::convert_to_phantom. It constructs the embedded "
        "listener vector and active mindist set before creating optional "
        "object/core tracking sets.",
    ),
    (
        0x100042B0,
        "?event_post_collision@PhysicsCollDetectionListener@@UAEXPAVIVP_Event_Collision@@@Z",
        "void __thiscall corrected(PhysicsCollDetectionListener *self, IVP_Event_Collision *event)",
        "Retail vtable slot 0. The old imported pre-collision name is wrong: "
        "the Building Block implements post-collision and registers callback bit 0x01.",
    ),
    (
        0x1000A770,
        "?fire_event_post_collision@IVP_Cluster_Manager@@QAEXPAVIVP_Real_Object@@PAVIVP_Event_Collision@@@Z",
        "void __thiscall corrected(IVP_Cluster_Manager *self, IVP_Real_Object *object, IVP_Event_Collision *event)",
        "Private-object listener dispatch: callback bit 0x01, retail vtable slot 0.",
    ),
    (
        0x1000A850,
        "?fire_event_collision_object_deleted@IVP_Cluster_Manager@@QAEXPAVIVP_Real_Object@@@Z",
        "void __thiscall corrected(IVP_Cluster_Manager *self, IVP_Real_Object *object)",
        "Private-object listener dispatch: callback bit 0x02, retail vtable slot 1.",
    ),
    (
        0x1000A8A0,
        "?fire_event_friction_created@IVP_Cluster_Manager@@QAEXPAVIVP_Real_Object@@PAVIVP_Event_Friction@@@Z",
        "void __thiscall corrected(IVP_Cluster_Manager *self, IVP_Real_Object *object, IVP_Event_Friction *event)",
        "Private-object contact-creation dispatch: callback bit 0x04, retail vtable slot 2.",
    ),
    (
        0x1000A900,
        "?fire_event_friction_deleted@IVP_Cluster_Manager@@QAEXPAVIVP_Real_Object@@PAVIVP_Event_Friction@@@Z",
        "void __thiscall corrected(IVP_Cluster_Manager *self, IVP_Real_Object *object, IVP_Event_Friction *event)",
        "Private-object contact-destruction dispatch: callback bit 0x04, retail vtable slot 3.",
    ),
    (
        0x1000A9F0,
        "?check_for_unused_objects@IVP_Cluster_Manager@@QAEXPAVIVP_Universe_Manager@@@Z",
        "void __thiscall corrected(IVP_Cluster_Manager *self, IVP_Universe_Manager *universe_manager)",
        "Retail large-world policy dispatcher. Calls universe-manager slot 3 "
        "for thresholds and slot 1 for each object no longer needed.",
    ),
    (
        0x10009A40,
        "?unlink_contact_points@IVP_Real_Object@@AAEXW4IVP_BOOL@@@Z",
        "void __thiscall corrected(IVP_Real_Object *self, IVP_BOOL silent)",
        "Retained private all-contact unlink helper. Object friction list is "
        "read at +0x28; a false silent flag wakes both neighboring cores.",
    ),
    (
        0x10009610,
        "?update_exact_mindist_events_of_object@IVP_Real_Object@@IAEXXZ",
        "void __thiscall corrected(IVP_Real_Object *self)",
        "Retained protected exact-mindist event traversal. The empty-list "
        "return leaves EAX undefined, so the imported int return is stale; "
        "the source declaration and all machine-code paths establish void.",
    ),
    (
        0x1000A590,
        "??_GIVP_Object_Callback_Table_Hash@@UAEPAXI@Z",
        "void *__thiscall corrected(IVP_Object_Callback_Table_Hash *self, unsigned int flags)",
        "IVP_Object_Callback_Table_Hash scalar deleting destructor, retail "
        "vtable 0x100633C8 slot 1.",
    ),
    (
        0x1000A5B0,
        "??_GIVP_Collision_Callback_Table_Hash@@UAEPAXI@Z",
        "void *__thiscall corrected(IVP_Collision_Callback_Table_Hash *self, unsigned int flags)",
        "IVP_Collision_Callback_Table_Hash scalar deleting destructor, "
        "retail vtable 0x100633C0 slot 1.",
    ),
    (
        0x1000A630,
        "?find_table@IVP_Object_Callback_Table_Hash@@QAEPAVIVP_Object_Callback_Table@@PAVIVP_Real_Object@@@Z",
        "IVP_Object_Callback_Table *__thiscall corrected(IVP_Object_Callback_Table_Hash *self, IVP_Real_Object *object)",
        "Retained typed lookup used by all per-object lifecycle dispatchers. "
        "The imported unknown_libname_32 name hid this public-listener path.",
    ),
    (
        0x1000AAD0,
        "??1IVP_Object_Callback_Table@@QAE@XZ",
        "void __thiscall corrected(IVP_Object_Callback_Table *self)",
        "Folded destructor body shared by the layout-identical object and "
        "collision callback tables and IVP_Environment_Manager; releases "
        "the listener/environment vector at owner offset 0x04.",
    ),
    (
        0x1000AC70,
        "?remove_table@IVP_Object_Callback_Table_Hash@@QAEPAVIVP_Object_Callback_Table@@PAVIVP_Real_Object@@@Z",
        "IVP_Object_Callback_Table *__thiscall corrected(IVP_Object_Callback_Table_Hash *self, IVP_Real_Object *object)",
        "Retained typed removal from the object-listener hash at cluster "
        "manager offset 0x08; 0x1000AD80 is the collision-table counterpart.",
    ),
    (
        0x1000B0D0,
        "??_GIVP_Friction_System@@UAEPAXI@Z",
        "void *__thiscall corrected(IVP_Friction_System *self, unsigned int flags)",
        "IVP_Friction_System scalar deleting destructor, controller vtable "
        "slot 6. Calls the complete destructor then conditionally operator delete.",
    ),
    (
        0x1000B100,
        "??1IVP_Friction_System@@UAE@XZ",
        "void __thiscall corrected(IVP_Friction_System *self)",
        "IVP_Friction_System complete destructor. Releases the three vectors "
        "at +0x24/+0x2c/+0x34 and restores controller vptrs.",
    ),
    (
        0x1000B360,
        "?add_dist_to_system@IVP_Friction_System@@QAEXPAVIVP_Contact_Point@@@Z",
        "void __thiscall corrected(IVP_Friction_System *self, IVP_Contact_Point *contact_point)",
        "Retained friction-distance insertion body. The contact-creation path "
        "calls it with self in ECX; writes prove Contact_Point system/next/prev "
        "at +0x70/+0x00/+0x04 and Friction_System first/count at +0x20/+0x3e.",
    ),
    (
        0x1001A8B0,
        "??0IVP_Contact_Point@@QAE@PAVIVP_Mindist@@@Z",
        "IVP_Contact_Point *__thiscall corrected(IVP_Contact_Point *self, IVP_Mindist *mindist)",
        "Previously anonymous complete contact-point constructor. The retail "
        "factory allocates exactly 0x78 bytes before calling it; its writes "
        "place the two synapses at +0x08/+0x1c, IVP_Time at +0x68 and the "
        "friction-system backlink at +0x70.",
    ),
    (
        0x1001AA20,
        "?two_values_friction@IVP_Contact_Point@@AAENPAVIVP_U_Float_Point@@@Z",
        "double __thiscall corrected(IVP_Contact_Point *self, IVP_U_Float_Point *world_friction)",
        "Retained two-axis friction projection. Decorated N return and x87 "
        "epilogue confirm IVP_DOUBLE; the body uses contact spans at +0x38 "
        "and long-term span vectors through +0x40.",
    ),
    (
        0x1001AD90,
        "?get_and_set_real_friction_len@IVP_Contact_Point@@AAENPAVIVP_U_Float_Point@@@Z",
        "double __thiscall corrected(IVP_Contact_Point *self, IVP_U_Float_Point *world_friction)",
        "Retained real-friction length/update helper with one mutable float "
        "vector argument and an x87 double return.",
    ),
    (
        0x1001AE40,
        "?static_friction_single@IVP_Contact_Point@@AAEXPBVIVP_Event_Sim@@MM@Z",
        "void __thiscall corrected(IVP_Contact_Point *self, const IVP_Event_Sim *event, float desired_gap, float speedup_factor)",
        "Retained one-contact distance-keeper solve. The decorated MM tail "
        "and RET 0x0c confirm two float values after the event pointer.",
    ),
    (
        0x1001BD90,
        "?ease_the_friction_force@IVP_Contact_Point@@AAEXPAVIVP_U_Float_Point@@@Z",
        "void __thiscall corrected(IVP_Contact_Point *self, IVP_U_Float_Point *difference)",
        "Retained friction easing helper. It projects the supplied world "
        "difference onto both long-term span vectors and updates +0x38/+0x3c.",
    ),
    (
        0x100248A0,
        "?get_rot_speed_uncertainty@IVP_Contact_Point@@AAEMXZ",
        "float __thiscall corrected(IVP_Contact_Point *self)",
        "Retained rotational collision-distance uncertainty calculation; "
        "decorated M and x87 return confirm IVP_FLOAT.",
    ),
    (
        0x10024930,
        "?get_rescue_speed_impact@IVP_Contact_Point@@AAEMPAVIVP_Environment@@@Z",
        "float __thiscall corrected(IVP_Contact_Point *self, IVP_Environment *environment)",
        "Retained penetration rescue-speed calculation. It combines gap "
        "depth with rotational uncertainty and returns IVP_FLOAT.",
    ),
    (
        0x100249B0,
        "?calc_coll_distance@IVP_Contact_Point@@AAEXXZ",
        "void __thiscall corrected(IVP_Contact_Point *self)",
        "Retained impact-system collision-distance predictor. It marks the "
        "long-term record valid and writes rescue speed and predicted distance.",
    ),
    (
        0x1001F850,
        "?reset_time@IVP_Contact_Point@@AAEXVIVP_Time@@@Z",
        "void __thiscall corrected(IVP_Contact_Point *self, IVP_Time offset)",
        "Retained contact timestamp rebasing helper. The by-value IVP_Time "
        "is subtracted from the double timestamp at +0x68.",
    ),
    (
        0x1001D910,
        "?is_same_as@IVP_Contact_Point@@ABE?AW4IVP_BOOL@@PBVIVP_Mindist@@@Z",
        "IVP_BOOL __thiscall corrected(const IVP_Contact_Point *self, const IVP_Mindist *mindist)",
        "Retained const contact/mindist identity test. It compares both "
        "synapse objects, edges and statuses using the Ballance 0x14 nodes.",
    ),
    (
        0x1001F050,
        "?p_calc_friction_s_PP@IVP_Contact_Point@@AAEXPBVIVP_U_Point@@0PAVIVP_Impact_Solver_Long_Term@@PAVIVP_U_Float_Point@@@Z",
        "void __thiscall corrected(IVP_Contact_Point *self, const IVP_U_Point *first, const IVP_U_Point *second, IVP_Impact_Solver_Long_Term *information, IVP_U_Float_Point *contact_difference)",
        "Retained point/point contact-coordinate calculation; RET 0x10 "
        "confirms four pointer arguments.",
    ),
    (
        0x1001EE70,
        "?p_calc_friction_qr_PF@IVP_Contact_Point@@AAEXPBVIVP_U_Point@@PBVIVP_Compact_Edge@@PAVIVP_Cache_Ledge_Point@@PAVIVP_Impact_Solver_Long_Term@@PAVIVP_U_Float_Point@@@Z",
        "void __thiscall corrected(IVP_Contact_Point *self, const IVP_U_Point *point, const IVP_Compact_Edge *face, IVP_Cache_Ledge_Point *face_cache, IVP_Impact_Solver_Long_Term *information, IVP_U_Float_Point *contact_difference)",
        "Retained point/face friction-coordinate calculation with five "
        "pointer arguments.",
    ),
    (
        0x1001F160,
        "?p_calc_friction_s_PK@IVP_Contact_Point@@AAEXPBVIVP_U_Point@@PBVIVP_Compact_Edge@@PAVIVP_Cache_Ledge_Point@@PAVIVP_Impact_Solver_Long_Term@@PAVIVP_U_Float_Point@@@Z",
        "void __thiscall corrected(IVP_Contact_Point *self, const IVP_U_Point *point, const IVP_Compact_Edge *edge, IVP_Cache_Ledge_Point *edge_cache, IVP_Impact_Solver_Long_Term *information, IVP_U_Float_Point *contact_difference)",
        "Retained point/edge friction-coordinate calculation with five "
        "pointer arguments.",
    ),
    (
        0x1001F3B0,
        "?p_calc_friction_ss_KK@IVP_Contact_Point@@AAEXPBVIVP_Compact_Edge@@0PAVIVP_Cache_Ledge_Point@@1PAVIVP_Impact_Solver_Long_Term@@PAVIVP_U_Float_Point@@@Z",
        "void __thiscall corrected(IVP_Contact_Point *self, const IVP_Compact_Edge *first_edge, const IVP_Compact_Edge *second_edge, IVP_Cache_Ledge_Point *first_cache, IVP_Cache_Ledge_Point *second_cache, IVP_Impact_Solver_Long_Term *information, IVP_U_Float_Point *contact_difference)",
        "Retained edge/edge friction-coordinate calculation with six pointer "
        "arguments; RET 0x18 confirms the stack shape.",
    ),
    (
        0x1001CCD0,
        "?remove_energy_gained_by_real_friction@IVP_Friction_Core_Pair@@QAEXXZ",
        "void __thiscall corrected(IVP_Friction_Core_Pair *self)",
        "Previously anonymous retained pair-energy correction. The friction "
        "system calls it directly and the body forwards accumulated energy "
        "to IVP_Friction_Core_Pair::destroy_energy.",
    ),
    (
        0x10020030,
        "?generate_contact_point@IVP_Friction_Manager@@SAPAVIVP_Contact_Point@@PAVIVP_Mindist@@PAW4IVP_BOOL@@@Z",
        "IVP_Contact_Point *__cdecl corrected(IVP_Mindist *mindist, IVP_BOOL *was_successful)",
        "Previously anonymous static factory. Its caller-cleanup convention "
        "and two stack arguments prove cdecl; the allocation immediate is "
        "0x78 and the result is initialized by the retained constructor.",
    ),
    (
        0x1000B000,
        "??0IVP_Friction_System@@QAE@PAVIVP_Environment@@@Z",
        "IVP_Friction_System *__thiscall corrected(IVP_Friction_System *self, IVP_Environment *environment)",
        "Retained complete friction-system constructor. It initializes the "
        "three vectors and confirmed fields through +0x44, then installs the "
        "seven-slot Ballance controller vtable.",
    ),
    (
        0x1001D340,
        "??0IVP_Friction_Core_Pair@@QAE@XZ",
        "IVP_Friction_Core_Pair *__thiscall corrected(IVP_Friction_Core_Pair *self)",
        "Retained pair constructor. It initializes only the vector, time, "
        "integrated energy and ease counter; debug span and core pointers "
        "remain deliberately untouched until pair insertion.",
    ),
    (
        0x1001BC20,
        "?calc_friction_forces@IVP_Friction_System@@QAEXPBVIVP_Event_Sim@@@Z",
        "void __thiscall corrected(IVP_Friction_System *self, const IVP_Event_Sim *event)",
        "Exact decorated retail member retained by the friction PSI path. "
        "Its body contains the inlined pair slide-way, pretension and force "
        "loop used to reconstruct the public pair helpers.",
    ),
    (
        0x1001B080,
        "?friction_force_local_constraint_2d@IVP_Contact_Point@@AAEMPBVIVP_Event_Sim@@@Z",
        "float __thiscall corrected(IVP_Contact_Point *self, const IVP_Event_Sim *event)",
        "Previously anonymous retained two-dimensional contact constraint. "
        "The inlined pair loop at RVA 0x1BC20 calls it when the Ballance "
        "two-friction-values byte at contact +0x34 is not one.",
    ),
    (
        0x1001B8B0,
        "?friction_force_local_constraint_1d@IVP_Contact_Point@@AAEXPBVIVP_Event_Sim@@@Z",
        "void __thiscall corrected(IVP_Contact_Point *self, const IVP_Event_Sim *event)",
        "Retained one-dimensional contact constraint. The inlined pair loop "
        "selects it exactly when the byte at contact +0x34 equals one.",
    ),
    (
        0x10024140,
        "?do_impact_long_term@IVP_Impact_Solver_Long_Term@@QAEXQAPAVIVP_Core@@MPAVIVP_Contact_Point@@@Z",
        "void __thiscall corrected(IVP_Impact_Solver_Long_Term *self, IVP_Core **pushed_cores, float rescue_speed, IVP_Contact_Point *contact_point)",
        "The imported IDB prototype incorrectly returned int and erased all "
        "argument types. The decorated name, RET 0x0c and body accesses "
        "confirm the public void member with IVP_Core**, float and contact "
        "arguments. The body also confirms object/core fields through +0xc0.",
    ),
    (
        0x1001CD20,
        "?calc_impulse_to_reduce_energy_level@IVP_Mutual_Energizer@@AAENNNNN@Z",
        "double __thiscall corrected(IVP_Mutual_Energizer *self, double speed_potential, double inverse_mass0, double inverse_mass1, double delta_energy)",
        "Private retained helper. The decorated name and RET 0x20 prove a "
        "thiscall member with four IVP_DOUBLE arguments and x87 double return; "
        "the imported stdcall/int prototype was wrong.",
    ),
    (
        0x1001CD60,
        "?calc_energy_potential@IVP_Mutual_Energizer@@SANNNNNN@Z",
        "double __cdecl corrected(double speed_potential, double mass0, double mass1, double inverse_mass0, double inverse_mass1)",
        "Previously anonymous retained public static calculation. Five "
        "caller-cleaned IVP_DOUBLE arguments and its x87 result prove the "
        "cdecl signature; the pair-energy member calls this body twice.",
    ),
    (
        0x1001CDB0,
        "?init_mutual_energizer@IVP_Mutual_Energizer@@QAEXPAVIVP_Core@@0@Z",
        "void __thiscall corrected(IVP_Mutual_Energizer *self, IVP_Core *core0, IVP_Core *core1)",
        "Previously anonymous retained public initializer. The caller at RVA "
        "0x1D2A0 reserves exactly 0xA0 bytes; RET 8 and the writes at +0x90/"
        "+0x94 prove the two IVP_Core pointer parameters.",
    ),
    (
        0x1001D060,
        "?calc_energy_potential@IVP_Mutual_Energizer@@QAEXXZ",
        "void __thiscall corrected(IVP_Mutual_Energizer *self)",
        "Retained public member. It calls the five-double static calculation "
        "for rotation and translation, then stores results at +0x80/+0x88 "
        "and their sum at +0x98.",
    ),
    (
        0x1001D0E0,
        "?destroy_percent_energy@IVP_Mutual_Energizer@@QAEXN@Z",
        "void __thiscall corrected(IVP_Mutual_Energizer *self, double percent_energy_to_destroy)",
        "Retained public energy-reduction member. RET 8 and x87 argument "
        "loads prove one IVP_DOUBLE parameter; the imported stdcall/int "
        "prototype was wrong.",
    ),
    (
        0x1001D610,
        "?do_simulation_controller@IVP_Friction_Sys_Energy@@UAEXPAVIVP_Event_Sim@@PAV?$IVP_U_Vector@VIVP_Core@@@@@Z",
        "void __thiscall corrected(IVP_Friction_Sys_Energy *self, IVP_Event_Sim *event, IVP_U_Vector *core_list)",
        "Energy-friction controller slot 4. RET 8 confirms two pointer "
        "arguments and every system operation loads the backlink at +0x04.",
    ),
    (
        0x1001D660,
        "?do_simulation_single_friction@IVP_Friction_Sys_Static@@AAEXPAVIVP_Event_Sim@@@Z",
        "void __thiscall corrected(IVP_Friction_Sys_Static *self, IVP_Event_Sim *event)",
        "Private retained single-contact static-friction step. RET 4 and its "
        "caller prove the IVP_Event_Sim pointer parameter.",
    ),
    (
        0x1001D6C0,
        "?do_simulation_controller@IVP_Friction_Sys_Static@@UAEXPAVIVP_Event_Sim@@PAV?$IVP_U_Vector@VIVP_Core@@@@@Z",
        "void __thiscall corrected(IVP_Friction_Sys_Static *self, IVP_Event_Sim *event, IVP_U_Vector *core_list)",
        "Static-friction controller slot 4. It reads the +0x04 owner, chooses "
        "single/system solving and may delete an empty friction system.",
    ),
    (
        0x1001D9A0,
        "?core_is_going_to_be_deleted_event@IVP_Friction_Sys_Static@@UAEXPAVIVP_Core@@@Z",
        "void __thiscall corrected(IVP_Friction_Sys_Static *self, IVP_Core *deleted_core)",
        "Static-friction controller slot 0. It traverses owner pairs and "
        "deletes every contact involving the supplied core; RET 4 confirms "
        "the pointer parameter.",
    ),
    (
        0x1000B080,
        "?get_controller_priority@IVP_Friction_Sys_Energy@@UAE?AW4IVP_CONTROLLER_PRIORITY@@XZ",
        "IVP_CONTROLLER_PRIORITY __thiscall corrected(IVP_Friction_Sys_Energy *self)",
        "Energy-friction controller slot 5 returns the immediate 2000, "
        "matching IVP_CP_ENERGY_FRICTION.",
    ),
    (
        0x1000B0B0,
        "?get_controller_priority@IVP_Friction_Sys_Static@@UAE?AW4IVP_CONTROLLER_PRIORITY@@XZ",
        "IVP_CONTROLLER_PRIORITY __thiscall corrected(IVP_Friction_Sys_Static *self)",
        "Previously anonymous static-friction controller slot 5. XOR EAX,EAX "
        "returns IVP_CP_STATIC_FRICTION (zero).",
    ),
    (
        0x1000B090,
        "ivp_friction_helper_scalar_deleting_destructor",
        "void *__thiscall corrected(IVP_Controller_Independent *self, unsigned int flags)",
        "Folded scalar-deleting destructor shared by the 0x08 static and "
        "energy friction-controller subobjects. It calls the Controller base "
        "destructor and conditionally operator delete; assigning either one "
        "class's decorated name would be misleading.",
    ),
    (
        0x1001C350,
        "??0IVP_Friction_Solver@@QAE@PAVIVP_Friction_System@@PBVIVP_Event_Sim@@@Z",
        "IVP_Friction_Solver *__thiscall corrected(IVP_Friction_Solver *self, IVP_Friction_System *system, const IVP_Event_Sim *event)",
        "Retained complete 0x840-byte friction-solver constructor. It "
        "constructs the matrix, initializes the inline vector at +0x30 and "
        "allocates three double workspaces from transaction memory.",
    ),
    (
        0x10036760,
        "?calc_solver_PSI@IVP_Friction_Solver@@QAEHPAVIVP_Friction_System@@PAH@Z",
        "int __thiscall corrected(IVP_Friction_Solver *self, IVP_Friction_System *system, int *original_position_of_active)",
        "Retained PSI matrix/setup member; RET 8 confirms its two pointer "
        "arguments.",
    ),
    (
        0x100364C0,
        "?do_resulting_pushes@IVP_Friction_Solver@@QAEHPAVIVP_Friction_System@@@Z",
        "int __thiscall corrected(IVP_Friction_Solver *self, IVP_Friction_System *system)",
        "Retained resulting contact-impulse application member; RET 4 "
        "confirms its friction-system argument.",
    ),
    (
        0x1001C070,
        "?ease_friction_pair@IVP_Friction_Solver@@SAXPAVIVP_Friction_Core_Pair@@PAVIVP_U_Memory@@@Z",
        "void __cdecl corrected(IVP_Friction_Core_Pair *pair, IVP_U_Memory *memory)",
        "Retained static cdecl friction-pair easing helper.",
    ),
    (
        0x1001BDF0,
        "?ease_two_mindists@IVP_Friction_Solver@@SAXPAVIVP_Contact_Point@@0PAVIVP_U_Float_Point@@1N@Z",
        "void __cdecl corrected(IVP_Contact_Point *first, IVP_Contact_Point *second, IVP_U_Float_Point *first_difference, IVP_U_Float_Point *second_difference, double ease_factor)",
        "Retained static cdecl two-contact easing helper. The decorated name "
        "and qword stack load confirm the final IVP_DOUBLE argument.",
    ),
    (
        0x10036170,
        "?factor_result_vec@IVP_Friction_Solver@@QAEXXZ",
        "void __thiscall corrected(IVP_Friction_Solver *self)",
        "Retained result-vector scaling member.",
    ),
    (
        0x10036B30,
        "?get_closing_speed_core_i@IVP_Friction_Solver@@SANPBVIVP_Impact_Solver_Long_Term@@HPBVIVP_U_Float_Point@@PAV3@@Z",
        "double __cdecl corrected(const IVP_Impact_Solver_Long_Term *information, int core_index, const IVP_U_Float_Point *rotational_speed, IVP_U_Float_Point *linear_speed)",
        "Out-of-line retail copy of the source-inline closing-speed helper. "
        "The x87 return and four caller-cleaned arguments confirm its ABI.",
    ),
    (
        0x10036190,
        "?normize_constraint_equ@IVP_Friction_Solver@@QAEXXZ",
        "void __thiscall corrected(IVP_Friction_Solver *self)",
        "Retained constraint-equation normalization member, preserving the "
        "historical spelling in the decorated name.",
    ),
    (
        0x100366A0,
        "?setup_coords_mindists@IVP_Friction_Solver@@QAEXPAVIVP_Friction_System@@@Z",
        "void __thiscall corrected(IVP_Friction_Solver *self, IVP_Friction_System *system)",
        "Retained contact-coordinate setup member; RET 4 confirms the owner "
        "system pointer.",
    ),
    (
        0x100362E0,
        "?solve_linear_equation_and_push@IVP_Friction_Solver@@QAEXPAVIVP_Friction_System@@PAHHPAVIVP_U_Memory@@@Z",
        "void __thiscall corrected(IVP_Friction_Solver *self, IVP_Friction_System *system, int *active_is_at_position, int total_actives, IVP_U_Memory *memory)",
        "Retained matrix solve and contact-push member; RET 0x10 confirms four "
        "arguments after this.",
    ),
    (
        0x10036080,
        "?test_gauss_solution_suggestion@IVP_Friction_Solver@@QAE?AW4IVP_RETURN_TYPE@@PANPAHHPAVIVP_U_Memory@@@Z",
        "IVP_RETURN_TYPE __thiscall corrected(IVP_Friction_Solver *self, double *push_results, int *active_is_at_position, int total_actives, IVP_U_Memory *memory)",
        "Retained Gauss candidate validator; RET 0x10 confirms four arguments "
        "and the decorated return type is IVP_RETURN_TYPE.",
    ),
    (
        0x1001C1F0,
        "?ease_friction_forces@IVP_Friction_System@@QAEXXZ",
        "void __thiscall corrected(IVP_Friction_System *self)",
        "Exact decorated retail friction-force easing member.",
    ),
    (
        0x1001C550,
        "?apply_real_friction@IVP_Friction_System@@QAEXPBVIVP_Event_Sim@@@Z",
        "void __thiscall corrected(IVP_Friction_System *self, const IVP_Event_Sim *event)",
        "Exact decorated retail real-friction application member.",
    ),
    (
        0x1001CC90,
        "?remove_energy_gained_by_real_friction@IVP_Friction_System@@QAEXXZ",
        "void __thiscall corrected(IVP_Friction_System *self)",
        "Exact decorated retail system-level gained-energy removal member.",
    ),
    (
        0x1001CCB0,
        "?clear_integrated_anti_energy@IVP_Friction_System@@QAEXXZ",
        "void __thiscall corrected(IVP_Friction_System *self)",
        "Exact decorated retail integrated-energy reset member.",
    ),
    (
        0x100362C0,
        "?get_num_supposed_active_frdists@IVP_Friction_System@@QAEHXZ",
        "int __thiscall corrected(IVP_Friction_System *self)",
        "Exact decorated retail active-contact count query.",
    ),
    (
        0x10036B80,
        "?reorder_mindists_for_complex@IVP_Friction_System@@QAEXXZ",
        "void __thiscall corrected(IVP_Friction_System *self)",
        "Exact decorated retail complex-solver contact reorder member.",
    ),
    (
        0x10036C60,
        "?bubble_sort_dists_importance@IVP_Friction_System@@QAEXXZ",
        "void __thiscall corrected(IVP_Friction_System *self)",
        "Exact decorated retail contact-importance ordering member.",
    ),
    (
        0x10036CB0,
        "?core_is_terminal_in_fs@IVP_Friction_System@@QAE?AW4IVP_BOOL@@PAVIVP_Core@@@Z",
        "IVP_BOOL __thiscall corrected(IVP_Friction_System *self, IVP_Core *core)",
        "Exact decorated retail friction-graph terminal query.",
    ),
    (
        0x10036D10,
        "?static_fr_oversized_matrix_panic@IVP_Friction_System@@QAEXXZ",
        "void __thiscall corrected(IVP_Friction_System *self)",
        "Exact decorated retail oversized static-friction fallback.",
    ),
    (
        0x10036D70,
        "?do_friction_system@IVP_Friction_System@@QAEXPBVIVP_Event_Sim@@@Z",
        "void __thiscall corrected(IVP_Friction_System *self, const IVP_Event_Sim *event)",
        "Exact decorated retail friction-system solve entry.",
    ),
    (
        0x10036EC0,
        "?kinetic_energy_of_hole_frs@IVP_Friction_System@@QAENXZ",
        "double __thiscall corrected(IVP_Friction_System *self)",
        "Exact decorated retail kinetic-energy query; N encodes double.",
    ),
    (
        0x10036F80,
        "?confirm_complex_pushes@IVP_Friction_System@@QAEXXZ",
        "void __thiscall corrected(IVP_Friction_System *self)",
        "Exact decorated retail complex-push commit member.",
    ),
    (
        0x10036FA0,
        "?undo_complex_pushes@IVP_Friction_System@@QAEXXZ",
        "void __thiscall corrected(IVP_Friction_System *self)",
        "Exact decorated retail complex-push rollback member.",
    ),
    (
        0x10036FC0,
        "?get_max_energy_gain@IVP_Friction_System@@QAENXZ",
        "double __thiscall corrected(IVP_Friction_System *self)",
        "Exact decorated retail maximum-energy-gain query; N encodes double.",
    ),
    (
        0x1001C680,
        "?number_of_pair_dists@IVP_Friction_Core_Pair@@QAEHXZ",
        "int __thiscall corrected(IVP_Friction_Core_Pair *self)",
        "Previously anonymous retained vector-count query; the pair contact "
        "vector starts at offset zero and its 16-bit count is at +0x02.",
    ),
    (
        0x1001C690,
        "?del_fr_dist_obj_pairs@IVP_Friction_Core_Pair@@QAEXPAVIVP_Contact_Point@@@Z",
        "void __thiscall corrected(IVP_Friction_Core_Pair *self, IVP_Contact_Point *contact_point)",
        "Previously generic-named retained removal from the pair's offset-zero "
        "contact vector.",
    ),
    (
        0x1001C6F0,
        "?add_fr_pair@IVP_Friction_System@@QAEXPAVIVP_Friction_Core_Pair@@@Z",
        "void __thiscall corrected(IVP_Friction_System *self, IVP_Friction_Core_Pair *pair)",
        "Previously anonymous Ballance pair-vector insertion at +0x34. Unlike "
        "the nearby revision it dispatches no friction-pair listener callback.",
    ),
    (
        0x1001C720,
        "?del_fr_pair@IVP_Friction_System@@QAEXPAVIVP_Friction_Core_Pair@@@Z",
        "void __thiscall corrected(IVP_Friction_System *self, IVP_Friction_Core_Pair *pair)",
        "Previously anonymous Ballance pair-vector removal at +0x34. Unlike "
        "the nearby revision it dispatches no friction-pair listener callback.",
    ),
    (
        0x1000AFE0,
        "?freeze_simulation_core@IVP_Core@@QAEXXZ",
        "void __thiscall corrected(IVP_Core *self)",
        "Freezes one core through the retained stop-movement helper and then "
        "dispatches IVP_Core::fire_event_object_frozen.",
    ),
    (
        0x1000B540,
        "?add_revive_core@IVP_Environment@@AAEXPAVIVP_Core@@@Z",
        "void __thiscall corrected(IVP_Environment *self, IVP_Core *core)",
        "Queues a core once in the environment revive vector at +0x104 and "
        "sets the two-bit is_in_wakeup_vec field in IVP_Core::flags.",
    ),
    (
        0x1000B4D0,
        "?remove_revive_core@IVP_Environment@@AAEXPAVIVP_Core@@@Z",
        "void __thiscall corrected(IVP_Environment *self, IVP_Core *core)",
        "Removes a core from environment +0x104 revive vector and clears the "
        "two-bit is_in_wakeup_vec field in IVP_Core::flags.",
    ),
    (
        0x1000D680,
        "?calc_movement_state@IVP_Core@@QAE?AW4IVP_Movement_Type@@VIVP_Time@@@Z",
        "IVP_Movement_Type __thiscall corrected(IVP_Core *self, IVP_Time current_time)",
        "Retail calm/moving classifier. RET 8 and qword loads prove IVP_Time "
        "is passed by value as an eight-byte scalar wrapper.",
    ),
    (
        0x1000CE20,
        "?stop_physical_movement@IVP_Core@@QAEXXZ",
        "void __thiscall corrected(IVP_Core *self)",
        "Previously anonymous retained core stop path. It clears core motion, "
        "marks every attached real object not simulated, rechecks broadphase "
        "mindists, exits hull simulation and invalidates live caches.",
    ),
    (
        0x1000CEC0,
        "?reset_freeze_check_values@IVP_Core@@QAEXXZ",
        "void __thiscall corrected(IVP_Core *self)",
        "Previously anonymous retained helper. It copies environment current "
        "time at +0x120 into both core calm-reference times at +0x1d8/+0x1e0.",
    ),
    (
        0x1000CF20,
        "?init_core_for_simulation@IVP_Core@@QAEXXZ",
        "void __thiscall corrected(IVP_Core *self)",
        "Previously anonymous retained transition from IVP_MT_NOT_SIM to "
        "IVP_MT_MOVING. It seeds PSI/calm clocks and revives every attached "
        "object's hull and nearby-mindist state.",
    ),
    (
        0x1000CFA0,
        "?synchronize_with_rot_z@IVP_Core@@QAEXXZ",
        "void __thiscall corrected(IVP_Core *self)",
        "Previously anonymous retained impact synchronization path. It saves "
        "the old angular state in sim-unit transaction memory and interpolates "
        "the core transform to environment current time.",
    ),
    (
        0x1000D1A0,
        "?calc_calc@IVP_Core@@QAEXXZ",
        "void __thiscall corrected(IVP_Core *self)",
        "Previously anonymous retained redundant-value update. Field writes "
        "confirm inverse inertia/mass at +0x34..+0x40, the equal-inertia flag "
        "in +0x00 and inverse object diameter at +0x48.",
    ),
    (
        0x1000D2F0,
        "?init@IVP_Core@@IAEXPAVIVP_Real_Object@@@Z",
        "void __thiscall corrected(IVP_Core *self, IVP_Real_Object *object)",
        "Retained protected Core initializer. It clears the complete object, "
        "initializes the embedded object vector, creates a simulation unit and "
        "installs the environment gravity controller.",
    ),
    (
        0x1000D580,
        "??1IVP_Core@@QAE@XZ",
        "void __thiscall corrected(IVP_Core *self)",
        "Retail complete Core destructor. It removes pending wakeup state, "
        "notifies controllers or frees the static friction hash, detaches the "
        "simulation unit and releases both owned vectors plus spin clipping. "
        "The public wrapper must therefore expose controller-vector storage "
        "without adding a second host-side member lifetime.",
    ),
    (
        0x1000D930,
        "?update_exact_mindist_events_of_core@IVP_Core@@QAEXXZ",
        "void __thiscall corrected(IVP_Core *self)",
        "Previously anonymous retained exact-mindist refresh. It stamps core "
        "+0x230 from environment +0x13c and calls each attached object's "
        "protected exact-mindist update.",
    ),
    (
        0x100120B0,
        "?sim_unit_clear_movement_check_values@IVP_Simulation_Unit@@QAEXXZ",
        "void __thiscall corrected(IVP_Simulation_Unit *self)",
        "Walks the simulation-unit core vector at +0x0c and resets each "
        "core's freeze-check history.",
    ),
    (
        0x100120F0,
        "?sim_unit_calc_movement_state@IVP_Simulation_Unit@@QAE?AW4IVP_BOOL@@PAVIVP_Environment@@@Z",
        "IVP_BOOL __thiscall corrected(IVP_Simulation_Unit *self, IVP_Environment *environment)",
        "Classifies every core, freezes a fully calm unit, and moves the unit "
        "to the manager's IVP_MT_NOT_SIM slot.",
    ),
    (
        0x1001C230,
        "??1IVP_Contact_Point@@AAE@XZ",
        "void __thiscall corrected(IVP_Contact_Point *self)",
        "Retail complete contact-point destructor. It publishes friction "
        "deletion callbacks and unlinks both embedded friction synapses; "
        "operator delete remains a separate cdecl call.",
    ),
    (
        0x10022180,
        "?try_to_generate_managed_friction@IVP_Mindist@@QAEPAVIVP_Contact_Point@@PAPAVIVP_Friction_System@@PAW4IVP_BOOL@@PAVIVP_Simulation_Unit@@W44@@Z",
        "IVP_Contact_Point *__thiscall corrected(IVP_Mindist *self, IVP_Friction_System **associated_system, IVP_BOOL *having_new, IVP_Simulation_Unit *simulation_unit_not_destroy, IVP_BOOL call_recalculate_s_vals)",
        "Creates or reuses the managed contact point. The new-contact branch "
        "publishes creation through Environment RVA 0x13bc0 and Cluster "
        "Manager RVA 0xa8a0 before attaching the contact to friction state.",
    ),
    (
        0x1001C570,
        "?fusion_friction_systems@IVP_Friction_System@@QAEXPAV1@@Z",
        "void __thiscall corrected(IVP_Friction_System *self, IVP_Friction_System *second_system)",
        "Retail friction-system fusion used when live material or mass changes "
        "make one core span contacts from more than one system.",
    ),
    (
        0x1001D3D0,
        "?calc_virtual_mass_of_mindist@IVP_Contact_Point@@AAEXXZ",
        "void __thiscall corrected(IVP_Contact_Point *self)",
        "Recalculates contact virtual mass from the current core inverse mass "
        "and inverse rotational inertia values.",
    ),
    (
        0x1001F860,
        "?recalc_friction_s_vals@IVP_Contact_Point@@QAEXXZ",
        "void __thiscall corrected(IVP_Contact_Point *self)",
        "Previously anonymous retained contact refresh. It allocates the "
        "0xe0-byte temporary impact record from sim-unit transaction memory, "
        "stores it at contact +0x40 and recomputes the friction basis.",
    ),
    (
        0x100201B0,
        "?free_mem_transaction@IVP_U_Memory@@AAEXXZ",
        "void __thiscall corrected(IVP_U_Memory *self)",
        "Retained transaction-memory epilogue paired with the inlined signed "
        "16-bit transaction counter at IVP_U_Memory +0x10.",
    ),
    (
        0x10024040,
        "?read_materials_for_contact_situation@IVP_Contact_Point@@AAEXPAVIVP_Impact_Solver_Long_Term@@@Z",
        "void __thiscall corrected(IVP_Contact_Point *self, IVP_Impact_Solver_Long_Term *info)",
        "Reads both contact materials into the current long-term impact record "
        "after the friction basis has been refreshed.",
    ),
    (
        0x1001EE10,
        "?object_to_index@IVP_Object_Callback_Table_Hash@@IAEHPAVIVP_Real_Object@@@Z",
        "int __thiscall corrected(IVP_Object_Callback_Table_Hash *self, IVP_Real_Object *object)",
        "CRC32-style hash of the four-byte x86 object pointer. The compiler "
        "folded the object and collision callback-hash implementations.",
    ),
    (
        0x1001EE50,
        "?compare@IVP_Object_Callback_Table_Hash@@MBE?AW4IVP_BOOL@@PAX0@Z",
        "IVP_BOOL __thiscall corrected(const IVP_Object_Callback_Table_Hash *self, void *left, void *right)",
        "Compares callback-table object pointers at offset zero. The compiler "
        "folded the object and collision callback-hash implementations.",
    ),
    (
        0x10013B80,
        "?fire_event_post_collision@IVP_Environment@@AAEXPAVIVP_Event_Collision@@@Z",
        "void __thiscall corrected(IVP_Environment *self, IVP_Event_Collision *event)",
        "Global listener dispatch: callback bit 0x01, retail vtable slot 0.",
    ),
    (
        0x10013BC0,
        "?fire_event_friction_created@IVP_Environment@@AAEXPAVIVP_Event_Friction@@@Z",
        "void __thiscall corrected(IVP_Environment *self, IVP_Event_Friction *event)",
        "Global contact-creation dispatch: callback bit 0x04, retail vtable slot 2.",
    ),
    (
        0x10013C00,
        "?fire_event_friction_deleted@IVP_Environment@@AAEXPAVIVP_Event_Friction@@@Z",
        "void __thiscall corrected(IVP_Environment *self, IVP_Event_Friction *event)",
        "Global contact-destruction dispatch: callback bit 0x04, retail vtable slot 3.",
    ),
    (
        0x10015CB0,
        "??1IVP_U_Active_Float@@UAE@XZ",
        "void __thiscall corrected(IVP_U_Active_Float *self)",
        "Folded complete Active Float destructor thunk. Its target installs "
        "the vtable at VA 0x10063718 and then destroys IVP_U_Active_Value; "
        "the imported IVP_Template_Surface destructor name was impossible.",
    ),
    (
        0x10015D20,
        "??1IVP_U_Active_Int@@UAE@XZ",
        "void __thiscall corrected(IVP_U_Active_Int *self)",
        "Folded complete Active Int destructor thunk. Its target installs "
        "the vtable at VA 0x10063720 and then destroys IVP_U_Active_Value; "
        "the duplicate imported IVP_Template_Surface destructor was stale.",
    ),
    (
        0x10015FC0,
        "ivp_mindist_settings_static_initializer_thunk",
        "void __cdecl corrected()",
        "Static-initializer thunk for the retail IVP_Mindist_Settings object. "
        "The old imported IVP_Template_Surface destructor name is impossible.",
    ),
    (
        0x1003BF80,
        "??0IVP_Template_Polygon@@QAE@XZ",
        "IVP_Template_Polygon *__thiscall corrected(IVP_Template_Polygon *self)",
        "Exact public polygon-template constructor. It clears all six "
        "count/pointer dwords through +0x14 and returns the 0x18-byte self.",
    ),
    (
        0x1003BFA0,
        "??1IVP_Template_Polygon@@QAE@XZ",
        "void __thiscall corrected(IVP_Template_Polygon *self)",
        "Exact public polygon-template destructor. It deletes lines and "
        "points, then walks the 0x30-byte owned surface array before freeing it.",
    ),
    (
        0x1003C000,
        "??0IVP_Template_Surface@@QAE@XZ",
        "IVP_Template_Surface *__thiscall corrected(IVP_Template_Surface *self)",
        "Exact public surface-template constructor. REP STOSD clears twelve "
        "dwords, proving the Ballance object size is 0x30.",
    ),
    (
        0x1003C020,
        "?close_surface@IVP_Template_Surface@@QAEXXZ",
        "void __thiscall corrected(IVP_Template_Surface *self)",
        "Exact public surface close operation. It frees lines at +0x28 and "
        "deletes revert_line at +0x2c, clearing both pointers.",
    ),
    (
        0x1003C050,
        "?get_surface_index@IVP_Template_Surface@@QAEHXZ",
        "int __thiscall corrected(IVP_Template_Surface *self)",
        "Exact public surface index query. It subtracts templ_poly->surfaces "
        "from self and divides by the verified 0x30-byte element stride.",
    ),
    (
        0x10015FD0,
        "ivp_mindist_settings_static_initializer",
        "void __cdecl corrected()",
        "Loads ECX with VA 0x10075DB0 and tail-calls the "
        "IVP_Mindist_Settings constructor.",
    ),
    (
        0x10015FE0,
        "?set_collision_tolerance@IVP_Mindist_Settings@@QAEXN@Z",
        "void __thiscall corrected(IVP_Mindist_Settings *self, double tolerance)",
        "Decorated name and RET 8 prove a void __thiscall method with one "
        "8-byte IVP_DOUBLE argument; the old int/float-pointer type was wrong.",
    ),
    (
        0x100160C0,
        "??0IVP_Mindist_Settings@@QAE@XZ",
        "IVP_Mindist_Settings *__thiscall corrected(IVP_Mindist_Settings *self)",
        "Retail constructor: applies the default tolerance then returns self.",
    ),
    (
        0x10017140,
        "?recheck_ov_element@IVP_Mindist_Manager@@QAEXPAVIVP_Real_Object@@@Z",
        "void __thiscall corrected(IVP_Mindist_Manager *self, IVP_Real_Object *object)",
        "Retail object broadphase recheck. The environment universe-manager "
        "pointer at +0x2c is dispatched through slot 0 when more objects are needed.",
    ),
    (
        0x1000BE00,
        "??1IVP_Material@@UAE@XZ",
        "void __thiscall corrected(IVP_Material *self)",
        "[BML direct complete destructor] IVP_Material complete destructor. "
        "The public base layer routes this exact body once; it restores the "
        "verified abstract vtable at VA 0x1006344C.",
    ),
    (
        0x1000BE10,
        "??_GIVP_Material@@UAEPAXI@Z",
        "void *__thiscall corrected(IVP_Material *self, unsigned int flags)",
        "IVP_Material scalar deleting destructor, retail vtable slot 5.",
    ),
    (
        0x1000BF00,
        "??0IVP_Material_Simple@@QAE@NN@Z",
        "IVP_Material_Simple *__thiscall corrected(IVP_Material_Simple *self, double friction, double elasticity)",
        "Retail IVP_Material_Simple constructor. Writes the vptr at "
        "VA 0x10063464, clears second_friction_x_enabled at +0x08, and "
        "writes friction/elasticity/adhesion at +0x10/+0x20/+0x28. It "
        "deliberately leaves material_type +0x04 and second_friction_x "
        "+0x18 untouched.",
    ),
    (
        0x1000BF50,
        "??_GIVP_Material_Simple@@UAEPAXI@Z",
        "void *__thiscall corrected(IVP_Material_Simple *self, unsigned int flags)",
        "IVP_Material_Simple scalar deleting destructor, retail vtable slot 5.",
    ),
    (
        0x1000BF70,
        "??1IVP_Material_Simple@@UAE@XZ",
        "void __thiscall corrected(IVP_Material_Simple *self)",
        "IVP_Material_Simple complete destructor. Restores the Simple vptr, "
        "then tail-calls IVP_Material::~IVP_Material.",
    ),
    (
        0x1000BF80,
        "?get_name@IVP_Material_Simple@@UAEPBDXZ",
        "const char *__thiscall corrected(IVP_Material_Simple *self)",
        "Retail IVP_Material_Simple vtable slot 4; returns \"Simple material\".",
    ),
    (
        0x100046A0,
        "?get_controller_priority@PhysicsControllerForce@@UAE?AW4IVP_CONTROLLER_PRIORITY@@XZ",
        "IVP_CONTROLLER_PRIORITY __thiscall corrected(PhysicsControllerForce *self)",
        "PhysicsControllerForce vtable slot 5. The old imported "
        "IVP_Actuator_Force owner was wrong; this returns IVP_CP_ACTUATOR.",
    ),
    (
        0x100046B0,
        "?do_simulation_controller@PhysicsControllerForce@@UAEXPAVIVP_Event_Sim@@PAV?$IVP_U_Vector@VIVP_Core@@@@@Z",
        "void __thiscall corrected(PhysicsControllerForce *self, IVP_Event_Sim *event, IVP_U_Vector *cores)",
        "PhysicsControllerForce vtable slot 4. PhysicsForceCall constructs "
        "the 0x40-byte adapter object and installs the vtable at VA 0x10063240.",
    ),
    (
        0x10004760,
        "?core_is_going_to_be_deleted_event@PhysicsControllerForce@@UAEXPAVIVP_Core@@@Z",
        "void __thiscall corrected(PhysicsControllerForce *self, IVP_Core *core)",
        "PhysicsControllerForce vtable slot 0; removes this independent "
        "controller from the supplied core.",
    ),
    (
        0x10004780,
        "??_GPhysicsControllerForce@@UAEPAXI@Z",
        "void *__thiscall corrected(PhysicsControllerForce *self, unsigned int flags)",
        "PhysicsControllerForce scalar deleting destructor, vtable slot 6.",
    ),
    (
        0x100047A0,
        "??1PhysicsControllerForce@@UAE@XZ",
        "void __thiscall corrected(PhysicsControllerForce *self)",
        "PhysicsControllerForce complete destructor; unregisters its core "
        "controller and restores the IVP_Controller base vptr.",
    ),
    (
        0x10004C20,
        "?reset_time@IVP_Controller@@UAEXVIVP_Time@@@Z",
        "void __thiscall corrected(IVP_Controller *self, IVP_Time time)",
        "Default IVP_Controller reset_time implementation, retail vtable slot 3.",
    ),
    (
        0x10004C30,
        "??_GIVP_Controller@@UAEPAXI@Z",
        "void *__thiscall corrected(IVP_Controller *self, unsigned int flags)",
        "IVP_Controller scalar deleting destructor used by the retail base vtable.",
    ),
    (
        0x10004C50,
        "?get_associated_controlled_cores@IVP_Controller_Independent@@UAEPAV?$IVP_U_Vector@VIVP_Core@@@@XZ",
        "IVP_U_Vector *__thiscall corrected(IVP_Controller_Independent *self)",
        "Shared IVP_Controller_Independent empty-vector implementation. "
        "Its five vtable xrefs disprove the old IVP_Actuator_Force owner.",
    ),
    (
        0x10013E70,
        "??0IVP_Template_Two_Point@@QAE@XZ",
        "IVP_Template_Two_Point *__thiscall corrected(IVP_Template_Two_Point *self)",
        "Retail IVP_Template_Two_Point constructor; clears client_data at "
        "+0x00 and the two anchor pointers at +0x04/+0x08.",
    ),
    (
        0x10014200,
        "??0IVP_Template_Spring@@QAE@XZ",
        "IVP_Template_Spring *__thiscall corrected(IVP_Template_Spring *self)",
        "Retail IVP_Template_Spring constructor; clears the 0x38-byte "
        "Ballance layout and writes break_max_len at offset 0x24.",
    ),
    (
        0x10014220,
        "?fire_event_spring_broken@IVP_Actuator_Spring@@AAEXXZ",
        "void __thiscall corrected(IVP_Actuator_Spring *self)",
        "Walks listeners_spring backwards and invokes event_spring_broken.",
    ),
    (
        0x10014250,
        "??0IVP_Actuator_Spring@@IAE@PAVIVP_Environment@@PAVIVP_Template_Spring@@W4IVP_ACTUATOR_TYPE@@@Z",
        "IVP_Actuator_Spring *__thiscall corrected(IVP_Actuator_Spring *self, IVP_Environment *environment, IVP_Template_Spring *definition, IVP_ACTUATOR_TYPE actuator_type)",
        "Retail protected Spring complete constructor. It owns the complete "
        "Two Point/Actuator/anchor chain and constructs listeners_spring at "
        "+0x90 before installing the eight-slot Spring vtable.",
    ),
    (
        0x10014350,
        "??_GIVP_Actuator_Spring@@UAEPAXI@Z",
        "void *__thiscall corrected(IVP_Actuator_Spring *self, unsigned int flags)",
        "IVP_Actuator_Spring scalar deleting destructor, primary vtable slot 6.",
    ),
    (
        0x10014370,
        "?set_len@IVP_Actuator_Spring@@QAEXN@Z",
        "void __thiscall corrected(IVP_Actuator_Spring *self, double length)",
        "Updates spring_len at offset 0x74 and wakes the actuator on change.",
    ),
    (
        0x10014390,
        "?set_constant@IVP_Actuator_Spring@@QAEXN@Z",
        "void __thiscall corrected(IVP_Actuator_Spring *self, double value)",
        "Retail spring-constant setter; the public argument is an 8-byte IVP_DOUBLE.",
    ),
    (
        0x100143B0,
        "?set_damp@IVP_Actuator_Spring@@QAEXN@Z",
        "void __thiscall corrected(IVP_Actuator_Spring *self, double value)",
        "Retail spring damping setter; the public argument is an 8-byte IVP_DOUBLE.",
    ),
    (
        0x100143D0,
        "?set_rel_pos_damp@IVP_Actuator_Spring@@QAEXN@Z",
        "void __thiscall corrected(IVP_Actuator_Spring *self, double value)",
        "Retail relative-position damping setter.",
    ),
    (
        0x100143F0,
        "??1IVP_Actuator_Spring@@UAE@XZ",
        "void __thiscall corrected(IVP_Actuator_Spring *self)",
        "Destroys listeners_spring then chains to IVP_Actuator_Two_Point.",
    ),
    (
        0x10014440,
        "?active_float_changed@IVP_Actuator_Spring_Active@@EAEXPAVIVP_U_Active_Float@@@Z",
        "void __thiscall corrected(IVP_U_Active_Float_Listener *listener_self, IVP_U_Active_Float *value)",
        "Secondary-base callback. Incoming ECX points at the listener subobject "
        "at full-object offset 0x98; calls the matching Spring setter.",
    ),
    (
        0x100145F0,
        "??_GIVP_Actuator_Spring_Active@@UAEPAXI@Z",
        "void *__thiscall corrected(IVP_Actuator_Spring_Active *self, unsigned int flags)",
        "IVP_Actuator_Spring_Active scalar deleting destructor. The old "
        "IVP_Actuator_Torque_Active name was imported from the wrong class.",
    ),
    (
        0x10014610,
        "??1IVP_Actuator_Spring_Active@@UAE@XZ",
        "void __thiscall corrected(IVP_Actuator_Spring_Active *self)",
        "Removes four active-float dependencies then chains to "
        "IVP_Actuator_Spring::~IVP_Actuator_Spring.",
    ),
    (
        0x100146D0,
        "?do_simulation_controller@IVP_Actuator_Spring@@UAEXPAVIVP_Event_Sim@@PAV?$IVP_U_Vector@VIVP_Core@@@@@Z",
        "void __thiscall corrected(IVP_Actuator_Spring *self, IVP_Event_Sim *event, IVP_U_Vector *controlled_cores)",
        "Retail spring simulation callback. Field accesses end at the 0x8C "
        "exceed-mode value before the listener vector; no later "
        "spring_force_only_on_stretch field or branch exists.",
    ),
    (
        0x10013270,
        "??1IVP_Environment@@QAE@XZ",
        "void __thiscall corrected(IVP_Environment *self)",
        "Non-virtual IVP_Environment destructor. Both CKIpionManager callers "
        "invoke it directly and then call physics_RT operator delete; the old "
        "virtual-destructor decoration was wrong.",
    ),
    (
        0x10013F60,
        "??0IVP_Actuator@@QAE@PAVIVP_Environment@@@Z",
        "IVP_Actuator *__thiscall corrected(IVP_Actuator *self, IVP_Environment *environment)",
        "Retail IVP_Actuator constructor. Initializes the controlled-core "
        "vector fields at offsets 0x04/0x06/0x08 and installs the vtable "
        "at VA 0x100635C0. The Two_Point constructor begins derived storage "
        "at 0x0C, confirming sizeof(IVP_Controller)=0x04 and "
        "sizeof(IVP_Actuator)=0x0C.",
    ),
    (
        0x10013FA0,
        "?get_associated_controlled_cores@IVP_Actuator@@UAEPAV?$IVP_U_Vector@VIVP_Core@@@@XZ",
        "IVP_U_Vector *__thiscall corrected(IVP_Actuator *self)",
        "IVP_Actuator vtable slot 2. Returns the controlled-core vector at "
        "object offset 4; shared by the Two_Point and Spring vtables.",
    ),
    (
        0x10013FB0,
        "?get_controller_priority@IVP_Actuator@@UAE?AW4IVP_CONTROLLER_PRIORITY@@XZ",
        "IVP_CONTROLLER_PRIORITY __thiscall corrected(IVP_Actuator *self)",
        "IVP_Actuator vtable slot 5. Returns 1500, the retail "
        "IVP_CP_ACTUATOR value.",
    ),
    (
        0x10013FC0,
        "??_GIVP_Actuator@@UAEPAXI@Z",
        "void *__thiscall corrected(IVP_Actuator *self, unsigned int flags)",
        "IVP_Actuator scalar deleting destructor, retail base-vtable slot 6.",
    ),
    (
        0x10013FE0,
        "??1IVP_Actuator@@UAE@XZ",
        "void __thiscall corrected(IVP_Actuator *self)",
        "IVP_Actuator complete destructor. Destroys the controlled-core "
        "vector and restores the IVP_Controller base vptr.",
    ),
    (
        0x10014020,
        "??0IVP_Actuator_Two_Point@@QAE@PAVIVP_Environment@@PAVIVP_Template_Two_Point@@W4IVP_ACTUATOR_TYPE@@@Z",
        "IVP_Actuator_Two_Point *__thiscall corrected(IVP_Actuator_Two_Point *self, IVP_Environment *environment, IVP_Template_Two_Point *definition, IVP_ACTUATOR_TYPE actuator_type)",
        "Retail two-point constructor. Builds two 0x30-byte anchors at "
        "offsets 0x0C and 0x3C, stores client_data at 0x6C, and installs "
        "the vtable at VA 0x100635E0. Derived actuator fields start at "
        "0x70, confirming sizeof(IVP_Actuator_Two_Point)=0x70.",
    ),
    (
        0x10014130,
        "??_GIVP_Actuator_Two_Point@@UAEPAXI@Z",
        "void *__thiscall corrected(IVP_Actuator_Two_Point *self, unsigned int flags)",
        "IVP_Actuator_Two_Point scalar deleting destructor, vtable slot 6.",
    ),
    (
        0x10014150,
        "??1IVP_Actuator_Two_Point@@UAE@XZ",
        "void __thiscall corrected(IVP_Actuator_Two_Point *self)",
        "IVP_Actuator_Two_Point complete destructor. Unregisters the "
        "controller, destroys both anchors, then chains to IVP_Actuator.",
    ),
    (
        0x100141C0,
        "?ensure_actuator_in_simulation@IVP_Actuator_Two_Point@@QAEXXZ",
        "void __thiscall corrected(IVP_Actuator_Two_Point *self)",
        "Retail wake helper used by Spring setters. It follows anchor[0]'s "
        "object to the environment controller manager and requests simulation "
        "for this dependent controller.",
    ),
    (
        0x10010880,
        "??0IVP_Template_Phantom@@QAE@XZ",
        "IVP_Template_Phantom *__thiscall corrected(IVP_Template_Phantom *self)",
        "Retail phantom-template constructor; initializes the complete 0x14 "
        "Ballance trigger-policy layout.",
    ),
    (
        0x10012570,
        "??0IVP_Template_Constraint@@QAE@XZ",
        "IVP_Template_Constraint *__thiscall corrected(IVP_Template_Constraint *self)",
        "Retail constraint-template constructor; initializes the complete "
        "0x200 Ballance creation layout.",
    ),
    (
        0x10012870,
        "?set_constraint_ws@IVP_Template_Constraint@@QAEXPAVIVP_Real_Object@@PBVIVP_U_Point@@1II0PBVIVP_U_Matrix@@@Z",
        "void __thiscall corrected(IVP_Template_Constraint *self, IVP_Real_Object *reference_object, const IVP_U_Point *anchor_ws, const IVP_U_Point *known_axis_ws, unsigned int fixed_translation_dimensions, unsigned int fixed_rotation_dimensions, IVP_Real_Object *attached_object, const IVP_U_Matrix *attached_displacement)",
        "Retail world-space constraint-template configurator. The decorated "
        "name confirms two unsigned dimension counts and the final matrix.",
    ),
    (
        0x10015E90,
        "??0IVP_Template_Object@@QAE@XZ",
        "IVP_Template_Object *__thiscall corrected(IVP_Template_Object *self)",
        "Retail object-template constructor; clears the owned name pointer.",
    ),
    (
        0x100107D0,
        "?calc_liquid_surface@IVP_Liquid_Surface_Descriptor_Simple@@UAEXPAVIVP_Environment@@PAVIVP_Core@@PAVIVP_U_Float_Hesse@@PAVIVP_U_Float_Point@@@Z",
        "void __thiscall corrected(IVP_Liquid_Surface_Descriptor_Simple *self, IVP_Environment *environment, IVP_Core *core, IVP_U_Float_Hesse *surface_normal_out, IVP_U_Float_Point *absolute_current_speed_out)",
        "Retail liquid descriptor virtual; copies the stored surface plane "
        "and absolute current into caller outputs.",
    ),
    (
        0x10011B10,
        "?add_controller_to_core@IVP_Controller_Manager@@SAXPAVIVP_Controller_Independent@@PAVIVP_Core@@@Z",
        "void __cdecl corrected(IVP_Controller_Independent *controller, IVP_Core *core)",
        "Retail static controller-manager helper adding an independent "
        "controller to one Core.",
    ),
    (
        0x10011B20,
        "?remove_controller_from_core@IVP_Controller_Manager@@SAXPAVIVP_Controller_Independent@@PAVIVP_Core@@@Z",
        "void __cdecl corrected(IVP_Controller_Independent *controller, IVP_Core *core)",
        "Retail static controller-manager helper removing an independent "
        "controller from one Core.",
    ),
    (
        0x100346E0,
        "?init_and_solve_lc@IVP_Linear_Constraint_Solver@@QAE?AW4IVP_RETURN_TYPE@@PAN00HHPAVIVP_U_Memory@@@Z",
        "IVP_RETURN_TYPE __thiscall corrected(IVP_Linear_Constraint_Solver *self, double *matrix, double *desired, double *result, int variable_count, int active_count, IVP_U_Memory *memory)",
        "Retail linear-constraint solver entry; the three PAN parameters are "
        "double buffers followed by two counts and transaction memory.",
    ),
    (
        0x10010630,
        "?get_parameters_per_core@IVP_Attacher_To_Cores_Buoyancy@@UAEPAVIVP_Template_Buoyancy@@PAVIVP_Core@@@Z",
        "IVP_Template_Buoyancy *__thiscall corrected(IVP_Attacher_To_Cores_Buoyancy *self, IVP_Core *core)",
        "Retail buoyancy-attacher virtual returning the per-Core parameter "
        "template.",
    ),
    (
        0x10010640,
        "?get_buoyancy_surface@IVP_Attacher_To_Cores_Buoyancy@@UAEPAVIVP_SurfaceManager@@PAVIVP_Real_Object@@@Z",
        "IVP_SurfaceManager *__thiscall corrected(IVP_Attacher_To_Cores_Buoyancy *self, IVP_Real_Object *object)",
        "Retail buoyancy-attacher virtual returning the object's surface "
        "manager.",
    ),
    (
        0x10015D80,
        "?set_double@IVP_U_Active_Terminal_Double@@UAEXNW4IVP_BOOL@@@Z",
        "void __thiscall corrected(IVP_U_Active_Terminal_Double *self, double value, IVP_BOOL delayed)",
        "Retail active-double terminal setter; exact decorated N parameter "
        "confirms the 64-bit value ABI.",
    ),
    (
        0x10015D30,
        "?update_float@IVP_U_Active_Terminal_Double@@UAEXXZ",
        "void __thiscall corrected(IVP_U_Active_Float_Delayed *self)",
        "Retail active-double terminal notification dispatch. This retained "
        "vtable implementation receives the adjusted secondary-base pointer "
        "at complete-object +0x28; it reads current value at self-0x08 and "
        "old value at self+0x08 before subtracting 0x28 for update_derived.",
    ),
    (
        0x10015DF0,
        "?set_int@IVP_U_Active_Terminal_Int@@UAEXHW4IVP_BOOL@@@Z",
        "void __thiscall corrected(IVP_U_Active_Terminal_Int *self, int value, IVP_BOOL delayed)",
        "Retail active-int terminal setter and delayed-update selector.",
    ),
    (
        0x10015D60,
        "?update_int@IVP_U_Active_Terminal_Int@@UAEXXZ",
        "void __thiscall corrected(IVP_U_Active_Int_Delayed *self)",
        "Retail active-int terminal notification dispatch. This retained "
        "vtable implementation receives the adjusted secondary-base pointer "
        "at complete-object +0x20; it reads current value at self-0x04 and "
        "old value at self+0x04 before subtracting 0x20 for update_derived.",
    ),
    (
        0x1000C830,
        "?async_push_core@IVP_Core@@QAEXPBVIVP_U_Float_Point@@00@Z",
        "void __thiscall corrected(IVP_Core *self, const IVP_U_Float_Point *point, const IVP_U_Float_Point *impulse, const IVP_U_Float_Point *angular_impulse)",
        "Retail asynchronous Core impulse entry with point, linear impulse "
        "and angular impulse vectors.",
    ),
    (
        0x1000E8C0,
        "?normize@IVP_U_Float_Hesse@@QAEXXZ",
        "void __thiscall corrected(IVP_U_Float_Hesse *self)",
        "Retail in-place normalization of the four-float Hesse plane.",
    ),
    (
        0x1000F830,
        "?shift_os@IVP_U_Matrix@@QAEXPBVIVP_U_Point@@@Z",
        "void __thiscall corrected(IVP_U_Matrix *self, const IVP_U_Point *shift)",
        "Retail object-space translation of a rigid transform.",
    ),
    (
        0x1000FD30,
        "?add@IVP_U_Float_Point@@QAEXPBV1@0@Z",
        "void __thiscall corrected(IVP_U_Float_Point *self, const IVP_U_Float_Point *left, const IVP_U_Float_Point *right)",
        "Retail component-wise float-point addition.",
    ),
    (
        0x1001B060,
        "?dot_product@IVP_U_Float_Point@@QBENPBV1@@Z",
        "double __thiscall corrected(const IVP_U_Float_Point *self, const IVP_U_Float_Point *other)",
        "Retail float-point dot product returning IVP_DOUBLE through x87.",
    ),
    (
        0x1001E7A0,
        "?quad_length@IVP_U_Float_Point@@QBENXZ",
        "double __thiscall corrected(const IVP_U_Float_Point *self)",
        "Retail squared length of a float point, returned as IVP_DOUBLE.",
    ),
    (
        0x100107B0,
        "?set@IVP_U_Float_Point@@QAEXMMM@Z",
        "void __thiscall corrected(IVP_U_Float_Point *self, float x, float y, float z)",
        "Retail three-component float-point setter.",
    ),
    (
        0x1000FD00,
        "?set@IVP_U_Float_Point@@QAEXPBV1@@Z",
        "void __thiscall corrected(IVP_U_Float_Point *self, const IVP_U_Float_Point *source)",
        "Retail float-point copy setter.",
    ),
    (
        0x10036B00,
        "?set_negative@IVP_U_Float_Point@@QAEXPBV1@@Z",
        "void __thiscall corrected(IVP_U_Float_Point *self, const IVP_U_Float_Point *source)",
        "Retail component-wise float-point negation.",
    ),
    (
        0x1001D020,
        "?set_pairwise_mult@IVP_U_Float_Point@@QAEXPBV1@0@Z",
        "void __thiscall corrected(IVP_U_Float_Point *self, const IVP_U_Float_Point *left, const IVP_U_Float_Point *right)",
        "Retail component-wise float-point multiplication.",
    ),
    (
        0x100102D0,
        "?subtract@IVP_U_Float_Point@@QAEXPBV1@0@Z",
        "void __thiscall corrected(IVP_U_Float_Point *self, const IVP_U_Float_Point *left, const IVP_U_Float_Point *right)",
        "Retail component-wise float-point subtraction.",
    ),
    (
        0x1000E8A0,
        "?calc_hesse_val@IVP_U_Float_Hesse@@QAEXPBVIVP_U_Float_Point@@@Z",
        "void __thiscall corrected(IVP_U_Float_Hesse *self, const IVP_U_Float_Point *point)",
        "Retail Hesse constant calculation from a point on the plane.",
    ),
    (
        0x1000E770,
        "?proj_on_plane@IVP_U_Float_Hesse@@QBEXPBVIVP_U_Float_Point@@PAV2@@Z",
        "void __thiscall corrected(const IVP_U_Float_Hesse *self, const IVP_U_Float_Point *point, IVP_U_Float_Point *result)",
        "Retail projection of a float point onto the Hesse plane.",
    ),
    (
        0x1000F920,
        "?set_col@IVP_U_Matrix3@@QAEXW4IVP_COORDINATE_INDEX@@PBVIVP_U_Point@@@Z",
        "void __thiscall corrected(IVP_U_Matrix3 *self, IVP_COORDINATE_INDEX column, const IVP_U_Point *value)",
        "Retail matrix-column writer using the Ballance coordinate enum.",
    ),
    (
        0x10027B30,
        "?inline_set_vert_to_area_defined_by_three_points@IVP_U_Point@@QAEXPBVIVP_U_Float_Point@@00@Z",
        "void __thiscall corrected(IVP_U_Point *self, const IVP_U_Float_Point *first, const IVP_U_Float_Point *second, const IVP_U_Float_Point *third)",
        "Retail double-precision area normal built from three float points.",
    ),
    (
        0x1000EDA0,
        "?set@IVP_U_Point@@QAEXPBV1@@Z",
        "void __thiscall corrected(IVP_U_Point *self, const IVP_U_Point *source)",
        "Retail double-point copy setter including the aligned fourth lane.",
    ),
    (
        0x1000EAC0,
        "?set_negative@IVP_U_Point@@QAEXPBV1@@Z",
        "void __thiscall corrected(IVP_U_Point *self, const IVP_U_Point *source)",
        "Retail component-wise double-point negation.",
    ),
    (
        0x10027B10,
        "?dot_product@IVP_U_Point@@QBENPBVIVP_U_Float_Point@@@Z",
        "double __thiscall corrected(const IVP_U_Point *self, const IVP_U_Float_Point *other)",
        "Retail mixed double/float dot product returning IVP_DOUBLE.",
    ),
    (
        0x10010720,
        "?element_removed@?$IVP_Attacher_To_Cores@VIVP_Controller_Buoyancy@@@@MAEXPAV?$IVP_U_Set_Active@VIVP_Core@@@@PAVIVP_Core@@@Z",
        "void __thiscall corrected(IVP_Attacher_To_Cores_Buoyancy *self, void *active_core_set, IVP_Core *core)",
        "Retail IVP_Attacher_To_Cores<IVP_Controller_Buoyancy>::element_removed "
        "specialization. The concrete derived this pointer is offset zero; "
        "active_core_set is IVP_U_Set_Active<IVP_Core>*, kept opaque only "
        "because IDA's legacy declaration parser rejects template spelling.",
    ),
    (
        0x10010740,
        "?pset_is_going_to_be_deleted@?$IVP_Attacher_To_Cores@VIVP_Controller_Buoyancy@@@@MAEXPAV?$IVP_U_Set_Active@VIVP_Core@@@@@Z",
        "void __thiscall corrected(IVP_Attacher_To_Cores_Buoyancy *self, void *active_core_set)",
        "Retail IVP_Attacher_To_Cores<IVP_Controller_Buoyancy>::pset_is_going_"
        "to_be_deleted specialization. The argument is "
        "IVP_U_Set_Active<IVP_Core>*; only the IDA parser-facing prototype "
        "uses void* while the decorated name and comment preserve its type.",
    ),
    (
        0x1000E280,
        "?calc_cross_product@IVP_U_Point@@QAEXPBV1@0@Z",
        "void __thiscall corrected(IVP_U_Point *self, const IVP_U_Point *left, const IVP_U_Point *right)",
        "Retail double-point cross product.",
    ),
    (
        0x10019540,
        "?fast_normize_quat@IVP_U_Quat@@QAEXXZ",
        "void __thiscall corrected(IVP_U_Quat *self)",
        "Retail fast in-place quaternion normalization.",
    ),
    (
        0x1000EB30,
        "?init3@IVP_U_Matrix3@@QAEXXZ",
        "void __thiscall corrected(IVP_U_Matrix3 *self)",
        "Retail 3x3 identity initialization.",
    ),
    (
        0x1000EB60,
        "?init_normized3_col@IVP_U_Matrix3@@QAEXPBVIVP_U_Point@@W4IVP_COORDINATE_INDEX@@@Z",
        "void __thiscall corrected(IVP_U_Matrix3 *self, const IVP_U_Point *column_value, IVP_COORDINATE_INDEX column)",
        "Retail normalized-basis construction around a selected matrix column.",
    ),
    (
        0x1000E4F0,
        "?line_max@IVP_U_Point@@QAEXPBV1@@Z",
        "void __thiscall corrected(IVP_U_Point *self, const IVP_U_Point *other)",
        "Retail component-wise double-point maximum.",
    ),
    (
        0x1000E4B0,
        "?line_min@IVP_U_Point@@QAEXPBV1@@Z",
        "void __thiscall corrected(IVP_U_Point *self, const IVP_U_Point *other)",
        "Retail component-wise double-point minimum.",
    ),
    (
        0x1000F140,
        "?mi2mult3@IVP_U_Matrix3@@QBEXPBV1@PAV1@@Z",
        "void __thiscall corrected(const IVP_U_Matrix3 *self, const IVP_U_Matrix3 *other, IVP_U_Matrix3 *result)",
        "Retail inverse-self times inverse-other 3x3 composition.",
    ),
    (
        0x1000F0E0,
        "?mi2mult4@IVP_U_Matrix@@QBEXPBV1@PAV1@@Z",
        "void __thiscall corrected(const IVP_U_Matrix *self, const IVP_U_Matrix *other, IVP_U_Matrix *result)",
        "Retail inverse-self times inverse-other rigid-transform composition.",
    ),
    (
        0x1000EFE0,
        "?mimult3@IVP_U_Matrix3@@QBEXPBV1@PAV1@@Z",
        "void __thiscall corrected(const IVP_U_Matrix3 *self, const IVP_U_Matrix3 *other, IVP_U_Matrix3 *result)",
        "Retail inverse-self times other 3x3 composition.",
    ),
    (
        0x1000EDE0,
        "?mimult4@IVP_U_Matrix@@QBEXPBV1@PAV1@@Z",
        "void __thiscall corrected(const IVP_U_Matrix *self, const IVP_U_Matrix *other, IVP_U_Matrix *result)",
        "Retail inverse-self times other rigid-transform composition.",
    ),
    (
        0x1000EF10,
        "?mmult3@IVP_U_Matrix3@@QBEXPBV1@PAV1@@Z",
        "void __thiscall corrected(const IVP_U_Matrix3 *self, const IVP_U_Matrix3 *other, IVP_U_Matrix3 *result)",
        "Retail forward 3x3 matrix composition.",
    ),
    (
        0x1000EC90,
        "?mmult4@IVP_U_Matrix@@QBEXPBV1@PAV1@@Z",
        "void __thiscall corrected(const IVP_U_Matrix *self, const IVP_U_Matrix *other, IVP_U_Matrix *result)",
        "Retail forward rigid-transform composition.",
    ),
    (
        0x1000E740,
        "?normize@IVP_U_Hesse@@QAEXXZ",
        "void __thiscall corrected(IVP_U_Hesse *self)",
        "Retail in-place normalization of the double Hesse plane.",
    ),
    (
        0x1000DF30,
        "?real_length_plus_normize@IVP_U_Float_Point@@QAENXZ",
        "double __thiscall corrected(IVP_U_Float_Point *self)",
        "Retail float-point length return followed by in-place normalization.",
    ),
    (
        0x1000DE30,
        "?real_length_plus_normize@IVP_U_Point@@QAENXZ",
        "double __thiscall corrected(IVP_U_Point *self)",
        "Retail double-point length return followed by in-place normalization.",
    ),
    (
        0x10019620,
        "?set_invert_mult@IVP_U_Quat@@QAEXPBV1@0@Z",
        "void __thiscall corrected(IVP_U_Quat *self, const IVP_U_Quat *left, const IVP_U_Quat *right)",
        "Retail inverse-left times right quaternion composition.",
    ),
    (
        0x100195F0,
        "?set_invert_unit_quat@IVP_U_Quat@@QAEXPBV1@@Z",
        "void __thiscall corrected(IVP_U_Quat *self, const IVP_U_Quat *source)",
        "Retail conjugate of a unit quaternion.",
    ),
    (
        0x100190C0,
        "?set_matrix@IVP_U_Quat@@QBEXPAVIVP_U_Matrix3@@@Z",
        "void __thiscall corrected(const IVP_U_Quat *self, IVP_U_Matrix3 *result)",
        "Retail quaternion-to-3x3-matrix conversion.",
    ),
    (
        0x1001E7C0,
        "?set_mult_quat@IVP_U_Quat@@QAEXPBV1@0@Z",
        "void __thiscall corrected(IVP_U_Quat *self, const IVP_U_Quat *left, const IVP_U_Quat *right)",
        "Retail quaternion product.",
    ),
    (
        0x1000E220,
        "?set_orthogonal_part@IVP_U_Float_Point@@QAEXPBV1@0@Z",
        "void __thiscall corrected(IVP_U_Float_Point *self, const IVP_U_Float_Point *vector, const IVP_U_Float_Point *normal)",
        "Retail removal of a float vector's component along a normal.",
    ),
    (
        0x1000F250,
        "?set_transpose3@IVP_U_Matrix3@@QAEXPBV1@@Z",
        "void __thiscall corrected(IVP_U_Matrix3 *self, const IVP_U_Matrix3 *source)",
        "Retail copy-transpose of a 3x3 matrix.",
    ),
    (
        0x1000F2C0,
        "?set_transpose@IVP_U_Matrix@@QAEXPBV1@@Z",
        "void __thiscall corrected(IVP_U_Matrix *self, const IVP_U_Matrix *source)",
        "Retail copy-transpose of a rigid transform.",
    ),
    (
        0x10018CA0,
        "?transform_position_to_object_coords@IVP_Cache_Object@@QBEXPBVIVP_U_Point@@PAV2@@Z",
        "void __thiscall corrected(const IVP_Cache_Object *self, const IVP_U_Point *world_position, IVP_U_Point *object_position)",
        "Retail cached world-to-object position transform.",
    ),
    (
        0x10018E30,
        "?transform_vector_to_object_coords@IVP_Cache_Object@@QBEXPBVIVP_U_Float_Point@@PAV2@@Z",
        "void __thiscall corrected(const IVP_Cache_Object *self, const IVP_U_Float_Point *world_vector, IVP_U_Float_Point *object_vector)",
        "Retail cached world-to-object float-vector transform.",
    ),
    (
        0x10018F10,
        "?transform_vector_to_world_coords@IVP_Cache_Object@@QBEXPBVIVP_U_Float_Point@@PAV2@@Z",
        "void __thiscall corrected(const IVP_Cache_Object *self, const IVP_U_Float_Point *object_vector, IVP_U_Float_Point *world_vector)",
        "Retail cached object-to-world float-vector transform.",
    ),
    (
        0x10018EA0,
        "?transform_vector_to_world_coords@IVP_Cache_Object@@QBEXPBVIVP_U_Point@@PAV2@@Z",
        "void __thiscall corrected(const IVP_Cache_Object *self, const IVP_U_Point *object_vector, IVP_U_Point *world_vector)",
        "Retail cached object-to-world double-vector transform.",
    ),
    (
        0x1000F210,
        "?transpose3@IVP_U_Matrix3@@QAEXXZ",
        "void __thiscall corrected(IVP_U_Matrix3 *self)",
        "Retail in-place 3x3 transpose.",
    ),
    (
        0x1000F450,
        "?vimult4@IVP_U_Matrix@@QBEXPBVIVP_U_Float_Point@@PAV2@@Z",
        "void __thiscall corrected(const IVP_U_Matrix *self, const IVP_U_Float_Point *world_position, IVP_U_Float_Point *object_position)",
        "Retail inverse rigid transform from float position to float position.",
    ),
    (
        0x1000F370,
        "?vimult4@IVP_U_Matrix@@QBEXPBVIVP_U_Point@@PAVIVP_U_Float_Point@@@Z",
        "void __thiscall corrected(const IVP_U_Matrix *self, const IVP_U_Point *world_position, IVP_U_Float_Point *object_position)",
        "Retail inverse rigid transform from double position to float position.",
    ),
    (
        0x1000F690,
        "?vmult3@IVP_U_Matrix3@@QBEXPBVIVP_U_Float_Point@@PAV2@@Z",
        "void __thiscall corrected(const IVP_U_Matrix3 *self, const IVP_U_Float_Point *input, IVP_U_Float_Point *result)",
        "Retail 3x3 transform of a float vector.",
    ),
    (
        0x1000F6F0,
        "?vmult3@IVP_U_Matrix3@@QBEXPBVIVP_U_Point@@PAV2@@Z",
        "void __thiscall corrected(const IVP_U_Matrix3 *self, const IVP_U_Point *input, IVP_U_Point *result)",
        "Retail 3x3 transform of a double vector.",
    ),
    (
        0x1000F4C0,
        "?vmult4@IVP_U_Matrix@@QBEXPBVIVP_U_Float_Point@@PAV2@@Z",
        "void __thiscall corrected(const IVP_U_Matrix *self, const IVP_U_Float_Point *object_position, IVP_U_Float_Point *world_position)",
        "Retail forward rigid transform from float position to float position.",
    ),
    (
        0x1000F5F0,
        "?vmult4@IVP_U_Matrix@@QBEXPBVIVP_U_Float_Point@@PAVIVP_U_Point@@@Z",
        "void __thiscall corrected(const IVP_U_Matrix *self, const IVP_U_Float_Point *object_position, IVP_U_Point *world_position)",
        "Retail forward rigid transform from float position to double position.",
    ),
    (
        0x1000F550,
        "?vmult4@IVP_U_Matrix@@QBEXPBVIVP_U_Point@@PAV2@@Z",
        "void __thiscall corrected(const IVP_U_Matrix *self, const IVP_U_Point *object_position, IVP_U_Point *world_position)",
        "Retail forward rigid transform from double position to double position.",
    ),
    (
        0x10033D80,
        "?matrix_check_unequation_line@IVP_Great_Matrix_Many_Zero@@QAE?AW4IVP_RETURN_TYPE@@H@Z",
        "IVP_RETURN_TYPE __thiscall corrected(IVP_Great_Matrix_Many_Zero *self, int line)",
        "Retail sparse-matrix inequality check; indexes rows with the "
        "aligned_row_len field at offset 0x0C.",
    ),
    (
        0x10033DE0,
        "?add_multiple_line@IVP_Great_Matrix_Many_Zero@@QAEXHHN@Z",
        "void __thiscall corrected(IVP_Great_Matrix_Many_Zero *self, int source_line, int destination_line, double factor)",
        "Retail sparse-matrix row operation with an 8-byte factor.",
    ),
    (
        0x10033E50,
        "?solve_great_matrix_many_zero@IVP_Great_Matrix_Many_Zero@@QAE?AW4IVP_RETURN_TYPE@@XZ",
        "IVP_RETURN_TYPE __thiscall corrected(IVP_Great_Matrix_Many_Zero *self)",
        "Retail sparse-matrix solve entry.",
    ),
    (
        0x10033E70,
        "?align_matrix_values@IVP_Great_Matrix_Many_Zero@@QAEXXZ",
        "void __thiscall corrected(IVP_Great_Matrix_Many_Zero *self)",
        "Retail x86 alignment body masks matrix_values to an 8-byte "
        "boundary; it does not round aligned_row_len.",
    ),
    (
        0x10033E80,
        "?find_pivot_in_column@IVP_Great_Matrix_Many_Zero@@QAEXH@Z",
        "void __thiscall corrected(IVP_Great_Matrix_Many_Zero *self, int column)",
        "Retail sparse-matrix pivot search.",
    ),
    (
        0x10033F00,
        "?exchange_rows@IVP_Great_Matrix_Many_Zero@@QAEXHH@Z",
        "void __thiscall corrected(IVP_Great_Matrix_Many_Zero *self, int first, int second)",
        "Retail sparse-matrix row exchange.",
    ),
    (
        0x10033F80,
        "?transform_to_lower_null_triangle@IVP_Great_Matrix_Many_Zero@@QAEXXZ",
        "void __thiscall corrected(IVP_Great_Matrix_Many_Zero *self)",
        "Retail sparse-matrix lower-triangle transform.",
    ),
    (
        0x10034030,
        "?solve_lower_null_matrix@IVP_Great_Matrix_Many_Zero@@QAE?AW4IVP_RETURN_TYPE@@XZ",
        "IVP_RETURN_TYPE __thiscall corrected(IVP_Great_Matrix_Many_Zero *self)",
        "Retail lower-null matrix solve entry.",
    ),
    (
        0x10034100,
        "??0IVP_Great_Matrix_Many_Zero@@QAE@XZ",
        "IVP_Great_Matrix_Many_Zero *__thiscall corrected(IVP_Great_Matrix_Many_Zero *self)",
        "Retail 0x20-byte sparse-matrix constructor. Clears columns and "
        "all three buffer pointers and sets MATRIX_EPS.",
    ),
    (
        0x10034120,
        "?fill_from_bigger_matrix@IVP_Great_Matrix_Many_Zero@@QAEXPAV1@PAHH@Z",
        "void __thiscall corrected(IVP_Great_Matrix_Many_Zero *self, IVP_Great_Matrix_Many_Zero *big_matrix, int *original_positions, int column_count)",
        "Retail sparse submatrix copy using both objects' aligned row widths.",
    ),
    (
        0x10034270,
        "?mult_aligned@IVP_Great_Matrix_Many_Zero@@QAEXXZ",
        "void __thiscall corrected(IVP_Great_Matrix_Many_Zero *self)",
        "Retail aligned sparse-matrix/vector multiplication.",
    ),
    (
        0x10035780,
        "?normize_row_L@IVP_Incr_L_U_Matrix@@QAE?AW4IVP_RETURN_TYPE@@H@Z",
        "IVP_RETURN_TYPE __thiscall corrected(IVP_Incr_L_U_Matrix *self, int row)",
        "Retail incremental-LU L-row normalization; uses the verified 0x30-byte state layout.",
    ),
    (
        0x100357F0,
        "?normize_row@IVP_Incr_L_U_Matrix@@QAE?AW4IVP_RETURN_TYPE@@H@Z",
        "IVP_RETURN_TYPE __thiscall corrected(IVP_Incr_L_U_Matrix *self, int row)",
        "Retail incremental-LU U-row normalization.",
    ),
    (
        0x100358A0,
        "?exchange_rows_l_u@IVP_Incr_L_U_Matrix@@QAEXHH@Z",
        "void __thiscall corrected(IVP_Incr_L_U_Matrix *self, int pivot_column, int exchange_row)",
        "Retail incremental-LU row exchange across the L and U buffers.",
    ),
    (
        0x10035950,
        "?pivot_search_l_u@IVP_Incr_L_U_Matrix@@QAEXH@Z",
        "void __thiscall corrected(IVP_Incr_L_U_Matrix *self, int column)",
        "Retail incremental-LU pivot search.",
    ),
    (
        0x100359D0,
        "?add_neg_row_to_row_l_u@IVP_Incr_L_U_Matrix@@QAEXHHN@Z",
        "void __thiscall corrected(IVP_Incr_L_U_Matrix *self, int pivot_row, int destination_row, double factor)",
        "Retail incremental-LU downward row elimination with an 8-byte factor.",
    ),
    (
        0x10035A80,
        "?subtract_row_L@IVP_Incr_L_U_Matrix@@QAEXHHN@Z",
        "void __thiscall corrected(IVP_Incr_L_U_Matrix *self, int source_row, int destination_row, double factor)",
        "Retail L-buffer row subtraction.",
    ),
    (
        0x10035AE0,
        "?l_u_decomposition_with_pivoting@IVP_Incr_L_U_Matrix@@QAE?AW4IVP_RETURN_TYPE@@XZ",
        "IVP_RETURN_TYPE __thiscall corrected(IVP_Incr_L_U_Matrix *self)",
        "Retail incremental-LU decomposition entry. The body is x87-only and "
        "assumes the Ballance physics-step invariant of an empty x87 register "
        "stack on entry; standalone hosts must establish an equivalent FPU state.",
    ),
    (
        0x10035BE0,
        "?increment_l_u@IVP_Incr_L_U_Matrix@@QAE?AW4IVP_RETURN_TYPE@@XZ",
        "IVP_RETURN_TYPE __thiscall corrected(IVP_Incr_L_U_Matrix *self)",
        "Retail one-variable incremental-LU expansion.",
    ),
    (
        0x10035D00,
        "?decrement_l_u@IVP_Incr_L_U_Matrix@@QAE?AW4IVP_RETURN_TYPE@@H@Z",
        "IVP_RETURN_TYPE __thiscall corrected(IVP_Incr_L_U_Matrix *self, int deleted_index)",
        "Retail one-variable incremental-LU removal. The linked-out legacy "
        "void delete_row_and_col_l_u API is compatibly reconstructed as a "
        "thin call to this body; exact-DLL testing removes the middle member "
        "of a coupled three-variable system and preserves its 2x2 submatrix.",
    ),
    (
        0x10035E80,
        "?exchange_columns_L@IVP_Incr_L_U_Matrix@@QAEXHH@Z",
        "void __thiscall corrected(IVP_Incr_L_U_Matrix *self, int first_column, int second_column)",
        "Retail L-buffer column exchange. The source-reconstructed public "
        "exchange_columns_l_u convenience method calls this first, followed "
        "by the retained U-buffer exchange at 0x10035EE0.",
    ),
    (
        0x10035EE0,
        "?exchange_columns_U@IVP_Incr_L_U_Matrix@@QAEXHH@Z",
        "void __thiscall corrected(IVP_Incr_L_U_Matrix *self, int first_column, int second_column)",
        "Retail U-buffer column exchange. This is the second retained half "
        "of the source-reconstructed public exchange_columns_l_u operation.",
    ),
    (
        0x10035F40,
        "?mult_vec_with_L@IVP_Incr_L_U_Matrix@@QAEXXZ",
        "void __thiscall corrected(IVP_Incr_L_U_Matrix *self)",
        "Retail incremental-LU L-times-vector step.",
    ),
    (
        0x10035F90,
        "?solve_vec_with_U@IVP_Incr_L_U_Matrix@@QAEXXZ",
        "void __thiscall corrected(IVP_Incr_L_U_Matrix *self)",
        "Retail incremental-LU U back-substitution step.",
    ),
    (
        0x10035FF0,
        "?solve_lin_equ@IVP_Incr_L_U_Matrix@@QAEXXZ",
        "void __thiscall corrected(IVP_Incr_L_U_Matrix *self)",
        "Retail incremental-LU linear-equation solve entry.",
    ),
    (
        0x10036030,
        "?add_neg_row_L@IVP_Incr_L_U_Matrix@@QAEXHHN@Z",
        "void __thiscall corrected(IVP_Incr_L_U_Matrix *self, int source_row, int destination_row, double factor)",
        "Retail L-buffer negative row accumulation.",
    ),
    (
        0x10033A30,
        "?init_reaction_solver_translation_ws@IVP_Solver_Core_Reaction@@QAEXPAVIVP_Core@@0AAVIVP_U_Point@@PAVIVP_U_Float_Point@@22@Z",
        "void __thiscall corrected(IVP_Solver_Core_Reaction *self, IVP_Core *core_0, IVP_Core *core_1, IVP_U_Point *position_ws, IVP_U_Float_Point *direction_0_ws, IVP_U_Float_Point *direction_1_ws, IVP_U_Float_Point *direction_2_ws)",
        "Retail six-argument translation reaction initializer. The source "
        "position reference has pointer ABI; ret 0x18 confirms six stack "
        "arguments after this.",
    ),
    (
        0x10033AD0,
        "?exert_impulse_dim2@IVP_Solver_Core_Reaction@@QAEXPAVIVP_Core@@0AAVIVP_U_Float_Point@@@Z",
        "void __thiscall corrected(IVP_Solver_Core_Reaction *self, IVP_Core *core_0, IVP_Core *core_1, IVP_U_Float_Point *impulse_ds)",
        "Retail two-dimensional reaction impulse application. The source "
        "impulse reference has pointer ABI.",
    ),
    (
        0x10009C40,
        "?get_m_world_f_object_AT@IVP_Real_Object@@QBEXPAVIVP_U_Matrix@@@Z",
        "void __thiscall corrected(const IVP_Real_Object *self, IVP_U_Matrix *matrix)",
        "Retail current object transform query. The const-qualified method "
        "combines the core transform and object/core offset.",
    ),
    (
        0x100129C0,
        "?create_constraint@IVP_Environment@@QAEPAVIVP_Constraint@@PBVIVP_Template_Constraint@@@Z",
        "IVP_Constraint *__thiscall corrected(IVP_Environment *self, const IVP_Template_Constraint *definition)",
        "Retail environment factory. Allocates exactly 0x190 bytes and "
        "constructs IVP_Constraint_Local.",
    ),
    (
        0x10012A60,
        "??0IVP_Environment@@AAE@PAVIVP_Environment_Manager@@PAVIVP_Application_Environment@@PBDI@Z",
        "IVP_Environment *__thiscall corrected(IVP_Environment *self, IVP_Environment_Manager *manager, IVP_Application_Environment *application, const char *customer_name, unsigned int authorization_code)",
        "Retail private environment constructor. The manager factory passes "
        "four stack arguments and the constructor returns self. Stores the "
        "customer string/code/count at +0x10C/+0x110/+0x114 and the manager "
        "backlink at +0x118; this is not the default constructor previously "
        "imported into the database.",
    ),
    (
        0x100137D0,
        "?get_root_cluster@IVP_Environment@@QAEPAVIVP_Cluster@@XZ",
        "IVP_Cluster *__thiscall corrected(IVP_Environment *self)",
        "Retail environment root-cluster query. It loads cluster_manager at "
        "environment + 0x0C and tail-calls the manager's first-field getter.",
    ),
    (
        0x100139A0,
        "?create_polygon@IVP_Environment@@QAEPAVIVP_Polygon@@PAVIVP_SurfaceManager@@PBVIVP_Template_Real_Object@@PBVIVP_U_Quat@@PBVIVP_U_Point@@@Z",
        "IVP_Polygon *__thiscall corrected(IVP_Environment *self, IVP_SurfaceManager *surface_manager, const IVP_Template_Real_Object *object_template, const IVP_U_Quat *rotation, const IVP_U_Point *position)",
        "Retail polygon factory. It allocates 0xB8 bytes, obtains the root "
        "cluster, and forwards the four caller arguments to IVP_Polygon.",
    ),
    (
        0x10013C70,
        "?add_listener_collision_global@IVP_Environment@@QAEXPAVIVP_Listener_Collision@@@Z",
        "void __thiscall corrected(IVP_Environment *self, IVP_Listener_Collision *listener)",
        "Retail global collision-listener append. The vector lives at "
        "environment + 0xF4; duplicate listeners are intentionally allowed.",
    ),
    (
        0x10013A80,
        "?add_listener_object_global@IVP_Environment@@QAEXPAVIVP_Listener_Object@@@Z",
        "void __thiscall corrected(IVP_Environment *self, IVP_Listener_Object *listener)",
        "Retail global object-listener append. The vector lives at "
        "environment + 0x150; duplicate listeners are intentionally allowed.",
    ),
    (
        0x10013AC0,
        "?fire_event_object_created@IVP_Environment@@QAEXPAVIVP_Event_Object@@@Z",
        "void __thiscall corrected(IVP_Environment *self, IVP_Event_Object *event)",
        "Anonymous retained global object-created dispatcher. It traverses "
        "the vector at environment +0x150 in reverse registration order and "
        "invokes IVP_Listener_Object vtable slot 1.",
    ),
    (
        0x10013AF0,
        "?fire_event_object_deleted@IVP_Environment@@QAEXPAVIVP_Event_Object@@@Z",
        "void __thiscall corrected(IVP_Environment *self, IVP_Event_Object *event)",
        "Anonymous retained global object-deleted dispatcher. It traverses "
        "the vector at environment +0x150 in reverse registration order and "
        "invokes IVP_Listener_Object vtable slot 0.",
    ),
    (
        0x10013B20,
        "?fire_event_object_frozen@IVP_Environment@@QAEXPAVIVP_Event_Object@@@Z",
        "void __thiscall corrected(IVP_Environment *self, IVP_Event_Object *event)",
        "Anonymous retained global object-frozen dispatcher. It traverses "
        "the vector at environment +0x150 in reverse registration order and "
        "invokes IVP_Listener_Object vtable slot 3.",
    ),
    (
        0x10013B50,
        "?fire_event_object_revived@IVP_Environment@@QAEXPAVIVP_Event_Object@@@Z",
        "void __thiscall corrected(IVP_Environment *self, IVP_Event_Object *event)",
        "Anonymous retained global object-revived dispatcher. It traverses "
        "the vector at environment +0x150 in reverse registration order and "
        "invokes IVP_Listener_Object vtable slot 2.",
    ),
    (
        0x10013C40,
        "?fire_event_PSI@IVP_Environment@@AAEXXZ",
        "void __thiscall corrected(IVP_Environment *self)",
        "Retained private PSI dispatcher. It builds the one-pointer "
        "IVP_Event_PSI on the stack and invokes each listener's slot 0; the "
        "body leaves no defined integer return value.",
    ),
    (
        0x10013CB0,
        "?simulate_psi@IVP_Environment@@AAEXVIVP_Time@@@Z",
        "void __thiscall corrected(IVP_Environment *self, IVP_Time psi_time)",
        "Retail private PSI body. RET 8 and the source-era decorated signature "
        "identify the two ignored stack dwords as one by-value IVP_Time.",
    ),
    (
        0x100136E0,
        "??0IVP_Environment_Manager@@AAE@XZ",
        "IVP_Environment_Manager *__thiscall corrected(IVP_Environment_Manager *self)",
        "Retail private singleton constructor. It clears the optimization "
        "flag at +0x00 and initializes the complete IVP_U_Vector fields at "
        "+0x04/+0x06/+0x08, proving the manager's 0x0C owner boundary.",
    ),
    (
        0x100137E0,
        "?create_environment@IVP_Environment_Manager@@QAEPAVIVP_Environment@@PAVIVP_Application_Environment@@PBDI@Z",
        "IVP_Environment *__thiscall corrected(IVP_Environment_Manager *self, IVP_Application_Environment *application, const char *customer_name, unsigned int authorization_code)",
        "Retail manager factory. It allocates exactly 0x178 bytes and passes "
        "the manager plus the three caller arguments to the private constructor.",
    ),
    (
        0x100138B0,
        "?get_environment_manager@IVP_Environment_Manager@@SAPAV1@XZ",
        "IVP_Environment_Manager *__cdecl corrected(void)",
        "Retail static singleton getter. It returns VA 0x10075DA0 and has no "
        "hidden this parameter.",
    ),
    (
        0x10013700,
        "?create_spring@IVP_Environment@@QAEPAVIVP_Actuator_Spring@@PAVIVP_Template_Spring@@@Z",
        "IVP_Actuator_Spring *__thiscall corrected(IVP_Environment *self, IVP_Template_Spring *definition)",
        "Retail spring factory. It selects the 0x98-byte passive or 0xAC-byte "
        "active implementation from the four active-value fields at template +0x28.",
    ),
    (
        0x100138C0,
        "?simulate_dtime@IVP_Environment@@QAEXN@Z",
        "void __thiscall corrected(IVP_Environment *self, double delta_time)",
        "Retail relative-time simulation entry. It adds the double delta to "
        "current_time at +0x120 and forwards the resulting IVP_Time to event_loop.",
    ),
    (
        0x10009590,
        "?remove_listener_collision@IVP_Real_Object@@QAEXPAVIVP_Listener_Collision@@@Z",
        "void __thiscall corrected(IVP_Real_Object *self, IVP_Listener_Collision *listener)",
        "Retail per-object collision-listener removal. It tail-forwards object "
        "and listener to the cluster manager; the public method returns void.",
    ),
    (
        0x1000A3E0,
        "?async_push_object_ws@IVP_Real_Object@@QAEXPBVIVP_U_Point@@PBVIVP_U_Float_Point@@@Z",
        "void __thiscall corrected(IVP_Real_Object *self, const IVP_U_Point *position_ws, const IVP_U_Float_Point *impulse_ws)",
        "Retail world-space object impulse path. It wakes the object, transforms "
        "position and impulse into core space, and calls async_push_core.",
    ),
    (
        0x1000B600,
        "?increment_mem@IVP_U_Vector_Base@@QAEXXZ",
        "void __thiscall corrected(IVP_U_Vector_Base *self)",
        "Retail pointer-vector growth body. Capacity is the 16-bit field at "
        "+0x00 and grows as 2*n+1 while preserving inline storage ownership.",
    ),
    (
        0x1000E030,
        "?fast_normize@IVP_U_Float_Point@@QAE?AW4IVP_RETURN_TYPE@@XZ",
        "IVP_RETURN_TYPE __thiscall corrected(IVP_U_Float_Point *self)",
        "Retail approximate float-vector normalization. EAX returns failure or "
        "success while the three float components are scaled in place.",
    ),
    (
        0x1000E2C0,
        "?calc_cross_product@IVP_U_Float_Point@@QAEXPBV1@0@Z",
        "void __thiscall corrected(IVP_U_Float_Point *self, const IVP_U_Float_Point *left, const IVP_U_Float_Point *right)",
        "Retail float-vector cross product. RET 8 confirms the two pointer "
        "arguments and the result is stored in the three floats at self.",
    ),
    (
        0x1000E480,
        "?real_length@IVP_U_Float_Point@@QBENXZ",
        "double __thiscall corrected(const IVP_U_Float_Point *self)",
        "Retail const float-vector length. It accumulates three float squares "
        "and returns their square root through x87 as IVP_DOUBLE.",
    ),
    (
        0x1002F780,
        "?solve_inter_penetration_simple@IVP_Anomaly_Manager@@QAEXPAVIVP_Real_Object@@0@Z",
        "void __thiscall corrected(IVP_Anomaly_Manager *self, IVP_Real_Object *first, IVP_Real_Object *second)",
        "Retail simple penetration recovery. RET 8 confirms the two object "
        "arguments; the body applies opposite center impulses and angular corrections.",
    ),
    (
        0x10018A40,
        "?update_cache_object@IVP_Cache_Object@@QAEXXZ",
        "void __thiscall corrected(IVP_Cache_Object *self)",
        "Retail cache refresh. It stores the environment time code at +0x00 "
        "and updates the interpolated object transform from the owning object at +0x08.",
    ),
    (
        0x10015EA0,
        "??1IVP_Template_Object@@QAE@XZ",
        "void __thiscall corrected(IVP_Template_Object *self)",
        "Retail complete template-object destructor. It releases the optional "
        "name at +0x00 with the physics CRT and nulls the field.",
    ),
    (
        0x10037050,
        "??1IVP_U_Min_Hash@@QAE@XZ",
        "void __thiscall corrected(IVP_U_Min_Hash *self)",
        "Retail complete minimum-hash destructor. It deletes every chained "
        "element, then frees the bucket and auxiliary arrays.",
    ),
    (
        0x100375B0,
        "??0IVP_Constraint@@QAE@XZ",
        "IVP_Constraint *__thiscall corrected(IVP_Constraint *self)",
        "Retail IVP_Constraint base constructor. It installs the 23-slot "
        "base vtable at VA 0x10063B20, constructs the inline two-Core vector "
        "at +0x08/+0x0C/+0x10, and initializes the enabled bits. Public direct "
        "construction and the nested Local path both enter this body once.",
    ),
    (
        0x100375E0,
        "??_GIVP_Constraint@@UAEPAXI@Z",
        "void *__thiscall corrected(IVP_Constraint *self, unsigned int flags)",
        "Retail IVP_Constraint scalar deleting destructor, base-vtable slot 6.",
    ),
    (
        0x10037620,
        "?activate@IVP_Constraint@@QAEXXZ",
        "void __thiscall corrected(IVP_Constraint *self)",
        "Retail base activation path; announces a previously disabled "
        "constraint to its environment.",
    ),
    (
        0x10037650,
        "??1IVP_Constraint@@UAE@XZ",
        "void __thiscall corrected(IVP_Constraint *self)",
        "Retail IVP_Constraint complete destructor. The imported "
        "IVP_Mindist_Base owner is disproved by both base-vptr writes and "
        "the IVP_Constraint_Local destructor chain.",
    ),
    (
        0x100376D0,
        "?change_fixing_point_Ros@IVP_Constraint@@UAEXPBVIVP_U_Point@@@Z",
        "void __thiscall corrected(IVP_Constraint *self, const IVP_U_Point *anchor)",
        "Retail diagnostic default for change_fixing_point_Ros; linker-folded "
        "with change_target_fixing_point_Ros.",
    ),
    (
        0x100376E0,
        "?change_translation_axes_Ros@IVP_Constraint@@UAEXPBVIVP_U_Matrix3@@@Z",
        "void __thiscall corrected(IVP_Constraint *self, const IVP_U_Matrix3 *axes)",
        "Retail diagnostic default for change_translation_axes_Ros; linker-"
        "folded with change_target_translation_axes_Ros.",
    ),
    (
        0x100376F0,
        "?fix_translation_axis@IVP_Constraint@@UAEXW4IVP_COORDINATE_INDEX@@@Z",
        "void __thiscall corrected(IVP_Constraint *self, IVP_COORDINATE_INDEX axis)",
        "Retail IVP_Constraint diagnostic default for fix_translation_axis.",
    ),
    (
        0x10037700,
        "?free_translation_axis@IVP_Constraint@@UAEXW4IVP_COORDINATE_INDEX@@@Z",
        "void __thiscall corrected(IVP_Constraint *self, IVP_COORDINATE_INDEX axis)",
        "Retail IVP_Constraint diagnostic default for free_translation_axis.",
    ),
    (
        0x10037710,
        "?limit_translation_axis@IVP_Constraint@@UAEXW4IVP_COORDINATE_INDEX@@MM@Z",
        "void __thiscall corrected(IVP_Constraint *self, IVP_COORDINATE_INDEX axis, float left, float right)",
        "Retail IVP_Constraint diagnostic default for limit_translation_axis.",
    ),
    (
        0x10037720,
        "?change_max_translation_impulse@IVP_Constraint@@UAEXW4IVP_CONSTRAINT_FORCE_EXCEED@@M@Z",
        "void __thiscall corrected(IVP_Constraint *self, IVP_CONSTRAINT_FORCE_EXCEED exceed_type, float impulse)",
        "Retail diagnostic default for change_max_translation_impulse.",
    ),
    (
        0x10037730,
        "?change_rotation_axes_Ros@IVP_Constraint@@UAEXPBVIVP_U_Matrix3@@@Z",
        "void __thiscall corrected(IVP_Constraint *self, const IVP_U_Matrix3 *axes)",
        "Retail diagnostic default for change_rotation_axes_Ros; linker-"
        "folded with change_target_rotation_axes_Ros.",
    ),
    (
        0x10037740,
        "?fix_rotation_axis@IVP_Constraint@@UAEXW4IVP_COORDINATE_INDEX@@@Z",
        "void __thiscall corrected(IVP_Constraint *self, IVP_COORDINATE_INDEX axis)",
        "Retail IVP_Constraint diagnostic default for fix_rotation_axis.",
    ),
    (
        0x10037750,
        "?free_rotation_axis@IVP_Constraint@@UAEXW4IVP_COORDINATE_INDEX@@@Z",
        "void __thiscall corrected(IVP_Constraint *self, IVP_COORDINATE_INDEX axis)",
        "Retail IVP_Constraint diagnostic default for free_rotation_axis.",
    ),
    (
        0x10037760,
        "?limit_rotation_axis@IVP_Constraint@@UAEXW4IVP_COORDINATE_INDEX@@MM@Z",
        "void __thiscall corrected(IVP_Constraint *self, IVP_COORDINATE_INDEX axis, float left, float right)",
        "Retail IVP_Constraint diagnostic default for limit_rotation_axis.",
    ),
    (
        0x10037770,
        "?change_max_rotation_impulse@IVP_Constraint@@UAEXW4IVP_CONSTRAINT_FORCE_EXCEED@@M@Z",
        "void __thiscall corrected(IVP_Constraint *self, IVP_CONSTRAINT_FORCE_EXCEED exceed_type, float impulse)",
        "Retail diagnostic default for change_max_rotation_impulse.",
    ),
    (
        0x10037780,
        "?change_Aos_to_relaxe_constraint@IVP_Constraint@@UAEXXZ",
        "void __thiscall corrected(IVP_Constraint *self)",
        "Retail diagnostic default for change_Aos_to_relaxe_constraint; "
        "linker-folded with change_Ros_to_relaxe_constraint.",
    ),
    (
        0x10037600,
        "?get_minimum_simulation_frequency@IVP_Constraint@@MAENXZ",
        "double __thiscall corrected(IVP_Constraint *self)",
        "Shared IVP_Constraint minimum-frequency implementation used by "
        "the retail base and Local vtables. Fixed_Keyframed has no retained "
        "vtable or method body in Ballance.",
    ),
    (
        0x10028230,
        "?get_controller_priority@IVP_Constraint@@MAE?AW4IVP_CONTROLLER_PRIORITY@@XZ",
        "IVP_CONTROLLER_PRIORITY __thiscall corrected(IVP_Constraint *self)",
        "Shared IVP_Constraint priority implementation; returns 405.",
    ),
    (
        0x10028240,
        "?get_associated_controlled_cores@IVP_Constraint@@UAEPAV?$IVP_U_Vector@VIVP_Core@@@@XZ",
        "IVP_U_Vector *__thiscall corrected(IVP_Constraint *self)",
        "Returns the embedded controlled-core vector at offset 8. The old "
        "type_info::raw_name import was impossible.",
    ),
    (
        0x10028250,
        "??_GIVP_Constraint_Local@@UAEPAXI@Z",
        "void *__thiscall corrected(IVP_Constraint_Local *self, unsigned int flags)",
        "IVP_Constraint_Local scalar deleting destructor, retail vtable slot 6.",
    ),
    (
        0x10028270,
        "??0IVP_Constraint_Local@@QAE@ABVIVP_Template_Constraint@@@Z",
        "IVP_Constraint_Local *__thiscall corrected(IVP_Constraint_Local *self, const IVP_Template_Constraint *definition)",
        "Retail IVP_Constraint_Local complete constructor. It calls the "
        "Constraint base constructor once, constructs Local Anchors at "
        "+0x70/+0xF8, installs identity mappings at +0x180/+0x183, then calls "
        "the initializer at 0x100284D0 and activate. The old imported second "
        "parameter type was an untyped integer.",
    ),
    (
        0x100288A0,
        "??1IVP_Constraint_Local@@UAE@XZ",
        "void __thiscall corrected(IVP_Constraint_Local *self)",
        "Retail IVP_Constraint_Local complete destructor; releases the "
        "three optional allocation pointers then chains to IVP_Constraint.",
    ),
    (
        0x10028960,
        "?do_simulation_controller@IVP_Constraint_Local@@MAEXPAVIVP_Event_Sim@@PAV?$IVP_U_Vector@VIVP_Core@@@@@Z",
        "void __thiscall corrected(IVP_Constraint_Local *self, IVP_Event_Sim *event, IVP_U_Vector *core_list)",
        "IVP_Constraint_Local vtable slot 4 and retail constraint solver body.",
    ),
    (
        0x1002A210,
        "?change_fixing_point_Ros@IVP_Constraint_Local@@UAEXPBVIVP_U_Point@@@Z",
        "void __thiscall corrected(IVP_Constraint_Local *self, const IVP_U_Point *anchor)",
        "IVP_Constraint_Local vtable slot 7.",
    ),
    (
        0x1002A2E0,
        "?change_target_fixing_point_Ros@IVP_Constraint_Local@@UAEXPBVIVP_U_Point@@@Z",
        "void __thiscall corrected(IVP_Constraint_Local *self, const IVP_U_Point *anchor)",
        "IVP_Constraint_Local vtable slot 8.",
    ),
    (
        0x1002A380,
        "?change_translation_axes_Ros@IVP_Constraint_Local@@UAEXPBVIVP_U_Matrix3@@@Z",
        "void __thiscall corrected(IVP_Constraint_Local *self, const IVP_U_Matrix3 *axes)",
        "IVP_Constraint_Local vtable slot 9.",
    ),
    (
        0x1002A430,
        "?change_target_translation_axes_Ros@IVP_Constraint_Local@@UAEXPBVIVP_U_Matrix3@@@Z",
        "void __thiscall corrected(IVP_Constraint_Local *self, const IVP_U_Matrix3 *axes)",
        "IVP_Constraint_Local vtable slot 10.",
    ),
    (
        0x1002A4A0,
        "?fix_translation_axis@IVP_Constraint_Local@@UAEXW4IVP_COORDINATE_INDEX@@@Z",
        "void __thiscall corrected(IVP_Constraint_Local *self, IVP_COORDINATE_INDEX axis)",
        "IVP_Constraint_Local vtable slot 11.",
    ),
    (
        0x1002A4D0,
        "?free_translation_axis@IVP_Constraint_Local@@UAEXW4IVP_COORDINATE_INDEX@@@Z",
        "void __thiscall corrected(IVP_Constraint_Local *self, IVP_COORDINATE_INDEX axis)",
        "IVP_Constraint_Local vtable slot 12.",
    ),
    (
        0x1002A500,
        "?limit_translation_axis@IVP_Constraint_Local@@UAEXW4IVP_COORDINATE_INDEX@@MM@Z",
        "void __thiscall corrected(IVP_Constraint_Local *self, IVP_COORDINATE_INDEX axis, float left, float right)",
        "IVP_Constraint_Local vtable slot 13; IVP_FLOAT is 32-bit float.",
    ),
    (
        0x1002A540,
        "?change_max_translation_impulse@IVP_Constraint_Local@@UAEXW4IVP_CONSTRAINT_FORCE_EXCEED@@M@Z",
        "void __thiscall corrected(IVP_Constraint_Local *self, IVP_CONSTRAINT_FORCE_EXCEED exceed_type, float impulse)",
        "IVP_Constraint_Local vtable slot 14.",
    ),
    (
        0x1002A5C0,
        "?change_rotation_axes_Ros@IVP_Constraint_Local@@UAEXPBVIVP_U_Matrix3@@@Z",
        "void __thiscall corrected(IVP_Constraint_Local *self, const IVP_U_Matrix3 *axes)",
        "IVP_Constraint_Local vtable slot 15.",
    ),
    (
        0x1002A6C0,
        "?change_target_rotation_axes_Ros@IVP_Constraint_Local@@UAEXPBVIVP_U_Matrix3@@@Z",
        "void __thiscall corrected(IVP_Constraint_Local *self, const IVP_U_Matrix3 *axes)",
        "IVP_Constraint_Local vtable slot 16.",
    ),
    (
        0x1002A760,
        "?fix_rotation_axis@IVP_Constraint_Local@@UAEXW4IVP_COORDINATE_INDEX@@@Z",
        "void __thiscall corrected(IVP_Constraint_Local *self, IVP_COORDINATE_INDEX axis)",
        "IVP_Constraint_Local vtable slot 17.",
    ),
    (
        0x1002A790,
        "?free_rotation_axis@IVP_Constraint_Local@@UAEXW4IVP_COORDINATE_INDEX@@@Z",
        "void __thiscall corrected(IVP_Constraint_Local *self, IVP_COORDINATE_INDEX axis)",
        "IVP_Constraint_Local vtable slot 18.",
    ),
    (
        0x1002A7C0,
        "?limit_rotation_axis@IVP_Constraint_Local@@UAEXW4IVP_COORDINATE_INDEX@@MM@Z",
        "void __thiscall corrected(IVP_Constraint_Local *self, IVP_COORDINATE_INDEX axis, float left, float right)",
        "IVP_Constraint_Local vtable slot 19; IVP_FLOAT is 32-bit float.",
    ),
    (
        0x1002A800,
        "?change_max_rotation_impulse@IVP_Constraint_Local@@UAEXW4IVP_CONSTRAINT_FORCE_EXCEED@@M@Z",
        "void __thiscall corrected(IVP_Constraint_Local *self, IVP_CONSTRAINT_FORCE_EXCEED exceed_type, float impulse)",
        "IVP_Constraint_Local vtable slot 20.",
    ),
    (
        0x1002A880,
        "?change_Aos_to_relaxe_constraint@IVP_Constraint_Local@@UAEXXZ",
        "void __thiscall corrected(IVP_Constraint_Local *self)",
        "IVP_Constraint_Local vtable slot 21.",
    ),
    (
        0x1002A920,
        "?change_Ros_to_relaxe_constraint@IVP_Constraint_Local@@UAEXXZ",
        "void __thiscall corrected(IVP_Constraint_Local *self)",
        "IVP_Constraint_Local vtable slot 22.",
    ),
    (
        0x10002DA0,
        "??_GIVP_SurfaceManager_Polygon@@UAEPAXI@Z",
        "void *__thiscall corrected(IVP_SurfaceManager_Polygon *self, unsigned int flags)",
        "IVP_SurfaceManager_Polygon scalar deleting destructor, retail "
        "vtable slot 9.",
    ),
    (
        0x1000BD00,
        "??1IVP_SurfaceManager_Polygon@@UAE@XZ",
        "void __thiscall corrected(IVP_SurfaceManager_Polygon *self)",
        "IVP_SurfaceManager_Polygon complete destructor. Restores the "
        "Polygon vptr then chains to IVP_SurfaceManager.",
    ),
    (
        0x10022160,
        "??1IVP_SurfaceManager@@UAE@XZ",
        "void __thiscall corrected(IVP_SurfaceManager *self)",
        "[BML direct complete destructor] IVP_SurfaceManager complete "
        "destructor. The public pure base layer routes this exact body once; "
        "it restores VA 0x10063890.",
    ),
    (
        0x1000BCF0,
        "?get_type@IVP_SurfaceManager_Polygon@@UAE?AW4IVP_SURMAN_TYPE@@XZ",
        "IVP_SURMAN_TYPE __thiscall corrected(IVP_SurfaceManager_Polygon *self)",
        "IVP_SurfaceManager_Polygon vtable slot 10; returns zero, "
        "IVP_SURMAN_POLYGON.",
    ),
    (
        0x10009990,
        "ivp_delete_real_object_via_vtable",
        "void __thiscall corrected(IVP_Real_Object *object)",
        "Compiler helper shared by TT_Physicalize and the environment "
        "destructor. Null-checks object then calls vtable slot 0 with "
        "deleting-destructor flag 1; this is an analysis name, not an "
        "invented IVP member.",
    ),
    (
        0x100099F0,
        "?get_all_near_mindists@IVP_Real_Object@@IAEXXZ",
        "void __thiscall corrected(IVP_Real_Object *self)",
        "Retained private vicinity-discovery helper. It temporarily marks "
        "the object and physical core as IVP_MT_GET_MINDIST, asks the retail "
        "mindist manager to recheck the OV element, then restores both states.",
    ),
    (
        0x1000DAB0,
        "?ensure_core_to_be_in_simulation@IVP_Core@@QAEXXZ",
        "void __thiscall corrected(IVP_Core *self)",
        "Retained Core wakeup entry. Its instruction stream contains the "
        "neighboring sim_unit_ensure_in_simulation operation: a sleeping "
        "movable unit is revived, while an active unit has its movement-check "
        "values cleared; physical-unmoveable cores return immediately.",
    ),
    (
        0x10028950,
        "ivp_shared_delete_this_callback",
        "void __thiscall corrected(void *self, void *deleted_subject)",
        "Compiler-folded callback body used for both core- and anchor-deletion "
        "slots across actuator and constraint vtables. It invokes this "
        "object's scalar deleting destructor and ignores deleted_subject.",
    ),
    (
        0x10015060,
        "??1IVP_Active_Value_Hash@@UAE@XZ",
        "void __thiscall corrected(IVP_Active_Value_Hash *self)",
        "Complete IVP_Active_Value_Hash destructor. The old ios_base name "
        "was imported onto an unrelated function; the body releases every "
        "stored active value and then destroys IVP_VHash.",
    ),
    (
        0x100150D0,
        "??_GIVP_Active_Value_Hash@@UAEPAXI@Z",
        "void *__thiscall corrected(IVP_Active_Value_Hash *self, unsigned int flags)",
        "IVP_Active_Value_Hash scalar deleting destructor, retail vtable slot 1.",
    ),
    (
        0x100150F0,
        "?object_to_index@IVP_Active_Value_Hash@@IAEHPAVIVP_U_Active_Value@@@Z",
        "int __thiscall corrected(IVP_Active_Value_Hash *self, IVP_U_Active_Value *value)",
        "Hashes value->name at offset 0x04 with the retail IVP CRC32 routine.",
    ),
    (
        0x10015140,
        "?compare@IVP_Active_Value_Hash@@MBE?AW4IVP_BOOL@@PAX0@Z",
        "IVP_BOOL __thiscall corrected(const IVP_Active_Value_Hash *self, void *left, void *right)",
        "Retail hash vtable slot 0; compares the names at active-value offset 0x04.",
    ),
    (
        0x10015160,
        "??0IVP_U_Active_Value_Manager@@QAE@W4IVP_BOOL@@@Z",
        "IVP_U_Active_Value_Manager *__thiscall corrected(IVP_U_Active_Value_Manager *self, IVP_BOOL delete_on_environment_delete)",
        "Retail 0x28-byte manager constructor. Hash pointers are at 0x08/0x0C "
        "and the reusable search value is at 0x24.",
    ),
    (
        0x10015260,
        "?environment_will_be_deleted@IVP_U_Active_Value_Manager@@UAEXPAVIVP_Environment@@@Z",
        "void __thiscall corrected(IVP_U_Active_Value_Manager *self, IVP_Environment *environment)",
        "IVP_U_Active_Value_Manager vtable slot 1.",
    ),
    (
        0x10015280,
        "??_GIVP_U_Active_Value_Manager@@UAEPAXI@Z",
        "void *__thiscall corrected(IVP_U_Active_Value_Manager *self, unsigned int flags)",
        "IVP_U_Active_Value_Manager scalar deleting destructor, vtable slot 0.",
    ),
    (
        0x100152A0,
        "??1IVP_U_Active_Value_Manager@@UAE@XZ",
        "void __thiscall corrected(IVP_U_Active_Value_Manager *self)",
        "Complete manager destructor. The old ios_base name was a bad imported type/name.",
    ),
    (
        0x10015360,
        "?init_active_values_generic@IVP_U_Active_Value_Manager@@UAEXXZ",
        "void __thiscall corrected(IVP_U_Active_Value_Manager *self)",
        "Manager vtable slot 9; installs double_null and current_time.",
    ),
    (
        0x10015410,
        "?insert_active_float@IVP_U_Active_Value_Manager@@UAEXPAVIVP_U_Active_Float@@@Z",
        "void __thiscall corrected(IVP_U_Active_Value_Manager *self, IVP_U_Active_Float *value)",
        "Manager vtable slot 2; uses the float-name hash at offset 0x08.",
    ),
    (
        0x10015490,
        "?insert_active_int@IVP_U_Active_Value_Manager@@UAEXPAVIVP_U_Active_Int@@@Z",
        "void __thiscall corrected(IVP_U_Active_Value_Manager *self, IVP_U_Active_Int *value)",
        "Manager vtable slot 4; uses the int-name hash at offset 0x0C.",
    ),
    (
        0x10015520,
        "?remove_active_float@IVP_U_Active_Value_Manager@@UAEXPAVIVP_U_Active_Float@@@Z",
        "void __thiscall corrected(IVP_U_Active_Value_Manager *self, IVP_U_Active_Float *value)",
        "Manager vtable slot 3.",
    ),
    (
        0x10015560,
        "?remove_active_int@IVP_U_Active_Value_Manager@@UAEXPAVIVP_U_Active_Int@@@Z",
        "void __thiscall corrected(IVP_U_Active_Value_Manager *self, IVP_U_Active_Int *value)",
        "Manager vtable slot 5.",
    ),
    (
        0x100155A0,
        "?delay_active_float@IVP_U_Active_Value_Manager@@UAEXPAVIVP_U_Active_Float_Delayed@@@Z",
        "void __thiscall corrected(IVP_U_Active_Value_Manager *self, IVP_U_Active_Float_Delayed *value)",
        "Manager vtable slot 6; appends uniquely to the vector at offset 0x10.",
    ),
    (
        0x100155F0,
        "?delay_active_int@IVP_U_Active_Value_Manager@@UAEXPAVIVP_U_Active_Int_Delayed@@@Z",
        "void __thiscall corrected(IVP_U_Active_Value_Manager *self, IVP_U_Active_Int_Delayed *value)",
        "Manager vtable slot 7; appends uniquely to the vector at offset 0x18.",
    ),
    (
        0x10015640,
        "?update_delayed_active_values@IVP_U_Active_Value_Manager@@UAEXXZ",
        "void __thiscall corrected(IVP_U_Active_Value_Manager *self)",
        "Manager vtable slot 8; dispatches and clears both delayed vectors.",
    ),
    (
        0x100156D0,
        "?refresh_psi_active_values@IVP_U_Active_Value_Manager@@UAEXPAVIVP_Environment@@@Z",
        "void __thiscall corrected(IVP_U_Active_Value_Manager *self, IVP_Environment *environment)",
        "Manager vtable slot 10; updates current_time then delayed values.",
    ),
    (
        0x10015700,
        "?install_active_float@IVP_U_Active_Value_Manager@@UAEPAVIVP_U_Active_Float@@PBDN@Z",
        "IVP_U_Active_Float *__thiscall corrected(IVP_U_Active_Value_Manager *self, const char *name, double value)",
        "Manager vtable slot 11; lookup-or-create for double active values.",
    ),
    (
        0x100157A0,
        "?create_active_float@IVP_U_Active_Value_Manager@@UAEPAVIVP_U_Active_Terminal_Double@@PBDN@Z",
        "IVP_U_Active_Terminal_Double *__thiscall corrected(IVP_U_Active_Value_Manager *self, const char *name, double value)",
        "Manager vtable slot 12; creates only when the name is absent.",
    ),
    (
        0x10015840,
        "?create_active_int@IVP_U_Active_Value_Manager@@UAEPAVIVP_U_Active_Terminal_Int@@PBDH@Z",
        "IVP_U_Active_Terminal_Int *__thiscall corrected(IVP_U_Active_Value_Manager *self, const char *name, int value)",
        "Manager vtable slot 14; creates only when the name is absent.",
    ),
    (
        0x100158E0,
        "?install_active_int@IVP_U_Active_Value_Manager@@UAEPAVIVP_U_Active_Int@@PBDH@Z",
        "IVP_U_Active_Int *__thiscall corrected(IVP_U_Active_Value_Manager *self, const char *name, int value)",
        "Manager vtable slot 13; lookup-or-create for integer active values.",
    ),
    (
        0x1000BDE0,
        "??_GIVP_Material_Manager@@UAEPAXI@Z",
        "void *__thiscall corrected(IVP_Material_Manager *self, unsigned int flags)",
        "Retail material-manager scalar deleting destructor, vtable slot 4. "
        "The imported std::locale::facet owner is false.",
    ),
    (
        0x1002F610,
        "??_GIVP_Anomaly_Limits@@UAEPAXI@Z",
        "void *__thiscall corrected(IVP_Anomaly_Limits *self, unsigned int flags)",
        "Retail anomaly-limits scalar deleting destructor, vtable slot 1.",
    ),
    (
        0x1002F630,
        "??1IVP_Anomaly_Limits@@UAE@XZ",
        "void __thiscall corrected(IVP_Anomaly_Limits *self)",
        "Retail anomaly-limits complete destructor. The imported "
        "IVP_Triangle owner is false.",
    ),
    (
        0x1002F680,
        "??_GIVP_Anomaly_Manager@@UAEPAXI@Z",
        "void *__thiscall corrected(IVP_Anomaly_Manager *self, unsigned int flags)",
        "Retail anomaly-manager scalar deleting destructor, vtable slot 6.",
    ),
    (
        0x1002F6A0,
        "??1IVP_Anomaly_Manager@@UAE@XZ",
        "void __thiscall corrected(IVP_Anomaly_Manager *self)",
        "Retail anomaly-manager complete destructor. The imported "
        "IVP_Triangle owner is false.",
    ),
    (
        0x100099A0,
        "?recalc_invalid_mindists_of_object@IVP_Real_Object@@IAEXXZ",
        "void __thiscall corrected(IVP_Real_Object *self)",
        "Retained protected invalid-mindist refresh. The body walks the "
        "object list at +0x24 and returns no value; the imported synapse "
        "pointer return type was false.",
    ),
    (
        0x100138F0,
        "?set_current_time@IVP_Environment@@QAEXVIVP_Time@@@Z",
        "void __thiscall corrected(IVP_Environment *self, IVP_Time time)",
        "Retained time-cache invalidation primitive. It increments the "
        "environment code at +0x138, copies the 8-byte IVP_Time to +0x120, "
        "and RET 8 confirms the by-value parameter.",
    ),
    (
        0x100187A0,
        "?recalc_exact_mindist@IVP_Mindist_Manager@@QAEXPAVIVP_Mindist@@@Z",
        "void __thiscall corrected(IVP_Mindist_Manager *self, IVP_Mindist *mindist)",
        "Retained exact-mindist refresh used by both retail object-list "
        "walkers. ECX is the manager, the sole stack argument is the "
        "mindist, and RET 4 fixes the member-call ABI.",
    ),
    (
        0x1001A820,
        "?reset_times@IVP_Hull_Manager@@AAEXXZ",
        "void __thiscall corrected(IVP_Hull_Manager *self)",
        "Retained hull reset. The body adjusts queued listener values, "
        "zeros the gradient state, and returns no value; the imported int "
        "prototype was false.",
    ),
    (
        0x1001E300,
        "?calc_next_PSI_matrix@IVP_Calc_Next_PSI_Solver@@QAEXPAVIVP_Event_Sim@@PAV?$IVP_U_Vector@VIVP_Hull_Manager_Base@@@@@Z",
        "void __thiscall corrected(IVP_Calc_Next_PSI_Solver *self, IVP_Event_Sim *event, IVP_U_Vector *active_hulls)",
        "Retained next-PSI core integrator. Its complete instruction flow "
        "and field offsets match the neighboring implementation, both "
        "retail commit callers pass event and hull-vector pointers, and "
        "RET 8 confirms the two member parameters.",
    ),
    (
        0x1001E730,
        "?are_events_in_hull@IVP_Hull_Manager@@QAE?AW4IVP_BOOL@@XZ",
        "IVP_BOOL __thiscall corrected(IVP_Hull_Manager *self)",
        "Compiler-emitted copy of the header-inline hull predicate. The "
        "retail body compares the min-list value at +0x28 with "
        "hull_value_next_psi at +0x18 and returns the 32-bit IVP_BOOL.",
    ),
    (
        0x1001E750,
        "?increase_hull_by_x@IVP_Hull_Manager@@QAEXVIVP_Time@@MMM@Z",
        "void __thiscall corrected(IVP_Hull_Manager *self, IVP_Time now, float delta_time, float gradient, float center_gradient)",
        "Compiler-emitted copy of the neighboring header-inline hull "
        "advance. RET 0x14 confirms an 8-byte IVP_Time followed by three "
        "32-bit IVP_FLOAT values.",
    ),
    (
        0x1001E870,
        "?calc_psi_rotation_axis@IVP_Calc_Next_PSI_Solver@@AAEXPBVIVP_U_Quat@@@Z",
        "void __thiscall corrected(IVP_Calc_Next_PSI_Solver *self, const IVP_U_Quat *relative_rotation)",
        "Retained rotation-axis and angular-speed calculation. The body "
        "reads the solver's sole Core pointer, consumes one quaternion "
        "pointer, writes Core angular fields, and RET 4 fixes the ABI.",
    ),
    (
        0x1001EA50,
        "?commit_all_calc_next_PSI_matrix@IVP_Calc_Next_PSI_Solver@@SAXPAVIVP_Environment@@PAV?$IVP_U_Vector@VIVP_Core@@@@PAV?$IVP_U_Vector@VIVP_Hull_Manager_Base@@@@@Z",
        "void __cdecl corrected(IVP_Environment *environment, IVP_U_Vector *cores, IVP_U_Vector *active_hulls)",
        "Retained static bulk integrator. All three inputs are read from "
        "the stack and each selected Core is passed through the retained "
        "calc_next_PSI_matrix body.",
    ),
    (
        0x1001EB10,
        "?commit_all_hull_managers@IVP_Calc_Next_PSI_Solver@@SAXPAVIVP_Environment@@PAV?$IVP_U_Vector@VIVP_Hull_Manager_Base@@@@@Z",
        "void __cdecl corrected(IVP_Environment *environment, IVP_U_Vector *active_hulls)",
        "Retained static bulk hull commit. Its two arguments are stack "
        "parameters; the environment is intentionally unused while each "
        "manager checks listeners and reset time.",
    ),
    (
        0x100116F0,
        "??1IVP_Sim_Unit_Controller_Core_List@@QAE@XZ",
        "void __thiscall corrected(IVP_Sim_Unit_Controller_Core_List *self)",
        "Previously anonymous complete controller/core-list destructor. It "
        "releases the embedded two-core vector at +0x04 and is called before "
        "retail operator delete by simulation-unit cleanup.",
    ),
    (
        0x10011890,
        "??1IVP_Simulation_Unit@@QAE@XZ",
        "void __thiscall corrected(IVP_Simulation_Unit *self)",
        "Previously anonymous complete Simulation Unit destructor. It calls "
        "clean_sim_unit, then destroys the controller and core vectors at "
        "+0x1c and +0x0c without freeing the object allocation itself; those "
        "vectors consequently cannot also be host-managed C++ members.",
    ),
    (
        0x10011920,
        "??0IVP_Simulation_Unit@@QAE@XZ",
        "IVP_Simulation_Unit *__thiscall corrected(IVP_Simulation_Unit *self)",
        "Complete 0x24-byte Simulation Unit constructor. It binds the "
        "two-cell inline core vector at +0x14, initializes the controller "
        "vector, and writes IVP_MT_NOT_SIM plus cleared state flags. Both "
        "vector lifetimes begin here, not before entry in wrapper code.",
    ),
    (
        0x100119D0,
        "?add_sim_unit_core@IVP_Simulation_Unit@@QAEXPAVIVP_Core@@@Z",
        "void __thiscall corrected(IVP_Simulation_Unit *self, IVP_Core *core)",
        "Previously anonymous compiler-emitted add_sim_unit_core. Its sole "
        "stack argument is appended to the vector at +0x0c and RET 4 fixes "
        "the member-call ABI.",
    ),
    (
        0x10011E70,
        "??0IVP_Sim_Units_Manager@@QAE@PAVIVP_Environment@@@Z",
        "IVP_Sim_Units_Manager *__thiscall corrected(IVP_Sim_Units_Manager *self, IVP_Environment *environment)",
        "Complete 0x1b0-byte manager constructor. It stores the environment "
        "at +0x00, initializes the two aligned IVP_Time values, and clears "
        "the moving and still list heads at +0x18 and +0x1a8.",
    ),
    (
        0x10011FE0,
        "?set_standard_gravity@IVP_Standard_Gravity_Controller@@QAEXPAVIVP_U_Point@@@Z",
        "void __thiscall corrected(IVP_Standard_Gravity_Controller *self, IVP_U_Point *gravity)",
        "Retained gravity setter. It converts the three doubles at gravity "
        "to floats and stores grav_vec at +0x04/+0x08/+0x0c.",
    ),
    (
        0x10012010,
        "?do_simulation_controller@IVP_Standard_Gravity_Controller@@UAEXPAVIVP_Event_Sim@@PAV?$IVP_U_Vector@VIVP_Core@@@@@Z",
        "void __thiscall corrected(IVP_Standard_Gravity_Controller *self, IVP_Event_Sim *event, IVP_U_Vector *cores)",
        "Previously anonymous retail gravity-controller slot 4. It damps "
        "and commits every Core in the supplied vector, then adds grav_vec "
        "times the event delta. The pinned test present in nearby source is "
        "absent from Ballance; RET 8 confirms both pointer parameters.",
    ),
    (
        0x10013230,
        "?get_controller_priority@IVP_Standard_Gravity_Controller@@UAE?AW4IVP_CONTROLLER_PRIORITY@@XZ",
        "IVP_CONTROLLER_PRIORITY __thiscall corrected(IVP_Standard_Gravity_Controller *self)",
        "Retail gravity-controller vtable slot 5. The body returns "
        "IVP_CP_GRAVITY (1000).",
    ),
    (
        0x10022170,
        "?core_is_going_to_be_deleted_event@IVP_Standard_Gravity_Controller@@UAEXPAVIVP_Core@@@Z",
        "void __thiscall corrected(IVP_Standard_Gravity_Controller *self, IVP_Core *core)",
        "Retail gravity-controller vtable slot 0. This intentional no-op "
        "returns with RET 4 for the IVP_Core pointer argument.",
    ),
    (
        0x100121B0,
        "?simulate_single_sim_unit_psi@IVP_Simulation_Unit@@QAEXPAVIVP_Event_Sim@@PAV?$IVP_U_Vector@VIVP_Core@@@@@Z",
        "void __thiscall corrected(IVP_Simulation_Unit *self, IVP_Event_Sim *event, IVP_U_Vector *touched_cores)",
        "Previously anonymous complete per-unit PSI routine. It owns the "
        "simulation-memory transaction, controller dispatch, core integration "
        "and touched-core output; RET 8 fixes both pointer parameters.",
    ),
    (
        0x10011300,
        "?sim_unit_calc_redundants@IVP_Simulation_Unit@@QAEXXZ",
        "void __thiscall corrected(IVP_Simulation_Unit *self)",
        "Retained redundant-controller rebuild. It walks each core's "
        "controller vector, adds unknown controllers, records controlled "
        "cores, then sorts by priority.",
    ),
    (
        0x10011370,
        "?controller_is_known_to_sim_unit@IVP_Simulation_Unit@@QAE?AW4IVP_BOOL@@PAVIVP_Controller@@@Z",
        "IVP_BOOL __thiscall corrected(IVP_Simulation_Unit *self, IVP_Controller *controller)",
        "Retained controller membership test. RET 4 and the scan of "
        "controller_cores at +0x1c confirm the single pointer argument.",
    ),
    (
        0x100113B0,
        "?add_controlled_core_for_controller@IVP_Simulation_Unit@@QAEXPAVIVP_Controller@@PAVIVP_Core@@@Z",
        "void __thiscall corrected(IVP_Simulation_Unit *self, IVP_Controller *controller, IVP_Core *core)",
        "Retained per-controller core append. RET 8 matches the two pointer "
        "parameters used by calc_redundants and add_controller_of_core.",
    ),
    (
        0x10011410,
        "?add_controller_unit_sim@IVP_Simulation_Unit@@QAEXPAVIVP_Controller@@@Z",
        "void __thiscall corrected(IVP_Simulation_Unit *self, IVP_Controller *controller)",
        "Retained controller-list allocation. It operator-new's 0x14 bytes, "
        "hand-inits the inline two-core vector, and appends to +0x1c.",
    ),
    (
        0x10011470,
        "?split_sim_unit@IVP_Simulation_Unit@@QAEXPAVIVP_Core@@@Z",
        "void __thiscall corrected(IVP_Simulation_Unit *self, IVP_Core *split_father)",
        "Retained split. It allocates a 0x24 MOVING unit, registers it, "
        "moves cores whose union-find father is split_father, then tail-loops "
        "instead of the neighboring recursive call when a third component "
        "remains. RET 4 confirms the father pointer.",
    ),
    (
        0x100115B0,
        "?perform_test_and_split@IVP_Simulation_Unit@@QAEXXZ",
        "void __thiscall corrected(IVP_Simulation_Unit *self)",
        "Retained split gate. A non-null union-find father cleans the unit, "
        "splits, then rebuilds redundants. Called from do_sim_unit_union_find.",
    ),
    (
        0x100115E0,
        "?sim_unit_union_find_test@IVP_Simulation_Unit@@QAEPAVIVP_Core@@XZ",
        "IVP_Core *__thiscall corrected(IVP_Simulation_Unit *self)",
        "Retained union-find probe. It clears Core tmp at +0x228, unions "
        "cores from each controller's associated-core vtable slot, and "
        "returns the first core whose father differs from core 0.",
    ),
    (
        0x10011690,
        "?clean_sim_unit@IVP_Simulation_Unit@@QAEXXZ",
        "void __thiscall corrected(IVP_Simulation_Unit *self)",
        "Retained controller-list teardown. Each IVP_Sim_Unit_Controller_Core_List "
        "is destroyed and operator-deleted, then the vector at +0x1c is cleared.",
    ),
    (
        0x10011770,
        "?fusion_simulation_unities@IVP_Simulation_Unit@@QAEXPAV1@@Z",
        "void __thiscall corrected(IVP_Simulation_Unit *self, IVP_Simulation_Unit *second_unit)",
        "Retained fuse. It cleans this unit, throws the second unit's cores "
        "in, then rebuilds redundants. Mindist managed-friction calls it "
        "when two simulated objects begin sharing a contact. RET 4 confirms "
        "the donor-unit pointer.",
    ),
    (
        0x10011790,
        "?get_pos_of_controller@IVP_Simulation_Unit@@QAEHPAVIVP_Controller@@@Z",
        "int __thiscall corrected(IVP_Simulation_Unit *self, IVP_Controller *controller)",
        "Retained controller index lookup. RET 4; returns the reverse-scan "
        "index or a negative value when the controller is absent.",
    ),
    (
        0x100117C0,
        "?sim_unit_remove_core@IVP_Simulation_Unit@@QAEXPAVIVP_Core@@@Z",
        "void __thiscall corrected(IVP_Simulation_Unit *self, IVP_Core *core)",
        "Retained core-removal-with-ownership. RET 4; previously mis-typed "
        "as int(void *, int). It unlinks the core, drops empty controller "
        "lists, and deletes the unit through the retail allocator when empty.",
    ),
    (
        0x10011970,
        "?rem_sim_unit_controller@IVP_Simulation_Unit@@QAEXPAVIVP_Controller@@@Z",
        "void __thiscall corrected(IVP_Simulation_Unit *self, IVP_Controller *controller)",
        "Retained controller-list removal. RET 4; destroys the matching "
        "IVP_Sim_Unit_Controller_Core_List and compact-removes it.",
    ),
    (
        0x10011A00,
        "?rem_sim_unit_core@IVP_Simulation_Unit@@QAEXPAVIVP_Core@@@Z",
        "void __thiscall corrected(IVP_Simulation_Unit *self, IVP_Core *core)",
        "Retained vector-only core removal. RET 4; this does not delete the "
        "unit or rewrite controller lists.",
    ),
    (
        0x10011CC0,
        "?add_controller_of_core@IVP_Simulation_Unit@@QAEXPAVIVP_Core@@PAVIVP_Controller@@@Z",
        "void __thiscall corrected(IVP_Simulation_Unit *self, IVP_Core *core, IVP_Controller *controller)",
        "Retained per-core controller install. RET 8; Core::add_core_controller "
        "calls it after recording the controller on the Core.",
    ),
    (
        0x10011D00,
        "?remove_controller_of_core@IVP_Simulation_Unit@@QAEXPAVIVP_Core@@PAVIVP_Controller@@@Z",
        "void __thiscall corrected(IVP_Simulation_Unit *self, IVP_Core *core, IVP_Controller *controller)",
        "Retained per-core controller removal. RET 8; Core::rem_core_controller "
        "calls it. An empty controller list is destroyed and compact-removed.",
    ),
    (
        0x10011DB0,
        "?sim_unit_sort_controllers@IVP_Simulation_Unit@@QAEXXZ",
        "void __thiscall corrected(IVP_Simulation_Unit *self)",
        "Retained insertion-style sort. Controller vtable slot 5 "
        "(get_controller_priority at +0x14) orders lists so smaller "
        "priority values come first.",
    ),
    (
        0x10011E40,
        "?sim_unit_exchange_controllers@IVP_Simulation_Unit@@QAEXHH@Z",
        "void __thiscall corrected(IVP_Simulation_Unit *self, int first, int second)",
        "Retained two-index swap of controller_cores. RET 8 matches the two "
        "int parameters used by the sort.",
    ),
    (
        0x10011EF0,
        "?add_sim_unit_to_manager@IVP_Sim_Units_Manager@@QAEXPAVIVP_Simulation_Unit@@@Z",
        "void __thiscall corrected(IVP_Sim_Units_Manager *self, IVP_Simulation_Unit *unit)",
        "Retained manager insert. RET 4; moving units go to slot 0, still "
        "units to still_slot at +0x1a8.",
    ),
    (
        0x10011F50,
        "?rem_sim_unit_from_manager@IVP_Sim_Units_Manager@@QAEXPAVIVP_Simulation_Unit@@@Z",
        "void __thiscall corrected(IVP_Sim_Units_Manager *self, IVP_Simulation_Unit *unit)",
        "Retained manager unlink. RET 4; split's donor unit is removed here "
        "after throw_cores rehomes every core.",
    ),
    (
        0x1001C7D0,
        "?union_find_get_father@IVP_Core@@QAEPAV1@XZ",
        "IVP_Core *__thiscall corrected(IVP_Core *self)",
        "Retained 8-instruction father walk of Core tmp at +0x228. Nearby "
        "source already marked it inline; Ballance kept the out-of-line body.",
    ),
    (
        0x100189C0,
        "??0IVP_Cache_Object_Manager@@QAE@H@Z",
        "IVP_Cache_Object_Manager *__thiscall corrected(IVP_Cache_Object_Manager *self, int initial_size)",
        "Retained Cache Object Manager constructor. The decorated symbol and "
        "RET 4 fix the single int parameter and member-call ABI.",
    ),
    (
        0x10015A00,
        "??0IVP_U_Active_Float@@QAE@PBD@Z",
        "IVP_U_Active_Float *__thiscall corrected(IVP_U_Active_Float *self, const char *name)",
        "Retained Active Float constructor; its decorated symbol fixes the "
        "single const char pointer parameter.",
    ),
    (
        0x10015980,
        "??0IVP_U_Active_Value@@QAE@PBD@Z",
        "IVP_U_Active_Value *__thiscall corrected(IVP_U_Active_Value *self, const char *name)",
        "Retained Active Value constructor; its decorated symbol fixes the "
        "single const char pointer parameter.",
    ),
    (
        0x10015A80,
        "??0IVP_U_Active_Int@@QAE@PBD@Z",
        "IVP_U_Active_Int *__thiscall corrected(IVP_U_Active_Int *self, const char *name)",
        "Retained Active Int constructor; the decorated symbol and RET 4 "
        "fix the single const char pointer parameter. Field writes confirm "
        "manager +0x14, last_update +0x18 and int_value +0x1c.",
    ),
    (
        0x10015C50,
        "??0IVP_U_Active_Terminal_Double@@QAE@PBDN@Z",
        "IVP_U_Active_Terminal_Double *__thiscall corrected(IVP_U_Active_Terminal_Double *self, const char *name, double value)",
        "Retained terminal-double constructor. The decorated symbol fixes "
        "const char pointer plus binary64 value; writes establish the delayed "
        "secondary base at +0x28 and old_value at +0x30.",
    ),
    (
        0x10015CC0,
        "??0IVP_U_Active_Terminal_Int@@QAE@PBDH@Z",
        "IVP_U_Active_Terminal_Int *__thiscall corrected(IVP_U_Active_Terminal_Int *self, const char *name, int value)",
        "Retained terminal-int constructor. The decorated symbol fixes the "
        "const char pointer and int parameters; writes establish the delayed "
        "secondary base at +0x20 and old_value at +0x24.",
    ),
    (
        0x10028210,
        "??0IVP_Constraint_Local_Anchor@@QAE@XZ",
        "IVP_Constraint_Local_Anchor *__thiscall corrected(IVP_Constraint_Local_Anchor *self)",
        "Retained Local Anchor constructor layer. It writes only rot at "
        "+0x84; object at +0x80 is intentionally left for the enclosing "
        "IVP_Constraint_Local constructor to assign. Direct public Anchor "
        "construction and both nested Local anchors enter this body.",
    ),
    (
        0x10037000,
        "??0IVP_U_Min_Hash@@QAE@H@Z",
        "IVP_U_Min_Hash *__thiscall corrected(IVP_U_Min_Hash *self, int initial_size)",
        "Retained Min Hash constructor. The decorated symbol and RET 4 fix "
        "the initial-size parameter.",
    ),
    (
        0x1001DE80,
        "??0IVP_VHash_Store@@QAE@H@Z",
        "IVP_VHash_Store *__thiscall corrected(IVP_VHash_Store *self, int initial_size)",
        "Retained pointer-store hash constructor with one int size parameter.",
    ),
    (
        0x1001DF60,
        "?add_elem@IVP_VHash_Store@@QAEXPAX0@Z",
        "void __thiscall corrected(IVP_VHash_Store *self, void *key, void *element)",
        "Retained pointer-store insertion overload; RET 8 confirms two pointer arguments.",
    ),
    (
        0x1001DFB0,
        "?add_elem@IVP_VHash_Store@@QAEXPAX0H@Z",
        "void __thiscall corrected(IVP_VHash_Store *self, void *key, void *element, int hash_index)",
        "Retained pre-hashed insertion overload; RET 0xc confirms all three arguments.",
    ),
    (
        0x1001E260,
        "?change_elem@IVP_VHash_Store@@QAEXPAX0@Z",
        "void __thiscall corrected(IVP_VHash_Store *self, void *key, void *element)",
        "Retained pointer-store replacement with two pointer arguments.",
    ),
    (
        0x1001E1B0,
        "?find_elem@IVP_VHash_Store@@QAEPAXPAX@Z",
        "void *__thiscall corrected(IVP_VHash_Store *self, void *key)",
        "Retained pointer-store lookup; the decorated symbol fixes its void-pointer result.",
    ),
    (
        0x1001E200,
        "?find_elem@IVP_VHash_Store@@QAEPAXPAXI@Z",
        "void *__thiscall corrected(IVP_VHash_Store *self, void *key, unsigned int hash_index)",
        "Retained pre-hashed pointer-store lookup; RET 8 fixes both arguments.",
    ),
    (
        0x1001E070,
        "?remove_elem@IVP_VHash_Store@@QAEPAXPAX@Z",
        "void *__thiscall corrected(IVP_VHash_Store *self, void *key)",
        "Retained pointer-store removal returning the removed element.",
    ),
    (
        0x1001E0C0,
        "?remove_elem@IVP_VHash_Store@@QAEPAXPAXI@Z",
        "void *__thiscall corrected(IVP_VHash_Store *self, void *key, unsigned int hash_index)",
        "Retained pre-hashed pointer-store removal returning the removed element.",
    ),
    (
        0x10037260,
        "?remove@IVP_U_Min_Hash@@QAEXPAX@Z",
        "void __thiscall corrected(IVP_U_Min_Hash *self, void *element)",
        "Retained Min Hash removal with one opaque element pointer.",
    ),
    (
        0x100372E0,
        "?remove_min@IVP_U_Min_Hash@@QAEXXZ",
        "void __thiscall corrected(IVP_U_Min_Hash *self)",
        "Retained Min Hash minimum removal; the decorated symbol has no explicit arguments.",
    ),
    (
        0x10018910,
        "?invalid_cache_object@IVP_Cache_Object_Manager@@SAXPAVIVP_Real_Object@@@Z",
        "void __cdecl corrected(IVP_Real_Object *object)",
        "Retained static cache invalidation callback. The decorated symbol "
        "requires cdecl rather than a synthetic manager self parameter.",
    ),
    (
        0x10018930,
        "?get_cache_object@IVP_Cache_Object_Manager@@QAEPAVIVP_Cache_Object@@PAVIVP_Real_Object@@@Z",
        "IVP_Cache_Object *__thiscall corrected(IVP_Cache_Object_Manager *self, IVP_Real_Object *object)",
        "Retained cache lookup/creation entry returning an IVP_Cache_Object pointer.",
    ),
    (
        0x1001A190,
        "BML_Retail_IVP_Real_Object_get_cache_object_inline",
        "IVP_Cache_Object *__thiscall corrected(IVP_Real_Object *self)",
        "Compiler-emitted shared body of the public inline locking accessor. "
        "It creates object+0x40 through Environment+0xA4 when absent, increments "
        "cache+0x04 before a simulated-object time-code refresh through RVA "
        "0x18A40, and returns object+0x40. This is an inline implementation "
        "artifact, not evidence of a retained decorated export.",
    ),
    (
        0x10024C20,
        "BML_Retail_IVP_U_Vector_construct_allocated_inline",
        "void __thiscall corrected(IVP_U_Vector_Base *self, int size)",
        "Compiler-emitted shared allocated constructor for compact 16-bit "
        "IVP_U_Vector instantiations. It writes uint16 capacity/count at "
        "+0x00/+0x02 and allocates size*4 bytes into +0x04 through the retail "
        "allocator. Direct callers include Phantom Active Set listener vectors "
        "and Impact Solver Core vectors; this is not a decorated template export.",
    ),
    (
        0x10015BA0,
        "BML_Retail_IVP_U_Active_Float_add_dependency_inline",
        "void __thiscall corrected(IVP_U_Active_Float *self, IVP_U_Active_Float_Listener *listener)",
        "Compiler-emitted shared body of the public Float dependency insertion. "
        "It grows the compact listener vector at +0x0C when needed, appends the "
        "listener, then increments reference_count at +0x08. Four calls from the "
        "retained Spring Active constructor establish the owner and semantics; "
        "this is not evidence of a decorated export.",
    ),
    (
        0x10015BE0,
        "BML_Retail_IVP_U_Active_Float_remove_dependency_inline",
        "void __thiscall corrected(IVP_U_Active_Float *self, IVP_U_Active_Float_Listener *listener)",
        "Compiler-emitted shared body of the public Float dependency removal. "
        "It removes the registered listener from the compact vector at +0x0C, "
        "then decrements reference_count at +0x08 and invokes deleting slot 0 "
        "when it reaches zero. Four calls from the retained Spring Active "
        "destructor establish the owner and lifecycle; callers must remove only "
        "a registered listener. This is not evidence of a decorated export.",
    ),
    (
        0x100109E0,
        "?mindist_entered_volume@IVP_Controller_Phantom@@IAEXPAVIVP_Mindist@@@Z",
        "void __thiscall corrected(IVP_Controller_Phantom *self, IVP_Mindist *mindist)",
        "Retained protected Phantom transition. Controller constructor and "
        "Mindist Manager callsites establish the owner and one-pointer thiscall "
        "ABI; it updates the mindist/object/Core sets and listener callbacks.",
    ),
    (
        0x10010B70,
        "?mindist_left_volume@IVP_Controller_Phantom@@IAEXPAVIVP_Mindist@@@Z",
        "void __thiscall corrected(IVP_Controller_Phantom *self, IVP_Mindist *mindist)",
        "Retained protected Phantom exit transition. Mindist Manager callsites, "
        "the compact-set removal, counter decrements and listener callbacks "
        "establish the owner and one-pointer thiscall ABI.",
    ),
    (
        0x1000C200,
        "?calc_virt_mass_worst_case@IVP_Core@@QBENPBVIVP_U_Float_Point@@@Z",
        "double __thiscall corrected(const IVP_Core *self, const IVP_U_Float_Point *core_point)",
        "Retained public worst-case effective-mass body. Three calls from "
        "IVP_Contact_Point::calc_virtual_mass_of_mindist pass Core-space "
        "contact arms; RET 4, fields +0x34..+0x40 and the x87 return establish "
        "one pointer argument and binary64 result. Unlike nearby source, this "
        "Ballance body has no pinned branch.",
    ),
    (
        0x10011B30,
        "?announce_controller_to_environment@IVP_Controller_Manager@@QAEXPAVIVP_Controller_Dependent@@@Z",
        "void __thiscall corrected(IVP_Controller_Manager *self, IVP_Controller_Dependent *controller)",
        "Retained dependent-controller registration with one controller pointer.",
    ),
    (
        0x10011AD0,
        "?ensure_controller_in_simulation@IVP_Controller_Manager@@QAEXPAVIVP_Controller_Dependent@@@Z",
        "void __thiscall corrected(IVP_Controller_Manager *self, IVP_Controller_Dependent *controller)",
        "Retained dependent-controller wake/installation path.",
    ),
    (
        0x10011A60,
        "?remove_controller_from_environment@IVP_Controller_Manager@@SAXPAVIVP_Controller_Dependent@@W4IVP_BOOL@@@Z",
        "void __cdecl corrected(IVP_Controller_Dependent *controller, IVP_BOOL silently)",
        "Retained static controller-removal entry. The decorated symbol fixes "
        "cdecl and the 32-bit IVP_BOOL second parameter.",
    ),
    (
        0x10009BE0,
        "?calc_m_core_f_object@IVP_Real_Object@@QAEXPAVIVP_U_Matrix@@@Z",
        "void __thiscall corrected(IVP_Real_Object *self, IVP_U_Matrix *matrix)",
        "Retained object-to-core transform calculation with one output matrix pointer.",
    ),
    (
        0x1000A2B0,
        "?get_collision_check_reference_count@IVP_Real_Object@@QAEHXZ",
        "int __thiscall corrected(IVP_Real_Object *self)",
        "Retained collision-check reference count query returning a 32-bit int.",
    ),
    (
        0x10009F00,
        "?init_object_core@IVP_Real_Object@@QAEXPAVIVP_Environment@@PBVIVP_Template_Real_Object@@@Z",
        "void __thiscall corrected(IVP_Real_Object *self, IVP_Environment *environment, const IVP_Template_Real_Object *configuration)",
        "Retained Real Object core initialization entry; the decorated symbol "
        "fixes mutable environment and const configuration pointers.",
    ),
    (
        0x100095B0,
        "?insert_anchor@IVP_Real_Object@@QAEXPAVIVP_Anchor@@@Z",
        "void __thiscall corrected(IVP_Real_Object *self, IVP_Anchor *anchor)",
        "Retained intrusive anchor-list insertion.",
    ),
    (
        0x100095E0,
        "?remove_anchor@IVP_Real_Object@@QAEXPAVIVP_Anchor@@@Z",
        "void __thiscall corrected(IVP_Real_Object *self, IVP_Anchor *anchor)",
        "Retained intrusive anchor-list removal.",
    ),
    (
        0x10009670,
        "?revive_object_for_simulation@IVP_Real_Object@@QAEXXZ",
        "void __thiscall corrected(IVP_Real_Object *self)",
        "Retained object revival entry with no explicit arguments.",
    ),
    (
        0x10013EA0,
        "?init_anchor@IVP_Anchor@@QAEXPAVIVP_Actuator@@PAVIVP_Template_Anchor@@@Z",
        "void __thiscall corrected(IVP_Anchor *self, IVP_Actuator *actuator, IVP_Template_Anchor *configuration)",
        "Retained anchor initialization with actuator and template pointers.",
    ),
    (
        0x100141F0,
        "?object_is_going_to_be_deleted_event@IVP_Anchor@@QAEXPAVIVP_Real_Object@@@Z",
        "void __thiscall corrected(IVP_Anchor *self, IVP_Real_Object *object)",
        "Retained anchor object-deletion callback.",
    ),
    (
        0x10016610,
        "?init_synapse_real@IVP_Synapse@@QAEXPAVIVP_Mindist_Base@@PAVIVP_Real_Object@@@Z",
        "void __thiscall corrected(IVP_Synapse *self, IVP_Mindist_Base *mindist, IVP_Real_Object *object)",
        "Retained real-synapse initialization with mindist and object pointers.",
    ),
    (
        0x1000BDC0,
        "?environment_will_be_deleted@IVP_Material_Manager@@UAEXPAVIVP_Environment@@@Z",
        "void __thiscall corrected(IVP_Material_Manager *self, IVP_Environment *environment)",
        "Retail Material Manager environment-deletion virtual callback.",
    ),
    (
        0x1000BD40,
        "?get_elasticity@IVP_Material_Manager@@UAENPAUIVP_Contact_Situation@@@Z",
        "double __thiscall corrected(IVP_Material_Manager *self, IVP_Contact_Situation *contact)",
        "Retail Material Manager elasticity query. MSVC result code N confirms double.",
    ),
    (
        0x1000BD10,
        "?get_friction_factor@IVP_Material_Manager@@UAENPAUIVP_Contact_Situation@@@Z",
        "double __thiscall corrected(IVP_Material_Manager *self, IVP_Contact_Situation *contact)",
        "Retail Material Manager friction query. MSVC result code N confirms double.",
    ),
    (
        0x1002F640,
        "?environment_will_be_deleted@IVP_Anomaly_Limits@@UAEXPAVIVP_Environment@@@Z",
        "void __thiscall corrected(IVP_Anomaly_Limits *self, IVP_Environment *environment)",
        "Retail Anomaly Limits environment-deletion virtual callback.",
    ),
    (
        0x1002F6B0,
        "?environment_will_be_deleted@IVP_Anomaly_Manager@@UAEXPAVIVP_Environment@@@Z",
        "void __thiscall corrected(IVP_Anomaly_Manager *self, IVP_Environment *environment)",
        "Retail Anomaly Manager environment-deletion virtual callback.",
    ),
    (
        0x1002FC80,
        "?get_push_speed_penetration@IVP_Anomaly_Manager@@UAEMPAVIVP_Real_Object@@0@Z",
        "float __thiscall corrected(IVP_Anomaly_Manager *self, IVP_Real_Object *first, IVP_Real_Object *second)",
        "Retail penetration push-speed query. MSVC result code M confirms float, "
        "matching Ballance IVP_FLOAT.",
    ),
    (
        0x1002F720,
        "?max_angular_velocity_exceeded@IVP_Anomaly_Manager@@UAEXPAVIVP_Anomaly_Limits@@PAVIVP_Core@@PAVIVP_U_Float_Point@@@Z",
        "void __thiscall corrected(IVP_Anomaly_Manager *self, IVP_Anomaly_Limits *limits, IVP_Core *core, IVP_U_Float_Point *angular_velocity)",
        "Retail excessive-angular-velocity virtual callback; RET 0xc confirms three pointers.",
    ),
    (
        0x1002FCC0,
        "?max_collisions_exceeded_check_freezing@IVP_Anomaly_Manager@@UAE?AW4IVP_BOOL@@PAVIVP_Anomaly_Limits@@PAVIVP_Core@@@Z",
        "IVP_BOOL __thiscall corrected(IVP_Anomaly_Manager *self, IVP_Anomaly_Limits *limits, IVP_Core *core)",
        "Retail excessive-collision freezing check returning the 32-bit IVP_BOOL enum.",
    ),
    (
        0x1002F6D0,
        "?max_velocity_exceeded@IVP_Anomaly_Manager@@UAEXPAVIVP_Anomaly_Limits@@PAVIVP_Core@@PAVIVP_U_Float_Point@@@Z",
        "void __thiscall corrected(IVP_Anomaly_Manager *self, IVP_Anomaly_Limits *limits, IVP_Core *core, IVP_U_Float_Point *velocity)",
        "Retail excessive-linear-velocity virtual callback; RET 0xc confirms three pointers.",
    ),
    (
        0x10021AB0,
        "BML_Retail_IVP_Ray_Solver_Os_check_ray_against_compact_ledge_os",
        "void __thiscall corrected(IVP_Ray_Solver_Os *self, const IVP_Compact_Ledge *ledge)",
        "Recovered exact object-space compact-ledge intersection body. "
        "Retained ledgetree traversal RVA 0x21EE0 calls this function with "
        "ECX=self and one const ledge pointer; RET 4 fixes the stack ABI. "
        "The machine code does not normalize EAX on every hit/miss exit, so "
        "Ballance must be called through its void-return body variant and the "
        "public neighboring IVP_BOOL result reconstructed from callback delivery.",
    ),
    (
        0x10022030,
        "??0IVP_Ray_Solver_Os@@QAE@PAVIVP_Ray_Solver@@PAVIVP_Real_Object@@@Z",
        "IVP_Ray_Solver_Os *__thiscall corrected(IVP_Ray_Solver_Os *self, IVP_Ray_Solver *solver, IVP_Real_Object *object)",
        "Retained object-space ray solver constructor. The decorated symbol "
        "and RET 8 fix the solver and Real Object pointers.",
    ),
    (
        0x10022010,
        "?check_ray_against_compact_surface_os@IVP_Ray_Solver_Os@@QAEXPBVIVP_Compact_Surface@@@Z",
        "void __thiscall corrected(IVP_Ray_Solver_Os *self, const IVP_Compact_Surface *surface)",
        "Retained object-space compact-surface traversal with one const surface pointer.",
    ),
    (
        0x10021EE0,
        "?check_ray_against_ledge_tree_node_os@IVP_Ray_Solver_Os@@QAEXPBVIVP_Compact_Ledgetree_Node@@@Z",
        "void __thiscall corrected(IVP_Ray_Solver_Os *self, const IVP_Compact_Ledgetree_Node *node)",
        "Retained recursive object-space ledgetree traversal with one const node pointer.",
    ),
    (
        0x1002D990,
        "?get_coll_range_intra_objects@IVP_Range_Manager@@UAEXPBVIVP_Real_Object@@0PAN1@Z",
        "void __thiscall corrected(IVP_Range_Manager *self, const IVP_Real_Object *first, const IVP_Real_Object *second, double *first_range, double *second_range)",
        "Retail two-object broadphase range calculation; RET 0x10 confirms four pointer arguments.",
    ),
    (
        0x1002DAB0,
        "?get_coll_range_in_world@IVP_Range_Manager@@UAENPBVIVP_Real_Object@@@Z",
        "double __thiscall corrected(IVP_Range_Manager *self, const IVP_Real_Object *object)",
        "Retail object/world broadphase range calculation with x87 IVP_DOUBLE return.",
    ),
    (
        0x1000BB00,
        "?insert_all_ledges_hitting_ray@IVP_SurfaceManager_Polygon@@UAEXPAVIVP_Ray_Solver@@PAVIVP_Real_Object@@@Z",
        "void __thiscall corrected(IVP_SurfaceManager_Polygon *self, IVP_Ray_Solver *ray_solver, IVP_Real_Object *object)",
        "Retail polygon-surface ray traversal with solver and object pointers.",
    ),
    (
        0x1000BBE0,
        "?get_all_terminal_ledges@IVP_SurfaceManager_Polygon@@UAEXPAV?$IVP_U_BigVector@VIVP_Compact_Ledge@@@@@Z",
        "void __thiscall corrected(IVP_SurfaceManager_Polygon *self, void *result)",
        "Retail terminal-ledge collection into IVP_U_BigVector<IVP_Compact_Ledge>; IDA's C declaration parser cannot spell the C++ template, so the pointer remains opaque in the database.",
    ),
    (
        0x1000BC00,
        "?get_mass_center@IVP_SurfaceManager_Polygon@@UBEXPAVIVP_U_Float_Point@@@Z",
        "void __thiscall corrected(const IVP_SurfaceManager_Polygon *self, IVP_U_Float_Point *output)",
        "Retail compact-surface mass-center copy reached through vtable slot 1.",
    ),
    (
        0x1000BC20,
        "?get_rotation_inertia@IVP_SurfaceManager_Polygon@@UBEXPAVIVP_U_Float_Point@@@Z",
        "void __thiscall corrected(const IVP_SurfaceManager_Polygon *self, IVP_U_Float_Point *output)",
        "Retail compact-surface rotational-inertia copy reached through vtable slot 3.",
    ),
    (
        0x1000BC40,
        "?get_radius_and_radius_dev_to_given_center@IVP_SurfaceManager_Polygon@@UBEXPBVIVP_U_Float_Point@@PAM1@Z",
        "void __thiscall corrected(const IVP_SurfaceManager_Polygon *self, const IVP_U_Float_Point *center, float *radius, float *radius_deviation)",
        "Retail center-shifted radius and quantized surface-deviation calculation.",
    ),
    (
        0x10060B90,
        "?is_debug_enabled@IVP_BetterDebugmanager@@QAE?AW4IVP_BOOL@@W4IVP_DEBUG_CLASS@@@Z",
        "IVP_BOOL __thiscall corrected(IVP_BetterDebugmanager *self, IVP_DEBUG_CLASS class_id)",
        "Retail debug-channel query; reads initialized at +0x04 and the selected 32-bit flag at +0x08 after rejecting identifiers >= 2048.",
    ),
    (
        0x10060BC0,
        "?dprint@IVP_BetterDebugmanager@@QAAXW4IVP_DEBUG_CLASS@@PBDZZ",
        "void __cdecl corrected(IVP_BetterDebugmanager *self, IVP_DEBUG_CLASS class_id, const char *formatstring, ...)",
        "Retail variadic member uses x86 __cdecl with self on the stack, formats into 4096 bytes and dispatches vtable slot 0.",
    ),
    (
        0x10060C10,
        "?output_function@IVP_BetterDebugmanager@@UAEXW4IVP_DEBUG_CLASS@@PBD@Z",
        "void __thiscall corrected(IVP_BetterDebugmanager *self, IVP_DEBUG_CLASS class_id, const char *string)",
        "Retail virtual output hook prints the supplied text and returns with RET 8.",
    ),
    (
        0x10060C30,
        "??0IVP_BetterDebugmanager@@QAE@XZ",
        "IVP_BetterDebugmanager *__thiscall corrected(IVP_BetterDebugmanager *self)",
        "Retail constructor installs vtable 0x10063D34, clears 2048 32-bit flags and sets initialized at +0x04.",
    ),
    (
        0x10060C50,
        "??_GIVP_BetterDebugmanager@@UAEPAXI@Z",
        "void *__thiscall corrected(IVP_BetterDebugmanager *self, unsigned int flags)",
        "Ballance two-slot vtable deleting destructor; the previously attached Triangle name came from a shared trivial complete-destructor body.",
    ),
    (
        0x1003AC00,
        "?convert_triangle_to_compace_ledge@IVP_SurfaceBuilder_Pointsoup@@SAPAVIVP_Compact_Ledge@@PAVIVP_U_Point@@00@Z",
        "IVP_Compact_Ledge *__cdecl corrected(IVP_U_Point *point0, IVP_U_Point *point1, IVP_U_Point *point2)",
        "Retained static triangle fast path. Three stack pointer arguments and plain RET prove __cdecl; the body clones the cached ledge through physics_RT's aligned allocator.",
    ),
    (
        0x1003A2D0,
        "?get_qlen_of_all_edges@IVP_SurMan_PS_Plane@@QAENXZ",
        "double __thiscall corrected(IVP_SurMan_PS_Plane *self)",
        "Retained squared-edge-length sum; accesses the embedded vector at +0x20/+0x22/+0x24 and returns IVP_DOUBLE through x87.",
    ),
    (
        0x1002DB50,
        "??0IVP_BetterStatisticsmanager@@QAE@XZ",
        "IVP_BetterStatisticsmanager *__thiscall corrected(IVP_BetterStatisticsmanager *self)",
        "Retail complete constructor clears both embedded vectors, enables collection, marks output delayed and stores the 8-byte 1.0 update interval at +0x28.",
    ),
    (
        0x1000C510,
        "?calc_at_matrix@IVP_Core@@QBEXVIVP_Time@@PAVIVP_U_Matrix@@@Z",
        "void __thiscall corrected(const IVP_Core *self, IVP_Time current_time, IVP_U_Matrix *matrix_out)",
        "Retail Core transform interpolation. The decorated X return is void; the VIVP_Time decoration, qword load at [ebp+8] and RET 0x0C prove an eight-byte by-value IVP_Time followed by one Matrix output pointer.",
    ),
    (
        0x10009D70,
        "?calc_at_matrix@IVP_Real_Object@@QBEXVIVP_Time@@PAVIVP_U_Matrix@@@Z",
        "void __thiscall corrected(const IVP_Real_Object *self, IVP_Time current_time, IVP_U_Matrix *matrix_out)",
        "Retail Real Object transform interpolation. The decorated VIVP_Time parameter and qword load at [ebp+8] preserve the public time wrapper rather than erasing it to a bare double; RET 0x0C confirms eight value bytes plus the output pointer.",
    ),
)


def require(condition: bool, message: str) -> None:
    if not condition:
        print(f"REFUSED\t{message}")
        ida_pro.qexit(2)


def ensure_forward_struct(name: str) -> None:
    """Add only a missing opaque tag; never replace an imported definition."""
    existing = ida_typeinf.tinfo_t()
    if existing.get_named_type(None, name, ida_typeinf.BTF_STRUCT):
        print(f"TYPE_PRESENT\t{name}")
        return
    ordinal = idc.set_local_type(
        -1,
        f"struct {name};",
        idc.PT_SIL | idc.PT_REPLACE,
    )
    require(ordinal != 0, f"could not add forward declaration for {name}")
    print(f"TYPE_ADDED\t{name}\t{ordinal}")


def ensure_source_struct_definition(
    name: str, expected_size: int, declaration: str, comment: str
) -> None:
    """Add a missing source-only UDT and label its evidence explicitly."""
    existing = ida_typeinf.tinfo_t()
    if not existing.get_named_type(None, name, ida_typeinf.BTF_STRUCT):
        ordinal = idc.set_local_type(
            -1,
            declaration,
            idc.PT_SIL | idc.PT_REPLACE,
        )
        require(ordinal != 0, f"could not add source UDT {name}")
        print(f"TYPE_SOURCE_ADDED\t{name}\t{ordinal}")
    set_struct_comment(name, expected_size, comment)


def replace_source_struct_definition(
    name: str, expected_size: int, declaration: str, comment: str
) -> None:
    """Replace a proven-wrong imported UDT with an instruction-backed layout."""
    existing_ordinal = ida_typeinf.get_type_ordinal(
        ida_typeinf.get_idati(), name
    )
    if existing_ordinal > 0:
        require(
            ida_typeinf.del_numbered_type(
                ida_typeinf.get_idati(), existing_ordinal
            ),
            f"could not remove imported UDT {name}",
        )
    ordinal = idc.set_local_type(
        existing_ordinal if existing_ordinal > 0 else -1,
        declaration,
        idc.PT_SIL | idc.PT_REPLACE,
    )
    require(ordinal != 0, f"could not replace source UDT {name}")
    set_struct_comment(name, expected_size, comment)
    print(f"TYPE_REPLACED\t{name}\t{ordinal}\t0x{expected_size:X}")


def replace_cyclic_struct_definition(
    name: str, expected_size: int, declaration: str, comment: str
) -> None:
    """Replace a UDT whose already-defined dependencies point back to it."""
    idati = ida_typeinf.get_idati()
    existing_ordinal = ida_typeinf.get_type_ordinal(idati, name)
    require(existing_ordinal > 0, f"missing cyclic UDT {name}")

    # Parse under a disposable tag while the original ordinal is still live,
    # so members such as IVP_Statistic_Manager::l_environment can resolve the
    # back-reference.  Then atomically replace the numbered type at the same
    # ordinal, preserving every dependent typeref.
    temporary_name = f"__BML_REBUILT_{name}"
    temporary_declaration = declaration.replace(
        f"struct {name} ", f"struct {temporary_name} ", 1
    )
    rebuilt = ida_typeinf.tinfo_t()
    parsed_name = ida_typeinf.parse_decl(
        rebuilt,
        idati,
        temporary_declaration,
        ida_typeinf.PT_SIL | ida_typeinf.PT_TYP,
    )
    require(parsed_name is not None, f"could not parse rebuilt UDT {name}")
    require(
        rebuilt.is_udt() and rebuilt.get_size() == expected_size,
        f"rebuilt {name} has size 0x{rebuilt.get_size():X}",
    )
    require(
        rebuilt.set_numbered_type(
            idati, existing_ordinal, ida_typeinf.NTF_REPLACE, name
        )
        == ida_typeinf.TERR_OK,
        f"could not replace cyclic UDT {name}",
    )
    set_struct_comment(name, expected_size, comment)
    print(f"TYPE_REPLACED_CYCLIC\t{name}\t{existing_ordinal}\t0x{expected_size:X}")


def rename_struct_member(
    struct_name: str,
    expected_size: int,
    old_name: str,
    expected_offset: int,
    new_name: str,
    comment: str,
) -> None:
    """Correct an imported member label without changing its stored type."""
    value = ida_typeinf.tinfo_t()
    require(
        value.get_named_type(None, struct_name, ida_typeinf.BTF_STRUCT),
        f"missing imported struct {struct_name}",
    )
    require(
        value.get_size() == expected_size,
        f"unexpected {struct_name} size 0x{value.get_size():X}",
    )
    members = ida_typeinf.udt_type_data_t()
    require(value.get_udt_details(members), f"could not read {struct_name}")
    matches = [
        (index, member)
        for index, member in enumerate(members)
        if member.offset == expected_offset * 8
        and member.name in (old_name, new_name)
    ]
    require(
        len(matches) == 1,
        f"missing {struct_name}::{old_name} at 0x{expected_offset:X}",
    )
    index, member = matches[0]
    action = "VERIFIED"
    if member.name != new_name:
        require(
            value.rename_udm(index, new_name) == ida_typeinf.TERR_OK,
            f"could not rename {struct_name}::{old_name}",
        )
        action = "RENAMED"
    require(
        value.set_udm_cmt(index, comment, True) == ida_typeinf.TERR_OK,
        f"could not comment {struct_name}::{new_name}",
    )
    print(
        f"TYPE_MEMBER_{action}\t{struct_name}\t0x{expected_offset:X}\t"
        f"{old_name}\t{new_name}"
    )


def require_struct_size(name: str, expected_size: int) -> None:
    value = ida_typeinf.tinfo_t()
    require(
        value.get_named_type(None, name, ida_typeinf.BTF_STRUCT),
        f"missing imported struct {name}",
    )
    require(
        value.get_size() == expected_size,
        f"unexpected {name} size 0x{value.get_size():X}",
    )
    print(f"TYPE_VERIFIED\t{name}\t0x{expected_size:X}")


def set_struct_comment(
    name: str, expected_size: int, comment: str
) -> None:
    """Annotate a size-verified imported UDT without changing its layout."""
    value = ida_typeinf.tinfo_t()
    require(
        value.get_named_type(None, name, ida_typeinf.BTF_STRUCT),
        f"missing imported struct {name}",
    )
    require(
        value.get_size() == expected_size,
        f"unexpected {name} size 0x{value.get_size():X}",
    )
    require(
        value.set_type_cmt(comment, True) == ida_typeinf.TERR_OK,
        f"could not comment imported struct {name}",
    )
    print(f"TYPE_COMMENTED\t{name}\t0x{expected_size:X}")


def set_struct_member_type(
    struct_name: str,
    expected_size: int,
    member_name: str,
    expected_offset: int,
    declaration: str,
    comment: str,
) -> None:
    """Correct one imported member without replacing the surrounding UDT."""
    value = ida_typeinf.tinfo_t()
    require(
        value.get_named_type(None, struct_name, ida_typeinf.BTF_STRUCT),
        f"missing imported struct {struct_name}",
    )
    require(
        value.get_size() == expected_size,
        f"unexpected {struct_name} size 0x{value.get_size():X}",
    )
    members = ida_typeinf.udt_type_data_t()
    require(
        value.get_udt_details(members),
        f"could not read members for {struct_name}",
    )
    matches = [
        (index, member)
        for index, member in enumerate(members)
        if member.name == member_name
        and member.offset == expected_offset * 8
    ]
    require(
        len(matches) == 1,
        f"missing {struct_name}::{member_name} at 0x{expected_offset:X}",
    )
    index, member = matches[0]
    replacement = ida_typeinf.tinfo_t(declaration)
    require(
        replacement.get_size() == member.size // 8,
        f"replacement size mismatch for {struct_name}::{member_name}",
    )
    require(
        value.set_udm_type(index, replacement) == ida_typeinf.TERR_OK,
        f"could not set {struct_name}::{member_name} to {declaration}",
    )
    require(
        value.set_udm_cmt(index, comment, True) == ida_typeinf.TERR_OK,
        f"could not comment {struct_name}::{member_name}",
    )
    print(
        f"TYPE_MEMBER_CORRECTED\t{struct_name}\t{member_name}\t"
        f"0x{expected_offset:X}\t{declaration}"
    )


def set_struct_member_named_udt_type(
    struct_name: str,
    expected_size: int,
    member_name: str,
    expected_offset: int,
    type_name: str,
    comment: str,
) -> None:
    """Set a member to a local UDT whose C++ template name is not parsable."""
    value = ida_typeinf.tinfo_t()
    require(
        value.get_named_type(None, struct_name, ida_typeinf.BTF_STRUCT),
        f"missing imported struct {struct_name}",
    )
    require(
        value.get_size() == expected_size,
        f"unexpected {struct_name} size 0x{value.get_size():X}",
    )
    members = ida_typeinf.udt_type_data_t()
    require(value.get_udt_details(members), f"could not read {struct_name}")
    matches = [
        (index, member)
        for index, member in enumerate(members)
        if member.name == member_name
        and member.offset == expected_offset * 8
    ]
    require(
        len(matches) == 1,
        f"missing {struct_name}::{member_name} at 0x{expected_offset:X}",
    )
    index, member = matches[0]
    replacement = ida_typeinf.tinfo_t()
    require(
        replacement.get_named_type(None, type_name, ida_typeinf.BTF_STRUCT),
        f"missing named UDT {type_name}",
    )
    require(
        replacement.get_size() == member.size // 8,
        f"replacement size mismatch for {struct_name}::{member_name}",
    )
    require(
        value.set_udm_type(index, replacement) == ida_typeinf.TERR_OK,
        f"could not set {struct_name}::{member_name} to {type_name}",
    )
    require(
        value.set_udm_cmt(index, comment, True) == ida_typeinf.TERR_OK,
        f"could not comment {struct_name}::{member_name}",
    )
    print(
        f"TYPE_MEMBER_CORRECTED\t{struct_name}\t{member_name}\t"
        f"0x{expected_offset:X}\t{type_name}"
    )


def normalized_binary64_type(
    source: ida_typeinf.tinfo_t,
) -> tuple[ida_typeinf.tinfo_t, bool]:
    """Return ``source`` with every nested IDA long-double node made double."""
    if source.is_ldouble():
        result = ida_typeinf.tinfo_t("double")
        result.set_modifiers(source.get_modifiers())
        return result, True

    if source.is_ptr():
        details = ida_typeinf.ptr_type_data_t()
        require(source.get_ptr_details(details), "could not read pointer type")
        nested, changed = normalized_binary64_type(details.obj_type)
        if not changed:
            return source.copy(), False
        details.obj_type = nested
        result = ida_typeinf.tinfo_t()
        require(result.create_ptr(details), "could not rebuild pointer type")
        result.set_modifiers(source.get_modifiers())
        return result, True

    if source.is_array():
        details = ida_typeinf.array_type_data_t()
        require(source.get_array_details(details), "could not read array type")
        nested, changed = normalized_binary64_type(details.elem_type)
        if not changed:
            return source.copy(), False
        details.elem_type = nested
        result = ida_typeinf.tinfo_t()
        require(result.create_array(details), "could not rebuild array type")
        result.set_modifiers(source.get_modifiers())
        return result, True

    if source.is_func():
        details = ida_typeinf.func_type_data_t()
        require(source.get_func_details(details), "could not read function type")
        details.rettype, changed = normalized_binary64_type(details.rettype)
        for argument in details:
            argument.type, argument_changed = normalized_binary64_type(
                argument.type
            )
            changed = changed or argument_changed
        if not changed:
            return source.copy(), False
        result = ida_typeinf.tinfo_t()
        require(result.create_func(details), "could not rebuild function type")
        result.set_modifiers(source.get_modifiers())
        return result, True

    return source.copy(), False


def normalize_imported_binary64_types() -> None:
    """Replace the legacy importer spelling of IVP_DOUBLE in every IVP UDT.

    Ballance's selected configuration defines IVP_DOUBLE as C++ ``double``.
    The PE is an x86 MSVC image and every affected imported member occupies
    exactly eight bytes (or an array/pointer/function type built from that
    scalar).  IDA's ``long double`` spelling came from the source type import;
    it is not a distinct retail type.  Work from a snapshot so changing one
    vtable/record cannot invalidate the ordinal traversal.
    """
    idati = ida_typeinf.get_idati()
    pending: list[tuple[str, str, int, int]] = []
    owner_layouts: dict[str, tuple[int, int]] = {}
    for ordinal in range(1, ida_typeinf.get_ordinal_count(idati)):
        name = ida_typeinf.get_numbered_type_name(idati, ordinal) or ""
        if not (name.startswith("IVP_") or name.startswith("IVV_")):
            continue
        value = ida_typeinf.tinfo_t()
        if not value.get_numbered_type(idati, ordinal) or not value.is_udt():
            continue
        members = ida_typeinf.udt_type_data_t()
        if not value.get_udt_details(members):
            continue
        for member in members:
            rendered = member.type.dstr()
            if "long double" not in rendered:
                continue
            pending.append(
                (
                    name,
                    member.name,
                    member.offset,
                    member.size,
                )
            )
            owner_layouts.setdefault(name, (value.get_size(), members.sda))

    corrected = 0
    for struct_name, member_name, offset, size in pending:
        value = ida_typeinf.tinfo_t()
        require(
            value.get_named_type(None, struct_name, ida_typeinf.BTF_STRUCT),
            f"missing IVP UDT while normalizing binary64: {struct_name}",
        )
        members = ida_typeinf.udt_type_data_t()
        require(
            value.get_udt_details(members),
            f"could not reread IVP UDT while normalizing: {struct_name}",
        )
        matches = [
            (index, member)
            for index, member in enumerate(members)
            if member.name == member_name and member.offset == offset
        ]
        require(
            len(matches) == 1,
            f"lost {struct_name}::{member_name} at bit offset {offset}",
        )
        index, member = matches[0]
        if "long double" not in member.type.dstr():
            continue
        replacement, changed = normalized_binary64_type(member.type)
        require(
            changed and "long double" not in replacement.dstr(),
            f"could not transform {struct_name}::{member_name} binary64 type",
        )
        require(
            replacement.get_size() * 8 == size,
            f"binary64 normalization changes {struct_name}::{member_name} size",
        )
        require(
            value.set_udm_type(index, replacement) == ida_typeinf.TERR_OK,
            f"could not normalize {struct_name}::{member_name}",
        )
        corrected += 1
        print(
            f"TYPE_BINARY64_NORMALIZED\t{struct_name}\t{member_name}\t"
            f"0x{offset // 8:X}\t{replacement.dstr()}"
        )

    # set_udm_type() preserves bit offsets but can silently replace an
    # imported UDT's explicit eight-byte alignment (IDA sda=4) with sda=3.
    # Restore the pre-edit declaration and require the complete record size
    # to remain byte-for-byte stable.  Natural alignment has sda=0 and needs
    # no override.
    for struct_name, (expected_size, original_sda) in owner_layouts.items():
        value = ida_typeinf.tinfo_t()
        require(
            value.get_named_type(None, struct_name, ida_typeinf.BTF_STRUCT),
            f"missing IVP UDT while restoring alignment: {struct_name}",
        )
        if original_sda:
            require(
                value.set_udt_alignment(original_sda)
                == ida_typeinf.TERR_OK,
                f"could not restore {struct_name} sda={original_sda}",
            )
        require(
            value.get_size() == expected_size,
            f"binary64 normalization changed {struct_name} size from "
            f"0x{expected_size:X} to 0x{value.get_size():X}",
        )
        print(
            f"TYPE_BINARY64_OWNER_VERIFIED\t{struct_name}\t"
            f"0x{expected_size:X}\tsda={original_sda}"
        )

    # Recovery path for an IDB written by the older non-idempotent pass.  The
    # exact expected sizes are the source-imported layouts corroborated by
    # retail stack/allocation boundaries where the types are instantiated.
    explicit_alignment_layouts = {
        "IVP_Debug_Manager": 0x68,
        "IVP_Friction_Solver": 0x840,
        "IVP_Impact_Solver": 0x128,
        "IVP_Clustering_Visualizer_Shortrange_Callback": 0x68,
        "IVP_Extra_Info": 0x40,
        "IVP_Geompack": 0x90,
    }
    for struct_name, expected_size in explicit_alignment_layouts.items():
        value = ida_typeinf.tinfo_t()
        require(
            value.get_named_type(None, struct_name, ida_typeinf.BTF_STRUCT),
            f"missing explicitly aligned IVP UDT: {struct_name}",
        )
        members = ida_typeinf.udt_type_data_t()
        require(
            value.get_udt_details(members),
            f"could not read explicitly aligned IVP UDT: {struct_name}",
        )
        if value.get_alignment() != 8 or value.get_size() != expected_size:
            require(
                value.set_udt_alignment(4) == ida_typeinf.TERR_OK,
                f"could not restore {struct_name} eight-byte alignment",
            )
        require(
            value.get_alignment() == 8
            and value.get_size() == expected_size,
            f"unexpected {struct_name} layout after alignment recovery: "
            f"size=0x{value.get_size():X}, align={value.get_alignment()}",
        )
        print(
            f"TYPE_BINARY64_EXPLICIT_ALIGNMENT\t{struct_name}\t"
            f"0x{expected_size:X}"
        )

    remaining = 0
    for ordinal in range(1, ida_typeinf.get_ordinal_count(idati)):
        name = ida_typeinf.get_numbered_type_name(idati, ordinal) or ""
        if not (name.startswith("IVP_") or name.startswith("IVV_")):
            continue
        value = ida_typeinf.tinfo_t()
        if not value.get_numbered_type(idati, ordinal) or not value.is_udt():
            continue
        members = ida_typeinf.udt_type_data_t()
        if value.get_udt_details(members):
            remaining += sum(
                "long double" in member.type.dstr() for member in members
            )
    require(remaining == 0, f"{remaining} imported long-double UDT members remain")
    print(f"TYPE_BINARY64_NORMALIZED_MEMBERS\t{corrected}")


def remove_trailing_struct_member(
    struct_name: str,
    expected_old_size: int,
    member_name: str,
    expected_offset: int,
    expected_new_size: int,
) -> None:
    """Remove one source-imported tail field disproved by retail allocation."""
    value = ida_typeinf.tinfo_t()
    require(
        value.get_named_type(None, struct_name, ida_typeinf.BTF_STRUCT),
        f"missing imported struct {struct_name}",
    )
    current_size = value.get_size()
    if current_size == expected_new_size:
        print(
            f"TYPE_MEMBER_ALREADY_REMOVED\t{struct_name}\t{member_name}\t"
            f"0x{expected_offset:X}"
        )
        return
    require(
        current_size == expected_old_size,
        f"unexpected {struct_name} size 0x{current_size:X}",
    )
    members = ida_typeinf.udt_type_data_t()
    require(
        value.get_udt_details(members),
        f"could not read members for {struct_name}",
    )
    matches = [
        index
        for index, member in enumerate(members)
        if member.name == member_name
        and member.offset == expected_offset * 8
    ]
    require(
        len(matches) == 1,
        f"missing {struct_name}::{member_name} at 0x{expected_offset:X}",
    )
    require(
        value.del_udm(matches[0]) == ida_typeinf.TERR_OK,
        f"could not remove {struct_name}::{member_name}",
    )
    require(
        value.get_size() == expected_new_size,
        f"{struct_name} did not shrink to 0x{expected_new_size:X}",
    )
    print(
        f"TYPE_MEMBER_REMOVED\t{struct_name}\t{member_name}\t"
        f"0x{expected_offset:X}\t0x{expected_new_size:X}"
    )


def set_function(address: int, name: str, declaration: str, comment: str) -> None:
    require(
        ida_funcs.get_func_start(address) == address,
        f"expected function boundary at 0x{address:08X}",
    )
    require(
        ida_name.set_name(
            address, name, ida_name.SN_FORCE | ida_name.SN_NOWARN
        ),
        f"could not rename 0x{address:08X} to {name}",
    )
    require(
        idc.SetType(address, declaration),
        f"could not apply type at 0x{address:08X}: {declaration}",
    )
    ida_bytes.set_cmt(address, comment, True)
    print(f"CORRECTED\t0x{address:08X}\t{name}\t{declaration}")


def append_repeatable_function_comment(address: int, addition: str) -> None:
    require(
        ida_funcs.get_func_start(address) == address,
        f"expected function boundary at 0x{address:08X}",
    )
    comment = ida_bytes.get_cmt(address, True) or ""
    if addition not in comment:
        separator = " " if comment and not comment.endswith((" ", "\n")) else ""
        require(
            ida_bytes.set_cmt(address, comment + separator + addition, True),
            f"could not append function comment at 0x{address:08X}",
        )
    print(f"OWNERSHIP_EVIDENCE\t0x{address:08X}")


def set_data(address: int, name: str, declaration: str, comment: str) -> None:
    require(
        ida_name.set_name(
            address, name, ida_name.SN_FORCE | ida_name.SN_NOWARN
        ),
        f"could not rename data at 0x{address:08X} to {name}",
    )
    require(
        idc.SetType(address, declaration),
        f"could not apply data type at 0x{address:08X}: {declaration}",
    )
    ida_bytes.set_cmt(address, comment, True)
    print(f"DATA_CORRECTED\t0x{address:08X}\t{name}\t{declaration}")


def set_named_address(address: int, name: str, comment: str) -> None:
    require(
        ida_name.set_name(
            address, name, ida_name.SN_FORCE | ida_name.SN_NOWARN
        ),
        f"could not rename address 0x{address:08X} to {name}",
    )
    ida_bytes.set_cmt(address, comment, True)
    print(f"ADDRESS_CORRECTED\t0x{address:08X}\t{name}")


def stage_friction_event_name_swaps() -> None:
    """Free the four decorated names before swapping their semantic owners."""
    for address in (0x1000A8A0, 0x1000A900, 0x10013BC0, 0x10013C00):
        temporary_name = f"ivp_friction_event_name_swap_{address:08X}"
        require(
            ida_name.set_name(
                address,
                temporary_name,
                ida_name.SN_FORCE | ida_name.SN_NOWARN,
            ),
            f"could not stage friction-event name at 0x{address:08X}",
        )


def export_named_functions(path: Path) -> None:
    records: list[tuple[str, int]] = []
    for address in idautils.Functions():
        name = idc.get_func_name(address)
        if not name or name.startswith(("sub_", "j_sub_")):
            continue
        records.append((name, address - IMAGE_BASE))
    records.sort(key=lambda record: record[0])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        "".join(f"{name}\t0x{rva:X}\n" for name, rva in records),
        encoding="utf-8",
    )
    print(f"EXPORTED\t{len(records)}\t{path}")


def check_and_annotate_retail_image() -> None:
    ida_auto.auto_wait()
    require(ida_nalt.get_imagebase() == IMAGE_BASE, "unexpected image base")
    require(ida_ida.inf_get_min_ea() <= 0x100042B0, "retail text is absent")

    # Stable retail listener-table anchors. These also prevent the correction
    # from being applied to a nearby IVP build with a different virtual ABI.
    require(
        ida_bytes.get_dword(0x10063214) == 0x100042B0,
        "PhysicsCollDetectionListener slot 0 does not match retail",
    )
    require(
        ida_bytes.get_dword(0x10063224) == 0x10003EB0,
        "PhysicsCollDetectionListener deleting-destructor slot does not match retail",
    )
    require(
        ida_bytes.get_byte(0x10013BA1) == 0x01
        and ida_bytes.get_byte(0x10013BE1) == 0x04,
        "environment listener callback masks do not match retail",
    )
    require(
        idc.get_operand_value(0x10022288, 0) == 0x10013BC0
        and idc.get_operand_value(0x100222A1, 0) == 0x1000A8A0
        and idc.get_operand_value(0x1001C2B6, 0) == 0x10013C00
        and idc.get_operand_value(0x1001C2CF, 0) == 0x1000A900,
        "friction contact creation/destruction call graph does not match retail",
    )
    require(
        ida_bytes.get_byte(0x10015FD0) == 0xB9
        and ida_bytes.get_dword(0x10015FD1) == 0x10075DB0,
        "mindist-settings static initializer does not match retail",
    )
    require(
        ida_bytes.get_bytes(0x10015E50, 18)
        == bytes.fromhex(
            "8B D1 57 B9 18 00 00 00 33 C0 8B FA F3 AB 8B C2 5F C3"
        ),
        "statistic-manager 0x60 clear constructor does not match retail",
    )
    require(
        ida_bytes.get_bytes(0x100129EB, 5)
        == bytes.fromhex("68 90 01 00 00")
        and ida_bytes.get_bytes(0x10028292, 6)
        == bytes.fromhex("8D 4E 70 C7 44 24")
        and ida_bytes.get_bytes(0x100282A2, 6)
        == bytes.fromhex("8D 8E F8 00 00 00")
        and ida_bytes.get_bytes(0x100375B2, 12)
        == bytes.fromhex("8D 48 10 66 C7 40 08 02 00 89 48 0C"),
        "constraint 0x18/0x190 retail layout anchors do not match",
    )
    require(
        tuple(ida_bytes.get_dword(0x100633C0 + slot * 4) for slot in range(2))
        == (0x1001EE50, 0x1000A5B0),
        "IVP_Collision_Callback_Table_Hash vtable does not match retail",
    )
    require(
        ida_bytes.get_bytes(0x10009776, 22)
        == bytes.fromhex(
            "8B 4D 10 8B 55 0C 51 8B 4C 24 58 52 51 53 56 8B C8 "
            "E8 74 3C 00 00"
        ),
        "Real Object constructor no longer passes template +0x10 to IVP_Core",
    )
    require(
        tuple(ida_bytes.get_dword(0x100633C8 + slot * 4) for slot in range(2))
        == (0x1001EE50, 0x1000A590),
        "IVP_Object_Callback_Table_Hash vtable does not match retail",
    )
    require(
        tuple(ida_bytes.get_bytes(0x1000A9FE, 3)) == (0xFF, 0x50, 0x0C)
        and tuple(ida_bytes.get_bytes(0x1000AA4E, 3)) == (0xFF, 0x50, 0x04)
        and tuple(ida_bytes.get_bytes(0x1001722C, 3)) == (0x8B, 0x48, 0x2C)
        and tuple(ida_bytes.get_bytes(0x10017260, 2)) == (0xFF, 0x12),
        "IVP_Universe_Manager dispatch sites do not match retail",
    )
    require(
        tuple(ida_bytes.get_dword(0x100633D0 + slot * 4) for slot in range(7))
        == (
            0x1000A490,
            0x10037600,
            0x1001D600,
            0x1001CBC0,
            0x1001D750,
            0x1000B0C0,
            0x1000B0D0,
        ),
        "IVP_Friction_System primary vtable does not match retail",
    )
    require(
        tuple(ida_bytes.get_dword(0x100633EC + slot * 4) for slot in range(7))
        == (
            0x1000A490,
            0x10004C10,
            0x10004C50,
            0x10004C20,
            0x1001D610,
            0x1000B080,
            0x1000B090,
        )
        and tuple(
            ida_bytes.get_dword(0x10063408 + slot * 4)
            for slot in range(7)
        )
        == (
            0x1001D9A0,
            0x10004C10,
            0x10004C50,
            0x10004C20,
            0x1001D6C0,
            0x1000B0B0,
            0x1000B090,
        ),
        "embedded friction-controller vtables do not match retail",
    )
    require(
        tuple(ida_bytes.get_bytes(0x1000B360, 7))
        == (0x8B, 0x44, 0x24, 0x04, 0x89, 0x48, 0x70)
        and tuple(ida_bytes.get_bytes(0x1000B37D, 5))
        == (0x66, 0xFF, 0x41, 0x3E, 0xC2),
        "IVP_Friction_System::add_dist_to_system body does not match retail",
    )
    require(
        ida_bytes.get_byte(0x1000B301) == 0x6A
        and ida_bytes.get_byte(0x1000B302) == 0x38
        and idc.get_operand_value(0x1000B319, 0) == 0x1001D340,
        "IVP_Friction_Core_Pair allocation/constructor path does not match retail",
    )
    require(
        ida_bytes.get_byte(0x10020071) == 0x6A
        and ida_bytes.get_byte(0x10020072) == 0x78
        and idc.get_operand_value(0x1002008E, 0) == 0x1001A8B0,
        "IVP_Contact_Point allocation/constructor path does not match retail",
    )
    require(
        idc.get_operand_value(0x1001CCA3, 0) == 0x1001CCD0
        and idc.get_operand_value(0x1001CCFA, 0) == 0x1001D2A0,
        "friction-core-pair real-friction energy path does not match retail",
    )
    require(
        tuple(ida_bytes.get_bytes(0x1000B015, 10))
        == (0x8B, 0xC1, 0xC7, 0x40, 0x08, 0x08, 0x34, 0x06, 0x10, 0xC7)
        and tuple(ida_bytes.get_bytes(0x1001D36C, 11))
        == (0xC7, 0x40, 0x18, 0x01, 0x00, 0x00, 0x00, 0x83, 0xC4, 0x08, 0xC3),
        "friction-system/core-pair constructors do not match retail",
    )
    require(
        ida_bytes.get_dword(0x1000B01A) == 0x10063408
        and ida_bytes.get_dword(0x1000B021) == 0x100633EC,
        "friction-system embedded controller offsets do not match retail",
    )
    require(
        tuple(ida_bytes.get_bytes(0x10036D85, 6))
        == (0x81, 0xEC, 0x40, 0x08, 0x00, 0x00)
        and idc.get_operand_value(0x10036DEA, 0) == 0x1001C350
        and tuple(ida_bytes.get_bytes(0x1001C374, 12))
        == (0x8D, 0x46, 0x38, 0x66, 0xC7, 0x46, 0x30, 0x00,
            0x02, 0x89, 0x46, 0x34),
        "0x840 friction-solver stack/layout anchors do not match retail",
    )
    require(
        tuple(ida_bytes.get_bytes(0x100368BB, 6))
        == (0x8B, 0x57, 0x60, 0x8B, 0x4F, 0x78)
        and tuple(ida_bytes.get_bytes(0x100369BE, 6))
        == (0x8B, 0x57, 0x64, 0x8B, 0x4F, 0x7C)
        and idc.get_operand_value(0x10036A30, 0) == 0x10036B30,
        "inlined friction distance-matrix column bodies do not match retail",
    )
    ida_bytes.set_cmt(
        0x100368BB,
        "Inlined calc_distance_matrix_column for contact core[0]: walks "
        "friction_infos[0], applies the negative orientation and accumulates "
        "matrix[row * aligned_row_len + current_column].",
        True,
    )
    ida_bytes.set_cmt(
        0x100369BE,
        "Inlined calc_distance_matrix_column for contact core[1]: walks "
        "friction_infos[1], applies the positive orientation and accumulates "
        "matrix[row * aligned_row_len + current_column].",
        True,
    )
    require(
        tuple(ida_bytes.get_bytes(0x1001B117, 6))
        == (0x8B, 0x5E, 0x78, 0x8B, 0x56, 0x7C)
        and tuple(ida_bytes.get_bytes(0x1001B713, 6))
        == (0x8D, 0x86, 0x90, 0x00, 0x00, 0x00),
        "inlined wheel-friction specialization does not match retail",
    )
    ida_bytes.set_cmt(
        0x1001B117,
        "Start of the inlined friction_force_local_constraint_2d_wheel "
        "specialization. Ballance folds the nearby private helper into "
        "IVP_Contact_Point::friction_force_local_constraint_2d; this block "
        "handles a lone car-wheel core and returns at 0x1001B70C.",
        True,
    )
    ida_bytes.set_cmt(
        0x1001B713,
        "Generic two-core/two-axis friction fallback after the inlined "
        "wheel specialization declines the contact.",
        True,
    )
    require(
        all(
            ida_funcs.get_func_start(address) == address
            for address in (
                0x1001B080, 0x1001B8B0, 0x1001BC20, 0x10024140,
                0x1001C1F0, 0x1001C550, 0x1001CC90,
                0x1001CCB0, 0x100362C0, 0x10036B80, 0x10036C60,
                0x10036CB0, 0x10036D10, 0x10036D70, 0x10036EC0,
                0x10036F80, 0x10036FA0, 0x10036FC0, 0x1001D610,
                0x1001D660, 0x1001D6C0, 0x1001D9A0, 0x1000B080,
                0x1000B090, 0x1000B0B0, 0x1001C350,
                0x10036760, 0x100364C0, 0x1001C070, 0x1001BDF0,
                0x10036170, 0x10036B30, 0x10036190, 0x100366A0,
                0x100362E0, 0x10036080,
            )
        ),
        "retained friction-system operation boundaries do not match retail",
    )
    require(
        tuple(ida_bytes.get_bytes(0x10024160, 8))
        == (0x8B, 0x4E, 0x7C, 0x85, 0xC9, 0x75, 0x5B, 0x8B)
        and tuple(ida_bytes.get_bytes(0x100242B5, 9))
        == (0x5F, 0x5E, 0x81, 0xC4, 0x3C, 0x01, 0x00, 0x00, 0xC2)
        and ida_bytes.get_word(0x100242BE) == 0x000C,
        "long-term impact solver body/stack cleanup does not match retail",
    )
    require(
        tuple(ida_bytes.get_bytes(0x1001CD20, 8))
        == (0xDD, 0x44, 0x24, 0x0C, 0xDC, 0x44, 0x24, 0x14),
        "mutual-energizer impulse-helper prologue does not match retail",
    )
    require(
        tuple(ida_bytes.get_bytes(0x1001CD60, 8))
        == (0xDD, 0x44, 0x24, 0x04, 0xDD, 0x44, 0x24, 0x1C),
        "mutual-energizer static energy prologue does not match retail",
    )
    require(
        ida_bytes.get_byte(0x1001CDB0) == 0x55
        and ida_bytes.get_word(0x1001CFF9) == 0xC25D
        and ida_bytes.get_word(0x1001CFFB) == 0x0008,
        "mutual-energizer initializer boundaries do not match retail",
    )
    require(
        idc.get_operand_value(0x1001D08B, 0) == 0x1001CD60,
        "mutual-energizer calculation call does not match retail",
    )
    require(
        idc.get_operand_value(0x1001D115, 0) == 0x1001CD20,
        "mutual-energizer impulse call does not match retail",
    )
    require(
        idc.get_operand_value(0x1001D2B2, 0) == 0x1001CDB0,
        "friction-pair mutual-energizer initialization call does not match retail",
    )
    require(
        tuple(ida_bytes.get_bytes(0x1001BC73, 12))
        == (0xD9, 0x40, 0x54, 0xD8, 0x48, 0x44, 0xD8, 0x48,
            0x30, 0xDE, 0xC1, 0x75)
        and idc.get_operand_value(0x1001BD34, 0) == 0x1001B080
        and idc.get_operand_value(0x1001BD43, 0) == 0x1001B8B0,
        "inlined friction-pair slide-way/constraint dispatch does not match retail",
    )
    require(
        tuple(ida_bytes.get_bytes(0x1001C680, 7))
        == (0x33, 0xC0, 0x66, 0x8B, 0x41, 0x02, 0xC3)
        and idc.get_operand_value(0x1000B335, 0) == 0x1001C6F0
        and idc.get_operand_value(0x1000B255, 0) == 0x1001C720
        and idc.get_operand_value(0x1001C9DF, 0) == 0x1001C720
        and idc.get_operand_value(0x1001C9E9, 0) == 0x1001C6F0,
        "friction pair-vector operations do not match retail",
    )
    require(
        tuple(ida_bytes.get_dword(0x1006344C + slot * 4) for slot in range(6))
        == (
            0x10060762,
            0x10060762,
            0x10060762,
            0x10060762,
            0x10060762,
            0x1000BE10,
        ),
        "IVP_Material base vtable does not match retail",
    )
    require(
        tuple(ida_bytes.get_dword(0x10063464 + slot * 4) for slot in range(6))
        == (
            0x1000BED0,
            0x1000BF40,
            0x1000BEE0,
            0x1000BEF0,
            0x1000BF80,
            0x1000BF50,
        ),
        "IVP_Material_Simple vtable does not match retail",
    )
    require(
        tuple(ida_bytes.get_dword(0x10063240 + slot * 4) for slot in range(7))
        == (
            0x10004760,
            0x10004C10,
            0x10004C50,
            0x10004C20,
            0x100046B0,
            0x100046A0,
            0x10004780,
        ),
        "PhysicsControllerForce vtable does not match retail",
    )
    require(
        ida_bytes.get_dword(0x10004A07) == 0x10063240,
        "PhysicsForceCall constructor does not install the expected vtable",
    )
    require(
        tuple(ida_bytes.get_dword(0x1006325C + slot * 4) for slot in range(7))
        == (
            0x10022170,
            0x10004C10,
            0x10060762,
            0x10004C20,
            0x10060762,
            0x10060762,
            0x10004C30,
        ),
        "IVP_Controller base vtable does not match retail",
    )
    require(
        tuple(ida_bytes.get_dword(0x10063390 + slot * 4) for slot in range(3))
        == (0x10009810, 0x10009520, 0x100093D0),
        "IVP_Real_Object vtable does not match retail",
    )
    require(
        ida_bytes.get_dword(0x100633A0) == 0x10009B40,
        "IVP_Object vtable does not match retail",
    )
    require(
        ida_bytes.get_dword(0x100633A4) == 0x10009C60,
        "IVP_Cluster vtable does not match retail",
    )
    require(
        tuple(ida_bytes.get_dword(0x10063434 + slot * 4) for slot in range(6))
        == (
            0x1000BE30,
            0x1000BD10,
            0x1000BD40,
            0x1000BD70,
            0x1000BDE0,
            0x1000BDC0,
        ),
        "IVP_Material_Manager vtable does not match retail",
    )
    require(
        tuple(ida_bytes.get_dword(0x10063524 + slot * 4) for slot in range(6))
        == (
            0x100106B0,
            0x10010720,
            0x10010740,
            0x10010790,
            0x10010630,
            0x10010640,
        ),
        "IVP_Attacher_To_Cores_Buoyancy vtable does not match retail",
    )
    require(
        tuple(ida_bytes.get_dword(0x10063A00 + slot * 4) for slot in range(5))
        == (
            0x1002DDA0,
            0x1002DDC0,
            0x1002DDB0,
            0x1001A810,
            0x1002DC50,
        )
        and ida_bytes.get_dword(0x1002DC29) == 0x10063A00,
        "IVP_OV_Element final vtable/constructor anchor does not match retail",
    )
    require(
        tuple(ida_bytes.get_dword(0x10063A24 + slot * 4) for slot in range(2))
        == (0x10037DD0, 0x1002E0C0)
        and ida_bytes.get_dword(0x1002E064) == 0x10063A24
        and ida_bytes.get_dword(0x10037D82) == 0x10063A24,
        "IVP_ov_tree_hash final vtable/constructor/destructor anchors do not match retail",
    )
    require(
        tuple(ida_bytes.get_dword(0x10063A50 + slot * 4) for slot in range(2))
        == (0x1002F640, 0x1002F610),
        "IVP_Anomaly_Limits vtable does not match retail",
    )
    require(
        tuple(ida_bytes.get_dword(0x10063A58 + slot * 4) for slot in range(7))
        == (
            0x1002F6D0,
            0x1002F720,
            0x1002F8F0,
            0x1002FCC0,
            0x1002F6B0,
            0x1002FC80,
            0x1002F680,
        ),
        "IVP_Anomaly_Manager vtable does not match retail",
    )
    require(
        tuple(ida_bytes.get_dword(0x10063604 + slot * 4) for slot in range(8))
        == (
            0x10028950,
            0x10004C10,
            0x10013FA0,
            0x10004C20,
            0x100146D0,
            0x10013FB0,
            0x10014350,
            0x10028950,
        ),
        "IVP_Actuator_Spring vtable does not match retail",
    )
    require(
        tuple(ida_bytes.get_dword(0x10063628 + slot * 4) for slot in range(8))
        == (
            0x10028950,
            0x10004C10,
            0x10013FA0,
            0x10004C20,
            0x100146D0,
            0x10013FB0,
            0x100145F0,
            0x10028950,
        )
        and ida_bytes.get_dword(0x10063624) == 0x10014440,
        "IVP_Actuator_Spring_Active vtables do not match retail",
    )
    require(
        tuple(ida_bytes.get_dword(0x10063598 + slot * 4) for slot in range(7))
        == (
            0x10022170,
            0x10004C10,
            0x10004C50,
            0x10004C20,
            0x10012010,
            0x10013230,
            0x1000B090,
        ),
        "IVP_Standard_Gravity_Controller vtable does not match retail",
    )
    require(
        tuple(ida_bytes.get_dword(0x100635C0 + slot * 4) for slot in range(8))
        == (
            0x10028950,
            0x10004C10,
            0x10013FA0,
            0x10004C20,
            0x10060762,
            0x10013FB0,
            0x10013FC0,
            0x10028950,
        ),
        "IVP_Actuator base vtable does not match retail",
    )
    require(
        tuple(ida_bytes.get_dword(0x100635E0 + slot * 4) for slot in range(8))
        == (
            0x10028950,
            0x10004C10,
            0x10013FA0,
            0x10004C20,
            0x10060762,
            0x10013FB0,
            0x10014130,
            0x10028950,
        ),
        "IVP_Actuator_Two_Point vtable does not match retail",
    )
    require(
        idc.get_operand_value(0x1002F48F, 0) == 0x88
        and idc.get_operand_value(0x1002F4B7, 0) == 0x98
        and idc.get_operand_value(0x100308AD, 1) == 0x10063AD0
        and idc.get_operand_value(0x100308B3, 1) == 0x10063AC8,
        "IVP_Mindist/Recursive allocation or dual-vptr layout does not match retail",
    )
    require(
        tuple(ida_bytes.get_dword(0x100637B4 + slot * 4) for slot in range(8))
        == (
            0x100181B0,
            0x10016410,
            0x10016430,
            0x10016190,
            0x100162D0,
            0x10019770,
            0x10016F70,
            0x100240A0,
        )
        and idc.get_operand_value(0x1001629F, 1) == 0x100637B4,
        "IVP_Mindist eight-slot vtable does not match retail",
    )
    require(
        tuple(ida_bytes.get_dword(0x10063A34 + slot * 4) for slot in range(2))
        == (0x10060762, 0x1002F580)
        and tuple(
            ida_bytes.get_dword(0x10063A3C + slot * 4)
            for slot in range(5)
        )
        == (
            0x1002F3F0,
            0x1002F5C0,
            0x1002F3B0,
            0x1002F430,
            0x1002F3E0,
        )
        and idc.get_operand_value(0x1002F5AA, 1) == 0x10063A3C,
        "Collision Delegator base/Root Mindist vtable split does not match retail",
    )
    require(
        tuple(ida_bytes.get_dword(0x100636D0 + slot * 4) for slot in range(2))
        == (
            0x10015140,
            0x100150D0,
        ),
        "IVP_Active_Value_Hash vtable does not match retail",
    )
    require(
        tuple(ida_bytes.get_dword(0x100636D8 + slot * 4) for slot in range(15))
        == (
            0x10015280,
            0x10015260,
            0x10015410,
            0x10015520,
            0x10015490,
            0x10015560,
            0x100155A0,
            0x100155F0,
            0x10015640,
            0x10015360,
            0x100156D0,
            0x10015700,
            0x100157A0,
            0x100158E0,
            0x10015840,
        ),
        "IVP_U_Active_Value_Manager vtable does not match retail",
    )
    require(
        ida_bytes.get_bytes(0x10033E70, 9)
        == bytes.fromhex("8B411024F8894110C3"),
        "IVP_Great_Matrix_Many_Zero alignment body does not match retail",
    )
    require(
        ida_bytes.get_bytes(0x10033A30, 4) == bytes.fromhex("8B442410")
        and ida_bytes.get_bytes(0x10033A56, 6)
        == bytes.fromhex("8DBED8000000")
        and ida_bytes.get_bytes(0x10033A5E, 6)
        == bytes.fromhex("899E38010000")
        and ida_bytes.get_bytes(0x10033ACA, 3)
        == bytes.fromhex("C21800")
        and ida_bytes.get_bytes(0x10033AD0, 6)
        == bytes.fromhex("558BEC83E4F8"),
        "IVP_Solver_Core_Reaction retained bodies do not match retail",
    )
    require(
        tuple(ida_bytes.get_dword(0x10063B20 + slot * 4) for slot in range(23))
        == (
            0x10028950,
            0x10037600,
            0x10028240,
            0x10004C20,
            0x10060762,
            0x10028230,
            0x100375E0,
            0x100376D0,
            0x100376D0,
            0x100376E0,
            0x100376E0,
            0x100376F0,
            0x10037700,
            0x10037710,
            0x10037720,
            0x10037730,
            0x10037730,
            0x10037740,
            0x10037750,
            0x10037760,
            0x10037770,
            0x10037780,
            0x10037780,
        ),
        "IVP_Constraint base vtable does not match retail",
    )
    require(
        tuple(ida_bytes.get_dword(0x10063930 + slot * 4) for slot in range(23))
        == (
            0x10028950,
            0x10037600,
            0x10028240,
            0x10004C20,
            0x10028960,
            0x10028230,
            0x10028250,
            0x1002A210,
            0x1002A2E0,
            0x1002A380,
            0x1002A430,
            0x1002A4A0,
            0x1002A4D0,
            0x1002A500,
            0x1002A540,
            0x1002A5C0,
            0x1002A6C0,
            0x1002A760,
            0x1002A790,
            0x1002A7C0,
            0x1002A800,
            0x1002A880,
            0x1002A920,
        ),
        "IVP_Constraint_Local vtable does not match retail",
    )
    require(
        tuple(ida_bytes.get_dword(0x100631E0 + slot * 4) for slot in range(11))
        == (
            0x1000BCD0,
            0x1000BC00,
            0x1000BC40,
            0x1000BC20,
            0x1000BB30,
            0x1000BBE0,
            0x1000BB00,
            0x1000A490,
            0x1000A490,
            0x10002DA0,
            0x1000BCF0,
        ),
        "IVP_SurfaceManager_Polygon vtable does not match retail",
    )
    require(
        tuple(ida_bytes.get_dword(0x10063D34 + slot * 4) for slot in range(2))
        == (0x10060C10, 0x10060C50),
        "IVP_BetterDebugmanager vtable does not match retail",
    )
    require(
        tuple(ida_bytes.get_dword(0x10063890 + slot * 4) for slot in range(11))
        == (
            0x10060762,
            0x10060762,
            0x10060762,
            0x10060762,
            0x10060762,
            0x10060762,
            0x10060762,
            0x10022170,
            0x10022170,
            0x10060762,
            0x10060762,
        ),
        "IVP_SurfaceManager base vtable does not match retail",
    )
    require(
        ida_bytes.get_bytes(0x100099A0, 6) == bytes.fromhex("558BE98B4524")
        and ida_bytes.get_bytes(0x100138F0, 6)
        == bytes.fromhex("8B8138010000")
        and ida_bytes.get_bytes(0x100187A0, 6)
        == bytes.fromhex("568B74240857")
        and ida_bytes.get_bytes(0x1001A820, 6)
        == bytes.fromhex("83EC08535556")
        and ida_bytes.get_bytes(0x1001E300, 6)
        == bytes.fromhex("558BEC83E4F8")
        and ida_bytes.get_bytes(0x1001E730, 6)
        == bytes.fromhex("D94128D86118")
        and ida_bytes.get_bytes(0x1001E750, 6)
        == bytes.fromhex("DD442404DC21")
        and ida_bytes.get_bytes(0x1001E870, 6)
        == bytes.fromhex("558BEC83E4F8")
        and ida_bytes.get_bytes(0x1001EA50, 6)
        == bytes.fromhex("83EC188B4424")
        and ida_bytes.get_bytes(0x1001EB10, 6)
        == bytes.fromhex("53558B6C2410"),
        "beam/next-PSI retained helper bodies do not match retail",
    )
    require(
        ida_bytes.get_bytes(0x100116F0, 6)
        == bytes.fromhex("568BF18B4608")
        and ida_bytes.get_bytes(0x10011890, 6)
        == bytes.fromhex("6AFF68D61406")
        and ida_bytes.get_bytes(0x10011920, 6)
        == bytes.fromhex("64A100000000")
        and ida_bytes.get_bytes(0x100119D0, 6)
        == bytes.fromhex("668B410E5666")
        and ida_bytes.get_bytes(0x10011E70, 6)
        == bytes.fromhex("83EC088BC1C7")
        and ida_bytes.get_bytes(0x10012010, 6)
        == bytes.fromhex("558BEC83E4F8")
        and ida_bytes.get_bytes(0x100121B0, 6)
        == bytes.fromhex("558BEC83E4F8")
        and ida_bytes.get_bytes(0x10011300, 6)
        == bytes.fromhex("51538BD933C0")
        and ida_bytes.get_bytes(0x10011470, 6)
        == bytes.fromhex("64A100000000")
        and ida_bytes.get_bytes(0x10011770, 6)
        == bytes.fromhex("568BF1E818FF")
        and ida_bytes.get_bytes(0x10011DB0, 6)
        == bytes.fromhex("83EC08535556")
        and ida_bytes.get_bytes(0x1001C7D0, 6)
        == bytes.fromhex("85C9740D8BC1"),
        "simulation-unit retained bodies do not match retail",
    )

def apply_type_definitions() -> None:
    # These internal tags were stripped from the imported source type set.
    # Forward tags are sufficient for the verified pointer-only prototypes and
    # deliberately do not replace any imported layout.
    ensure_forward_struct("IVP_Cluster_Manager")
    ensure_forward_struct("IVP_Object_Callback_Table")
    ensure_forward_struct("IVP_Object_Callback_Table_Hash")
    ensure_forward_struct("IVP_Collision_Callback_Table_Hash")
    ensure_forward_struct("IVV_Sphere_Cluster")
    ensure_forward_struct("IVV_Sphere")
    ensure_forward_struct("IVV_Cluster_Min_Hash")
    ensure_forward_struct("IVP_I_FPoint_VHash")
    replace_source_struct_definition(
        "IVP_Mindist_vtbl",
        0x20,
        "struct IVP_Mindist_vtbl { "
        "void (__thiscall *simulate_time_event)(IVP_Mindist *, IVP_Environment *); "
        "void (__thiscall *get_objects)(IVP_Collision *, IVP_Real_Object **); "
        "void (__thiscall *get_ledges)(IVP_Collision *, const IVP_Compact_Ledge **); "
        "void (__thiscall *delegator_is_going_to_be_deleted_event)(IVP_Collision *, IVP_Collision_Delegator *); "
        "void *(__thiscall *scalar_deleting_destructor)(IVP_Mindist *, unsigned int); "
        "void (__thiscall *mindist_rescue_push)(IVP_Mindist *); "
        "void (__thiscall *exact_mindist_went_invalid)(IVP_Mindist *, IVP_Mindist_Manager *); "
        "void (__thiscall *do_impact)(IVP_Mindist *); };",
        "Ballance IVP_Mindist primary vtable has exactly eight slots at "
        "0x100637B4. Unlike the neighboring source, it has no virtual "
        "is_recursive query; exact_mindist_went_invalid and do_impact are "
        "therefore slots six and seven rather than seven and eight.",
    )
    replace_source_struct_definition(
        "IVP_Mindist_Recursive",
        0x98,
        "struct IVP_Mindist_Recursive { IVP_Mindist base_mindist; "
        "IVP_Collision_Delegator base_collision_delegator; "
        "IVP_MINDIST_RECURSIVE_TYPES recursive_status; "
        "IVP_U_Vector_Base mindists; };",
        "Ballance allocates exactly 0x98 bytes for Recursive Mindist at RVA "
        "0x2F4B7. The secondary Collision Delegator vptr is +0x88, status "
        "is +0x8C and collision FVector is +0x90. The neighboring-source "
        "spawned_mindist_count at +0x98 is absent from the retail object.",
    )
    set_struct_member_named_udt_type(
        "IVP_Mindist_Recursive",
        0x98,
        "mindists",
        0x90,
        "IVP_U_FVector<IVP_Collision>",
        "Semantic collision FVector restored after the parser-safe layout "
        "rebuild; it remains the same eight-byte ABI at +0x90.",
    )
    replace_source_struct_definition(
        "IVP_Mindist_Recursive_vtbl",
        0x20,
        "struct IVP_Mindist_Recursive_vtbl { "
        "void (__thiscall *simulate_time_event)(IVP_Mindist_Recursive *, IVP_Environment *); "
        "void (__thiscall *get_objects)(IVP_Collision *, IVP_Real_Object **); "
        "void (__thiscall *get_ledges)(IVP_Collision *, const IVP_Compact_Ledge **); "
        "void (__thiscall *delegator_is_going_to_be_deleted_event)(IVP_Collision *, IVP_Collision_Delegator *); "
        "void *(__thiscall *scalar_deleting_destructor)(IVP_Mindist_Recursive *, unsigned int); "
        "void (__thiscall *mindist_rescue_push)(IVP_Mindist_Recursive *); "
        "void (__thiscall *exact_mindist_went_invalid)(IVP_Mindist_Recursive *, IVP_Mindist_Manager *); "
        "void (__thiscall *do_impact)(IVP_Mindist_Recursive *); };",
        "Exact eight-slot Ballance Recursive Mindist primary table at "
        "0x10063AD0. The imported nine-slot UDT incorrectly retained the "
        "neighboring-source virtual is_recursive method.",
    )
    replace_source_struct_definition(
        "IVP_Mindist_Recursive_Collision_Delegator_vtbl",
        0x08,
        "struct IVP_Mindist_Recursive_Collision_Delegator_vtbl { "
        "void (__thiscall *collision_is_going_to_be_deleted_event)(IVP_Collision_Delegator *, IVP_Collision *); "
        "void *(__thiscall *scalar_deleting_destructor_adjustor)(IVP_Collision_Delegator *, unsigned int); };",
        "Exact two-slot secondary Collision Delegator table at 0x10063AC8. "
        "Both entries receive ECX adjusted to complete object +0x88.",
    )
    replace_source_struct_definition(
        "IVP_Collision_Delegator_vtbl",
        0x08,
        "struct IVP_Collision_Delegator_vtbl { "
        "void (__thiscall *collision_is_going_to_be_deleted_event)(IVP_Collision_Delegator *, IVP_Collision *); "
        "void *(__thiscall *scalar_deleting_destructor)(IVP_Collision_Delegator *, unsigned int); };",
        "Ballance abstract Collision Delegator table has exactly two slots. "
        "Constructor/destructor vptr stores target 0x10063A34; the following "
        "0x10063A3C address is a separate five-slot Root Mindist table. The "
        "nearby change/get_spawned_mindist_count virtuals are absent here.",
    )
    replace_source_struct_definition(
        "IVP_Collision_Delegator_Root_vtbl",
        0x14,
        "struct IVP_Collision_Delegator_Root_vtbl { "
        "void (__thiscall *collision_is_going_to_be_deleted_event)(IVP_Collision_Delegator_Root *, IVP_Collision *); "
        "void *(__thiscall *scalar_deleting_destructor)(IVP_Collision_Delegator_Root *, unsigned int); "
        "void (__thiscall *object_is_removed_from_collision_detection)(IVP_Collision_Delegator_Root *, IVP_Real_Object *); "
        "IVP_Collision *(__thiscall *delegate_collisions_for_object)(IVP_Collision_Delegator_Root *, IVP_Real_Object *, IVP_Real_Object *); "
        "void (__thiscall *environment_is_going_to_be_deleted_event)(IVP_Collision_Delegator_Root *, IVP_Environment *); };",
        "Ballance Root Collision Delegator shape: the two-slot retail base "
        "followed by three Root callbacks. The concrete Root Mindist table at "
        "0x10063A3C proves the five-slot inherited ABI.",
    )
    replace_source_struct_definition(
        "IVP_Collision_Delegator_Root_Mindist_vtbl",
        0x14,
        "struct IVP_Collision_Delegator_Root_Mindist_vtbl { "
        "void (__thiscall *collision_is_going_to_be_deleted_event)(IVP_Collision_Delegator_Root_Mindist *, IVP_Collision *); "
        "void *(__thiscall *scalar_deleting_destructor)(IVP_Collision_Delegator_Root_Mindist *, unsigned int); "
        "void (__thiscall *object_is_removed_from_collision_detection)(IVP_Collision_Delegator_Root_Mindist *, IVP_Real_Object *); "
        "IVP_Collision *(__thiscall *delegate_collisions_for_object)(IVP_Collision_Delegator_Root_Mindist *, IVP_Real_Object *, IVP_Real_Object *); "
        "void (__thiscall *environment_is_going_to_be_deleted_event)(IVP_Collision_Delegator_Root_Mindist *, IVP_Environment *); };",
        "Exact five-slot Ballance Root Mindist table installed by constructor "
        "RVA 0x2F5A0 at address 0x10063A3C. The imported seven-slot UDT had "
        "incorrectly retained two virtuals from the neighboring source.",
    )
    ensure_source_struct_definition(
        "IVP_Complex_Simple",
        0x34,
        "struct IVP_Complex_Simple { int memory_column; int n_columns; "
        "int m_rows; int *inactives; int *inactives_copy; int *actives; "
        "int *actives_copy; double *matrix; double *matrix_copy; "
        "int *index_is_at; int *index_is_at_copy; double *help_column; "
        "double *help_row; };",
        "Source-only Ballance-era public tableau layout (0x34). The type "
        "was absent from the imported IDB and all seven algorithms are "
        "declaration-only in the neighboring tree; no retail body survives.",
    )
    replace_source_struct_definition(
        "IVP_Time",
        0x08,
        "struct IVP_Time { double seconds; };",
        "Ballance time value. Retained time-manager and event code use one "
        "binary64 double at +0x00; the imported long double spelling is a "
        "legacy source-parser rendering, not the public MSVC type.",
    )
    replace_source_struct_definition(
        "IVP_Event_Sim",
        0x18,
        "struct IVP_Event_Sim { double delta_time; double i_delta_time; "
        "IVP_Environment *environment; IVP_Simulation_Unit *sim_unit; };",
        "Ballance per-step event. Retained controller code reads binary64 "
        "delta/inverse-delta values at +0x00/+0x08 and pointers at "
        "+0x10/+0x14; imported long double labels were corrected.",
    )
    set_struct_comment(
        "IVP_Contact_Situation",
        0x58,
        "Ballance contact callback payload. Building Block callback RVA "
        "0x42B0 and Contact Point material fill RVA 0x24040 prove the "
        "normal/speed/world point at +0x00/+0x10/+0x20, object pointers at "
        "+0x40, compact edges at +0x48, materials at +0x50 and 0x58 end.",
    )
    set_struct_comment(
        "IVP_Event_Object",
        0x08,
        "Ballance object-lifecycle payload. Real Object destruction RVA "
        "0x9830 and Core freeze/revive RVAs 0xAF90/0xAEA0 write environment "
        "at +0x00 and real_object at +0x04 before exact dispatch.",
    )
    set_struct_comment(
        "IVP_Event_Collision",
        0x0C,
        "Ballance collision payload. Retained caller RVA 0x23CD0 writes the "
        "float elapsed time at +0x00, environment at +0x04 and contact "
        "situation at +0x08 before dispatch.",
    )
    set_struct_comment(
        "IVP_Event_Friction",
        0x0C,
        "Ballance friction lifecycle payload. Retained creation/deletion "
        "paths RVA 0x22180/0x1C230 write environment, contact situation and "
        "friction handle at +0x00/+0x04/+0x08.",
    )
    set_struct_comment(
        "IVP_Event_PSI",
        0x04,
        "Ballance PSI listener payload. Environment::fire_event_PSI RVA "
        "0x13C40 builds exactly one environment pointer on the stack.",
    )
    replace_source_struct_definition(
        "IVP_U_Point",
        0x20,
        "struct IVP_U_Point { double k[3]; double hesse_val; };",
        "Ballance double-precision point/Hesse storage. Retained vector and "
        "matrix bodies access four consecutive binary64 values through "
        "+0x18; imported long double labels were corrected.",
    )
    replace_source_struct_definition(
        "IVP_U_Quat",
        0x20,
        "struct IVP_U_Quat { double x; double y; double z; double w; };",
        "Ballance quaternion. Retained matrix conversion and normalization "
        "bodies access four consecutive binary64 components at "
        "+0x00/+0x08/+0x10/+0x18.",
    )
    replace_source_struct_definition(
        "IVP_Time_Manager",
        0x20,
        "struct IVP_Time_Manager { int n_events; "
        "IVP_Event_Manager *event_manager; IVP_U_Min_List *min_hash; "
        "IVP_Time_Event_PSI *psi_event; double last_time; "
        "IVP_Time base_time; };",
        "Ballance time manager. Retained event scheduling accesses the "
        "binary64 last_time at +0x10 and IVP_Time base_time at +0x18; "
        "the complete object remains 0x20.",
    )
    replace_source_struct_definition(
        "IVP_Range_Manager",
        0x60,
        "struct IVP_Range_Manager { IVP_Range_Manager_vtbl *__vftable; "
        "unsigned int alignment_padding_04; IVP_BOOL bound_to_environment; "
        "IVP_Environment *environment; double look_ahead_time_intra; "
        "double look_ahead_max_radius_intra; "
        "double look_ahead_min_distance_intra; "
        "double look_ahead_max_distance_intra; "
        "double look_ahead_min_seconds_intra; "
        "double look_ahead_time_world; "
        "double look_ahead_max_radius_world; "
        "double look_ahead_min_distance_world; "
        "double look_ahead_max_distance_world; "
        "double look_ahead_min_seconds_world; };",
        "Ballance range manager. Constructor RVA 0x2D8E0 proves the vptr, "
        "explicit +0x04 alignment gap, bound flag +0x08, environment +0x0C "
        "and ten consecutive binary64 look-ahead values through +0x58.",
    )
    replace_source_struct_definition(
        "IVP_Radar_Hit",
        0x10,
        "struct IVP_Radar_Hit { IVP_Object *this_object; "
        "IVP_Object *other_object; double dist; };",
        "Ballance radar result: two object pointers followed by the "
        "binary64 hit distance at +0x08.",
    )
    replace_source_struct_definition(
        "IVP_Radar",
        0x18,
        "struct IVP_Radar { IVP_Radar_vtbl *__vftable; "
        "unsigned int alignment_padding_04; double max_range; "
        "double max_relative_error; };",
        "Ballance radar interface state. The vptr is followed by the MSVC "
        "alignment gap and binary64 range/error values at +0x08/+0x10.",
    )
    replace_source_struct_definition(
        "IVP_U_Min_Hash_Elem",
        0x18,
        "struct IVP_U_Min_Hash_Elem { IVP_U_Min_Hash_Elem *next; "
        "unsigned int alignment_padding_04; double value; int cmp_index; "
        "void *elem; };",
        "Ballance sorted mindist hash element. Retail add RVA 0x371C0 "
        "allocates 0x18 and writes next +0x00, binary64 value +0x08, "
        "comparison index +0x10 and payload +0x14.",
    )
    replace_source_struct_definition(
        "p_Memory_Elem",
        0x08,
        "struct p_Memory_Elem { p_Memory_Elem *next; char data[4]; };",
        "Ballance transaction-arena block header. Retained allocation and "
        "rollback bodies read the previous-block link at +0x00 and align "
        "the data tail beginning at +0x04 to the next 32-byte address.",
    )
    replace_source_struct_definition(
        "IVP_U_Memory",
        0x14,
        "struct IVP_U_Memory { p_Memory_Elem *first_elem; "
        "p_Memory_Elem *last_elem; char *speicherbeginn; "
        "char *speicherende; short transaction_in_use; "
        "unsigned short size_of_external_mem; };",
        "Ballance simulation transaction arena. Retained constructor RVA "
        "0x20260 and init body RVA 0x20270 establish the four pointers and "
        "16-bit counters; allocation RVA 0xD110 and rollback RVA 0x201B0 "
        "confirm the 0x14-byte layout and 32-byte alignment contract.",
    )
    replace_source_struct_definition(
        "IVP_U_Matrix_Cache",
        0xAE8,
        "struct IVP_U_Matrix_Cache { IVP_Real_Object *object; "
        "IVP_Core *core; IVP_Time base_time; int base_time_code; "
        "IVP_U_Matrix *m_world_f_object[21]; "
        "IVP_U_Matrix matrizes[21]; };",
        "Ballance continuous-collision transform cache. Retained initializer "
        "RVA 0x2B300 proves object +0x00, Core +0x04, time code +0x10 and "
        "the 21-pointer table at +0x14. Retained solver RVA 0x37910 indexes "
        "the inline matrices at +0x68 with an exact 0x80 stride through "
        "index 20, proving the 0xAE8 boundary.",
    )
    replace_source_struct_definition(
        "IVP_Material_Simple",
        0x30,
        "struct IVP_Material_Simple { IVP_Material base; "
        "unsigned int alignment_padding_0C; double friction_value; "
        "double second_friction_x; double elasticity; double adhesion; };",
        "Ballance simple material. Retained constructor/getters confirm the "
        "0x0C material base, alignment gap and four binary64 material "
        "properties at +0x10/+0x18/+0x20/+0x28.",
    )
    replace_source_struct_definition(
        "IVP_SurfaceManager_Polygon_Solver",
        0x38,
        "struct IVP_SurfaceManager_Polygon_Solver { "
        "double visitor_bb_min_x; double visitor_bb_min_y; "
        "double visitor_bb_min_z; double visitor_bb_max_x; "
        "double visitor_bb_max_y; double visitor_bb_max_z; "
        "int traversion_depth; int max_traversion_depth; };",
        "Ballance polygon traversal workspace. Retained tree walks use six "
        "binary64 visitor bounds through +0x28 and depth counters at "
        "+0x30/+0x34.",
    )
    replace_source_struct_definition(
        "IVP_Mindist_Event_Solver",
        0x48,
        "struct IVP_Mindist_Event_Solver { "
        "double sum_max_surface_rot_speed; double projected_center_speed; "
        "double max_coll_speed; double worst_case_speed; "
        "IVP_Mindist *mindist; IVP_Environment *environment; "
        "IVP_Time t_now; IVP_Time t_max; IVP_COLL_TYPE event_type_out; "
        "unsigned int alignment_padding_3C; IVP_Time event_time_out; };",
        "Ballance mindist event workspace. Retained event prediction uses "
        "four binary64 speed bounds, two pointers, three IVP_Time values "
        "and the event enum; explicit +0x3C padding preserves the 0x48 ABI.",
    )
    replace_source_struct_definition(
        "IVP_Mindist_Minimize_Solver",
        0x838,
        "struct IVP_Mindist_Minimize_Solver { IVP_Mindist *mindist; "
        "int P_Finish_Counter; double termination_len; "
        "IVP_U_Point pos_opposite_BacksideOs; "
        "IVP_MM_Loop_Hash_Struct loop_hash[256]; int loop_hash_size; "
        "unsigned int alignment_padding_834; };",
        "Ballance mindist minimizer workspace. The binary64 termination "
        "length at +0x08, point +0x10, 256-entry loop hash +0x30 and count "
        "+0x830 are retained; explicit tail padding preserves size 0x838.",
    )
    replace_source_struct_definition(
        "IVP_BetterStatisticsmanager_Data_Double_Array",
        0x30,
        "struct IVP_BetterStatisticsmanager_Data_Double_Array { int size; "
        "double *array; double max_value; int xpos; int ypos; int width; "
        "int height; int bg_color; int border_color; int graph_color; "
        "unsigned int alignment_padding_2C; };",
        "Ballance imported statistics graph payload. The sample pointer and "
        "maximum are binary64 double types; explicit tail padding preserves "
        "the source-imported 0x30 layout.",
    )
    replace_source_struct_definition(
        "IVP_Statistic_Manager",
        0x60,
        "struct IVP_Statistic_Manager { "
        "IVP_Environment *l_environment; unsigned int alignment_padding_04; "
        "IVP_Time last_statistic_output; float max_rescue_speed; "
        "float max_speed_gain; int impact_sys_num; int impact_counter; "
        "int impact_sum_sys; int impact_hard_rescue_counter; "
        "int impact_rescue_after_counter; int impact_delayed_counter; "
        "int impact_coll_checks; int impact_unmov; double sum_energy_destr; "
        "int sum_of_mindists; int mindists_generated; int mindists_deleted; "
        "int range_intra_exceeded; int range_world_exceeded; "
        "int processed_fmindists; int global_fmd_counter; "
        "unsigned int alignment_padding_5C; };",
        "Ballance retail statistic manager. Constructor RVA 0x15E50 clears "
        "0x18 dwords, and IVP_Environment constructs this member at +0x38 "
        "followed by IVP_Freeze_Manager at +0x98. The imported 0x58 UDT "
        "incorrectly used four-byte double alignment; explicit padding "
        "records the actual x86 MSVC eight-byte class layout.",
    )
    replace_cyclic_struct_definition(
        "IVP_Environment",
        0x178,
        "struct IVP_Environment { "
        "IVP_Standard_Gravity_Controller *standard_gravity_controller; "
        "IVP_Time_Manager *time_manager; "
        "IVP_Sim_Units_Manager *sim_units_manager; "
        "IVP_Cluster_Manager *cluster_manager; "
        "IVP_Mindist_Manager *mindist_manager; "
        "IVP_OV_Tree_Manager *ov_tree_manager; "
        "IVP_Collision_Filter *collision_filter; "
        "IVP_Range_Manager *range_manager; "
        "IVP_Anomaly_Manager *anomaly_manager; "
        "IVP_Anomaly_Limits *anomaly_limits; "
        "IVP_PerformanceCounter *performancecounter; "
        "IVP_Universe_Manager *universe_manager; "
        "IVP_Real_Object *static_object; unsigned int reserved_34; "
        "IVP_Statistic_Manager statistic_manager; "
        "IVP_Freeze_Manager freeze_manager; "
        "IVP_BetterStatisticsmanager *better_statisticsmanager; "
        "IVP_Controller_Manager *controller_manager; "
        "IVP_Cache_Object_Manager *cache_object_manager; "
        "IVP_U_Active_Value_Manager *l_active_value_manager; "
        "IVP_Material_Manager *l_material_manager; "
        "IVP_U_Memory *short_term_mem; IVP_U_Memory *sim_unit_mem; "
        "double time_since_last_blocking; double delta_PSI_time; "
        "double inv_delta_PSI_time; IVP_U_Point gravity; "
        "float gravity_scalar; "
        "IVP_U_Vector_Base collision_listeners; "
        "IVP_U_Vector_Base psi_listeners; "
        "IVP_U_Vector_Base core_revive_list; "
        "char *auth_costumer_name; unsigned int auth_costumer_code; "
        "int pw_count; IVP_Environment_Manager *environment_manager; "
        "IVP_Time current_time; IVP_Time time_of_next_psi; "
        "IVP_Time time_of_last_psi; int current_time_code; "
        "int mindist_event_timestamp_reference; short next_movement_check; "
        "unsigned short reserved_142; IVP_ENV_STATE state; "
        "double integrated_energy_damp; "
        "IVP_U_Vector_Base global_object_listeners; "
        "IVP_U_Vector_Base collision_delegator_roots; "
        "IVP_Debug_Manager *debug_information; "
        "IVP_BOOL delete_debug_information; "
        "IVP_Draw_Vector_Debug *draw_vectors; void *client_data; "
        "int environment_magic_number; unsigned int reserved_174; };",
        "Ballance retail environment layout. The statistic manager starts "
        "at +0x38 and occupies 0x60 bytes; freeze state follows at +0x98. "
        "Unlike the imported neighboring layout, retail has no constraint "
        "listener vector at +0x10C: constructor/destructor code proves the "
        "customer fields at +0x10C/+0x110/+0x114 and the environment-manager "
        "backlink at +0x118. Retained time access starts at +0x120 and the "
        "complete object ends at 0x178. Binary64 fields use public C++ double.",
    )
    for member_name, member_offset, declaration in (
        ("collision_listeners", 0xF4, "IVP_U_Vector<IVP_Listener_Collision>"),
        ("psi_listeners", 0xFC, "IVP_U_Vector<IVP_Listener_PSI>"),
        ("core_revive_list", 0x104, "IVP_U_Vector<IVP_Core>"),
        (
            "global_object_listeners",
            0x150,
            "IVP_U_Vector<IVP_Listener_Object>",
        ),
        (
            "collision_delegator_roots",
            0x158,
            "IVP_U_Vector<IVP_Collision_Delegator_Root>",
        ),
    ):
        set_struct_member_named_udt_type(
            "IVP_Environment",
            0x178,
            member_name,
            member_offset,
            declaration,
            "Ballance public typed vector; the temporary base spelling is "
            "used only while atomically rebuilding the cyclic UDT.",
        )
    replace_source_struct_definition(
        "IVP_Constraint",
        0x18,
        "struct IVP_Constraint { IVP_Controller_Dependent base; "
        "unsigned int is_enabled; IVP_Vector_of_Cores_2 "
        "cores_of_constraint_system; };",
        "Ballance retail constraint base. RVA 0x375B0 initializes the vector "
        "at +0x08 and enabled bits at +0x04; the type ends at +0x18. The "
        "imported neighboring-revision client_data tail at +0x18 is absent.",
    )
    replace_source_struct_definition(
        "IVP_Constraint_Local",
        0x190,
        "struct IVP_Constraint_Local { IVP_Constraint base; "
        "float force_factor; float damp_factor_div_force; "
        "IVP_CONSTRAINT_AXIS_TYPE fixed[6]; float borderleft_Rfs[6]; "
        "float borderright_Rfs[6]; float limited_axis_stiffness; "
        "IVP_Constraint_Local_MaxImpulse *maxforce; "
        "IVP_Constraint_Local_Anchor m_Rfs_f_Rcs; "
        "IVP_Constraint_Local_Anchor m_Afs_f_Acs; "
        "IVP_U_Mapping mapping_uRfs_f_Rfs; "
        "IVP_U_Mapping mapping_uRrs_f_Rrs; "
        "unsigned char fixedtrans_dim; unsigned char fixedrot_dim; "
        "unsigned char limitedtrans_dim; unsigned char limitedrot_dim; "
        "unsigned char matrix_size; unsigned char alignment_padding_18B; "
        "unsigned char norm; unsigned char tail_padding[3]; };",
        "Ballance retail local constraint. The factory at RVA 0x129C0 "
        "allocates 0x190 bytes; RVA 0x28270 constructs 0x88-byte anchors at "
        "+0x70/+0xF8 and mappings at +0x180/+0x183. This replaces the "
        "imported 0x198 layout inherited from the later 0x1C base.",
    )
    replace_source_struct_definition(
        "IVP_Linear_Constraint_Solver",
        0x130,
        "struct IVP_Linear_Constraint_Solver { "
        "double SOLVER_EPS; double GAUSS_EPS; double TEST_EPS; "
        "double MAX_STEP_LEN; double *full_A; double *full_b; "
        "double *temp; double *full_x; double *delta_f; double *accel; "
        "double *delta_accel; double *reset_x; double *reset_accel; "
        "int *actives_inactives_ignored; int *variable_is_found_at; "
        "int n_variables; int aligned_size; int r_actives; "
        "int aligned_sub_size; int ignored_pos; int debug_lcs; "
        "int debug_no_lu_count; int first_permute_index; "
        "int second_permute_index; int first_permute_ignored; "
        "int second_permute_ignored; int sub_solver_status; "
        "unsigned int alignment_padding_7C; "
        "IVP_Incr_L_U_Matrix lu_sub_solver; "
        "IVP_Great_Matrix_Many_Zero sub_solver_mat; "
        "IVP_Great_Matrix_Many_Zero full_solver_mat; "
        "IVP_Great_Matrix_Many_Zero debug_mat; "
        "IVP_Great_Matrix_Many_Zero inv_mat; };",
        "Ballance retail linear-constraint solver. Retained solve code and "
        "the neighboring declaration agree on the 0x130 field sequence, "
        "including LU state at +0x80 and the last 0x20-byte matrix at "
        "+0x110. IVP_DOUBLE fields are recorded as binary64 double rather "
        "than the imported long double spelling.",
    )
    replace_source_struct_definition(
        "IVP_Incr_L_U_Matrix",
        0x30,
        "struct IVP_Incr_L_U_Matrix { double MATRIX_EPS; "
        "double *L_matrix; double *U_matrix; int *index_pos_contains; "
        "int *inv_index_pos_contains; double *input_vec; double *out_vec; "
        "double *temp_vec; double *mult_vec; int aligned_row_len; "
        "int n_sub; };",
        "Ballance incremental-LU state. Retained methods confirm L/U buffers "
        "at +0x08/+0x0C, aligned_row_len at +0x28 and n_sub at +0x2C. The "
        "public exchange_columns_l_u body was linked out but is compatibly "
        "reconstructed by calling the retained L and U exchange entries; "
        "delete_row_and_col_l_u adapts to retained decrement_l_u; "
        "debug_print_l_u is reconstructed from the complete nearby read-only "
        "body. debug_print_a preserves the nearby inverse(L)*U diagnostic "
        "while replacing its broken allocating constructor with local "
        "temporary buffers. No stripped convenience method is assigned a "
        "fake RVA.",
    )
    replace_source_struct_definition(
        "IVP_Great_Matrix_Many_Zero",
        0x20,
        "struct IVP_Great_Matrix_Many_Zero { double MATRIX_EPS; "
        "int columns; int aligned_row_len; double *matrix_values; "
        "double *desired_vector; double *result_vector; "
        "unsigned int alignment_padding_1C; };",
        "Ballance sparse matrix header. Retained constructor and solver code "
        "confirm binary64 epsilon/buffers and the 0x20 boundary. Explicit "
        "tail padding preserves the retail eight-byte class ABI in IDA. The "
        "linked-out get_number_null_lines helper is reconstructed without a "
        "fake RVA; the broken allocating constructor remains unavailable.",
    )
def apply_layout_corrections() -> None:
    normalize_imported_binary64_types()
    require_struct_size("IVP_Mindist_Settings", 0x138)
    remove_trailing_struct_member(
        "IVP_Controller_Phantom",
        0x48,
        "client_data",
        0x40,
        0x40,
    )
    set_struct_member_type(
        "IVP_Actuator_Rot_Mot",
        0xA0,
        "rot_inertia",
        0x90,
        "double",
        "Ballance IVP_DOUBLE is an MSVC 64-bit double. The legacy imported "
        "type rendered this member as long double even though the decorated "
        "API and x87 accesses use double semantics.",
    )
    set_struct_member_type(
        "IVP_Actuator_Torque",
        0x98,
        "rot_inertia",
        0x88,
        "double",
        "Ballance IVP_DOUBLE is an MSVC 64-bit double. The legacy imported "
        "type rendered this member as long double even though the decorated "
        "API and x87 accesses use double semantics.",
    )
    replace_source_struct_definition(
        "IVP_SurfaceBuilder_Ledge_Soup",
        0xA0,
        "struct IVP_SurfaceBuilder_Ledge_Soup { "
        "IVP_Compact_Surface *compact_surface; "
        "int number_of_terminal_spheres; int number_of_nodes; "
        "unsigned int alignment_padding_0C; double smallest_radius; "
        "int size_of_tree_in_bytes; IVV_Sphere_Cluster *spheres_cluster; "
        "IVP_U_Vector_Base c_ledge_vec; "
        "IVP_U_Vector_Base rec_spheres; "
        "IVP_U_Vector_Base terminal_spheres; "
        "IVP_U_Float_Point extents_min; IVP_U_Float_Point extents_max; "
        "int longest_axis; int number_of_unclustered_spheres; "
        "IVV_Cluster_Min_Hash *interval_minhash; "
        "IVP_U_Vector_Base overlapping_spheres; "
        "IVP_U_Vector_Base built_spheres; "
        "IVP_Template_Surbuild_LedgeSoup *parameters; "
        "IVP_Compact_Ledge *first_compact_ledge; "
        "IVP_Compact_Poly_Point *first_poly_point; "
        "int n_poly_points_allocated; IVP_I_FPoint_VHash *point_hash; "
        "IVP_Compact_Ledgetree_Node *ledgetree_work; "
        "char *clt_highmem; char *clt_lowmem; "
        "IVP_U_Vector_Base all_spheres; "
        "unsigned int alignment_padding_9C; };",
        "Ballance retained Ledge Soup builder. Constructor RVA 0x38290 "
        "clears the complete 0xA0 object and initializes vectors at "
        "+0x20/+0x28/+0x30/+0x64/+0x6C/+0x94; destructor RVA 0x38340 "
        "releases those same vectors. The field sequence matches retained "
        "accesses and the neighboring public declaration; smallest_radius "
        "is binary64 double, not the imported long double spelling. Explicit "
        "padding at +0x0C/+0x9C preserves the retail eight-byte class ABI in "
        "IDA, whose parsed double would otherwise collapse the UDT to 0x9C. "
        "IDA cannot reparse its imported angle-bracket template names, so "
        "the six IVP_U_Vector<T> subobjects are represented by their exact "
        "0x08 IVP_U_Vector_Base storage here; the public header retains the "
        "precise Compact_Ledge/IVV_Sphere element types.",
    )
    for member_name, member_offset, declaration in (
        ("trans_speed_potential", 0x30, "double"),
        ("trans_inertia", 0x38, "double[2]"),
        ("inv_trans_inertia", 0x48, "double[2]"),
        ("rot_speed_potential", 0x58, "double"),
        ("rot_inertia", 0x60, "double[2]"),
        ("inv_rot_inertia", 0x70, "double[2]"),
        ("rot_energy_potential", 0x80, "double"),
        ("trans_energy_potential", 0x88, "double"),
        ("whole_mutual_energy", 0x98, "double"),
    ):
        set_struct_member_type(
            "IVP_Mutual_Energizer",
            0xA0,
            member_name,
            member_offset,
            declaration,
            "Ballance IVP_DOUBLE is the MSVC 64-bit double ABI. The legacy "
            "import rendered this field as long double; retained x87 code "
            "and public decorated signatures confirm double semantics.",
        )
    set_struct_comment(
        "IVP_Mutual_Energizer",
        0xA0,
        "Ballance two-core friction-energy record. Size and offsets are "
        "confirmed by retained initialization/calculation/destruction bodies "
        "and the 0xA0 stack reservation in IVP_Friction_Core_Pair at RVA "
        "0x1D2A0; imported IVP_DOUBLE labels were corrected to double.",
    )
    for member_name, member_offset in (
        ("simulation_time", 0x18),
        ("update_interval", 0x28),
    ):
        set_struct_member_type(
            "IVP_BetterStatisticsmanager",
            0x30,
            member_name,
            member_offset,
            "double",
            "Ballance IVP_DOUBLE is the MSVC 64-bit double ABI. Retained "
            "constructor and update code use this field as binary64; the "
            "legacy import's long double spelling is misleading.",
        )
    replace_source_struct_definition(
        "IVP_Contact_Point",
        0x78,
        "struct IVP_Contact_Point { "
        "IVP_Contact_Point *next_dist_in_friction; "
        "IVP_Contact_Point *prev_dist_in_friction; "
        "IVP_Synapse_Friction synapse[2]; "
        "float inv_virt_mass_mindist_no_dir; "
        "unsigned char two_friction_values; unsigned char flags_padding[3]; "
        "float span_friction_s[2]; "
        "IVP_Impact_Solver_Long_Term *tmp_contact_info; "
        "float real_friction_factor; float integrated_destroyed_energy; "
        "float inv_triangle_det; float old_energy_dynamic_fr; "
        "float now_friction_pressure; float last_gap_len; "
        "short slowly_turn_on_keeper; unsigned short keeper_padding; "
        "int cp_status; int has_negative_pull_since; "
        "double last_time_of_recalc_friction_s_vals; "
        "IVP_Friction_System *l_friction_system; unsigned int tail_padding; "
        "};",
        "Ballance retail contact point, rebuilt from allocation size and "
        "retained field accesses rather than the incompatible imported "
        "0x88 hierarchy. The release object is 0x78 and ends with its "
        "friction-system backlink at +0x70; it has no neighboring-revision "
        "last_contact_point_ws field. The private wheel-friction helper is "
        "inlined into the retained 2D constraint body at RVA 0x1B080.",
    )
    for helper_name in (
        "IVP_Friction_Sys_Energy",
        "IVP_Friction_Sys_Static",
    ):
        set_struct_comment(
            helper_name,
            0x08,
            "Ballance embedded friction-controller subobject: controller "
            "vptr at +0x00 and IVP_Friction_System backlink at +0x04. The "
            "owner constructor installs the retail table directly.",
        )
    set_struct_comment(
        "IVP_Friction_System",
        0x50,
        "Ballance dynamic-friction controller and contact-topology owner. "
        "Retained bodies confirm vectors at +0x24/+0x2C/+0x34, counters at "
        "+0x3C/+0x3E/+0x40, flags at +0x44/+0x45 and the final double at "
        "+0x48. Nearby core_is_found_in_pairs is a short source-only scan "
        "over pair->objs[0/1]; obsolete get_controlled_cores is a no-op and "
        "is distinct from virtual get_associated_controlled_cores. The "
        "distance-keeper and diagnostic-only methods have no retail entry.",
    )
    rename_struct_member(
        "IVP_Friction_Sys_Static_vtbl",
        0x1C,
        "get_minimum_simulation_frequency",
        0x00,
        "core_is_going_to_be_deleted_event",
        "Retail slot 0 removes contacts involving the deleted core; the "
        "imported minimum-frequency label was wrong.",
    )
    rename_struct_member(
        "IVP_Template_Real_Object",
        0x70,
        "pinned",
        0x10,
        "enable_piling_optimization",
        "RVA 0x9787 passes template +0x10 as IVP_Core's final "
        "enable-piling argument. Ballance omits the following pinned "
        "field from the nearby revision, preserving material at +0x14.",
    )
    rename_struct_member(
        "IVP_Friction_Sys_Static_vtbl",
        0x1C,
        "get_minimum_simulation_frequency_2",
        0x04,
        "get_minimum_simulation_frequency",
        "Retail slot 1 is the inherited Controller minimum-frequency query.",
    )
    for table_name, owner_name in (
        ("IVP_Friction_Sys_Energy_vtbl", "IVP_Friction_Sys_Energy"),
        ("IVP_Friction_Sys_Static_vtbl", "IVP_Friction_Sys_Static"),
    ):
        set_struct_member_type(
            table_name,
            0x1C,
            "get_minimum_simulation_frequency",
            0x04,
            f"double (__thiscall *)({owner_name} *)",
            "Ballance IVP_DOUBLE is an MSVC 64-bit double; the imported long "
            "double rendering was wrong.",
        )
        set_struct_member_type(
            table_name,
            0x1C,
            "do_simulation_controller",
            0x10,
            f"void (__thiscall *)({owner_name} *, IVP_Event_Sim *, IVP_U_Vector *)",
            "Retail controller slot 4 with the concrete friction-helper owner.",
        )
        set_struct_member_type(
            table_name,
            0x1C,
            "get_controller_priority",
            0x14,
            f"IVP_CONTROLLER_PRIORITY (__thiscall *)({owner_name} *)",
            "Retail controller slot 5 with the concrete friction-helper owner.",
        )
        rename_struct_member(
            table_name,
            0x1C,
            "dtr_IVP_Controller",
            0x18,
            "scalar_deleting_destructor",
            "Retail slot 6 is the folded scalar-deleting destructor, not a "
            "parameterless complete destructor.",
        )
        set_struct_member_type(
            table_name,
            0x1C,
            "scalar_deleting_destructor",
            0x18,
            f"void *(__thiscall *)({owner_name} *, unsigned int)",
            "MSVC scalar-deleting destructor receives flags and returns the "
            "object pointer.",
        )
        set_struct_comment(
            table_name,
            0x1C,
            "Ballance seven-slot IVP_Controller ABI for an embedded friction "
            "helper: core deletion, frequency, associated cores, reset time, "
            "simulation, priority and scalar-deleting destructor.",
        )
    set_struct_member_type(
        "IVP_Friction_Sys_Static_vtbl",
        0x1C,
        "core_is_going_to_be_deleted_event",
        0x00,
        "void (__thiscall *)(IVP_Friction_Sys_Static *, IVP_Core *)",
        "Retail slot 0 receives the core whose contacts must be removed.",
    )
    rename_struct_member(
        "IVP_Great_Matrix_Many_Zero",
        0x20,
        "MATRIX_EPS",
        0x00,
        "MATRIX_EPS",
        "Eight-byte IVP_DOUBLE epsilon. IDA's legacy parser renders this "
        "as binary64 double; explicit tail padding preserves the retail "
        "0x20 class boundary.",
    )
    for member_name, member_offset in (
        ("matrix_values", 0x10),
        ("desired_vector", 0x14),
        ("result_vector", 0x18),
    ):
        rename_struct_member(
            "IVP_Great_Matrix_Many_Zero",
            0x20,
            member_name,
            member_offset,
            member_name,
            "Pointer targets are binary64 IVP_DOUBLE values. The rebuilt "
            "UDT records them as double pointers while explicit tail padding "
            "preserves the retail 0x20 class boundary.",
        )
    set_struct_comment(
        "IVP_Great_Matrix_Many_Zero",
        0x20,
        "Ballance sparse matrix header. The retained constructor and solver "
        "code confirm a double epsilon, three double buffers and size 0x20. "
        "The linked-out public get_number_null_lines rank helper is "
        "reconstructed by scanning complete logical rows against MATRIX_EPS; "
        "aligned padding is excluded and no fake RVA is assigned.",
    )
    rename_struct_member(
        "IVP_Friction_Solver",
        0x840,
        "correct_x_factor",
        0x20,
        "correct_x_factor",
        "Eight-byte IVP_DOUBLE. IDA's legacy parser renders the imported "
        "alignment-preserving scalar as long double; replacing it with its "
        "parser double would incorrectly shrink this UDT to 0x83C.",
    )
    set_struct_comment(
        "IVP_Vector_of_Contact_Info_512",
        0x808,
        "Ballance fixed-capacity contact-info pointer vector. Its base at "
        "+0x00 points to 512 inline pointer slots beginning at +0x08.",
    )
    set_struct_comment(
        "IVP_Friction_Solver",
        0x840,
        "Ballance temporary friction solver. do_friction_system reserves "
        "0x840 stack bytes; retained constructor accesses matrix +0x00, "
        "double +0x20, environment/event +0x28/+0x2C, inline vector +0x30 "
        "and diagnostic counter +0x838. calc_solver_PSI contains two verified "
        "inline expansions of calc_distance_matrix_column at 0x100368BB and "
        "0x100369BE.",
    )
def annotate_types() -> None:
    for type_name, type_size, type_comment in (
        (
            "IVP_Anchor",
            0x30,
            "Retail-confirmed anchor layout. RVA 0x13EA0 accesses object at "
            "+0x08, object/core positions at +0x0c/+0x1c and actuator at "
            "+0x2c; RVA 0x14020 constructs two anchors with 0x30 stride.",
        ),
        (
            "IVP_Template_Stiff_Spring",
            0x20,
            "Layout verified against the Ballance imported type and the "
            "nearby public header. No Stiff_Spring function body survives "
            "linking in physics_RT.dll.",
        ),
        (
            "IVP_Controller_Stiff_Spring",
            0x90,
            "Layout verified field-by-field for Ballance. This is a distinct "
            "public controller, not an alias for IVP_Actuator_Spring; its "
            "function bodies were link-stripped from physics_RT.dll.",
        ),
        (
            "IVP_Controller_Stiff_Spring_Active",
            0xA0,
            "Verified Ballance multiple-inheritance layout: Stiff_Spring at "
            "+0x00, IVP_U_Active_Float_Listener at +0x90, active inputs at "
            "+0x94/+0x98/+0x9C. Bodies are link-stripped.",
        ),
        (
            "IVP_Template_Check_Dist",
            0x58,
            "Layout verified field-by-field for Ballance. The linked image "
            "does not retain Check_Dist implementation bodies.",
        ),
        (
            "IVP_Anchor_Check_Dist",
            0x20,
            "Verified Ballance IVP_Listener_Hull-derived anchor layout. "
            "The nearby callback argument name says shorter-than-range, but "
            "the implementation passes the new is_outside state.",
        ),
        (
            "IVP_Actuator_Check_Dist",
            0x58,
            "Verified Ballance layout. The nearby implementation stores and "
            "publishes is_outside and reinserts both anchors in their hull "
            "min-lists after each range evaluation.",
        ),
        (
            "IVP_Template_Four_Point",
            0x14,
            "Ballance imported layout agrees with the nearby public "
            "four-anchor template. Its constructor body is link-stripped.",
        ),
        (
            "IVP_Actuator_Four_Point",
            0xD0,
            "Four-anchor actuator layout verified against the imported UDT "
            "and the x86 IVP_Actuator/IVP_Anchor prefixes. No function body "
            "or vtable survives linking. The nearby constructor has an "
            "inverted movable-core test and omits controller registration.",
        ),
        (
            "IVP_Template_Stabilizer",
            0x1C,
            "Ballance imported layout agrees with the nearby public "
            "Stabilizer template: Four_Point plus constant and active input.",
        ),
        (
            "IVP_Actuator_Stabilizer",
            0xD8,
            "Ballance imported layout: Four_Point +0x00, environment +0xD0, "
            "stabilizer constant +0xD4. All Stabilizer bodies are "
            "link-stripped from physics_RT.dll.",
        ),
        (
            "IVP_Template_Suspension",
            0x40,
            "Ballance imported layout: the 0x38-byte retail Spring template "
            "followed by compression damping at +0x38 and maximum body "
            "force at +0x3C. The constructor body is link-stripped.",
        ),
        (
            "IVP_Actuator_Suspension",
            0xA0,
            "Ballance imported layout: the 0x98-byte retail Spring actuator "
            "followed by mass-adapted compression damping at +0x98 and "
            "maximum body force at +0x9C. All Suspension bodies are "
            "link-stripped from physics_RT.dll.",
        ),
        (
            "IVP_Template_Controller_Golem",
            0x38,
            "Ballance imported layout agrees with the nearby public type: "
            "the 0x24-byte Motion template followed by five float policy "
            "values at +0x24..+0x34. Its constructor body is link-stripped.",
        ),
        (
            "IVP_Template_Controller_Floating",
            0x50,
            "Ballance imported layout agrees with the public source type: "
            "two float force limits, two 0x20 points and two float distances. "
            "All class-specific bodies are link-stripped.",
        ),
        (
            "IVP_Controller_Floating",
            0x40,
            "Ballance imported layout agrees field-for-field with the public "
            "source type: Independent base +0x00, object +0x04, force limits "
            "+0x08/+0x0C, float points +0x10/+0x20 and double distances "
            "+0x30/+0x38. No class-specific body or vtable survives linking.",
        ),
        (
            "IVP_Template_Controller_World_Friction",
            0x80,
            "Ballance imported public template layout contains four consecutive "
            "0x20 IVP_U_Point values: desired linear/angular speed followed by "
            "translation/rotation correction rates. Its bodies are stripped.",
        ),
        (
            "IVP_Controller_World_Friction",
            0x4C,
            "Ballance imported public runtime layout contains the Independent "
            "base, object pointer, four 0x10 float points and clip_manhattan at "
            "+0x48. No class-specific body or vtable survives linking.",
        ),
        (
            "IVP_Range_Manager",
            0x60,
            "Retail constructor 0x1002D8E0 proves alignment padding at +0x04, "
            "bound_to_environment at +0x08, environment at +0x0C and ten "
            "64-bit IVP_DOUBLE values at +0x10..+0x58. MSVC's eight-byte "
            "class alignment reproduces this layout from the nearby fields.",
        ),
        (
            "IVP_BetterDebugmanager",
            0x2008,
            "Retail constructor 0x10060C30 proves vptr +0x00, initialized "
            "+0x04 and 2048 32-bit channel flags at +0x08. Vtable "
            "0x10063D34 contains output_function and the deleting destructor.",
        ),
        (
            "IVP_SurMan_PS_Plane",
            0x28,
            "Retail edge-length body 0x1003A2D0 reads U_Vector memsize/n_elems/"
            "elems at +0x20/+0x22/+0x24 after the 0x20 IVP_U_Point base, "
            "proving the complete 0x28 layout.",
        ),
        (
            "IVP_Vector_of_Points_256",
            0x408,
            "Nearby public inline declaration places a 0x08 IVP_U_Vector base "
            "before 256 embedded x86 pointers. This is source-layout evidence; "
            "no class-specific Ballance body survives linking.",
        ),
        (
            "IVP_BetterStatisticsmanager",
            0x30,
            "Retail constructor 0x1002DB50 confirms enabled +0x00, vectors "
            "+0x04/+0x0C, update_delayed +0x20 and IVP_DOUBLE interval "
            "+0x28. Natural x86 double alignment leaves padding at +0x14 "
            "and +0x24; simulation_time occupies +0x18.",
        ),
        (
            "IVP_BetterStatisticsmanager_Data_Entity",
            0x48,
            "Imported public layout agrees with the neighboring source: "
            "enabled/type at +0x00/+0x04, 0x30-byte aligned value union at "
            "+0x08, text +0x38 and presentation fields through +0x44. No "
            "Data Entity function body survives linking.",
        ),
        (
            "IVP_BetterStatisticsmanager_Data_Int_Array",
            0x28,
            "Imported public layout contains ten consecutive x86 int/pointer "
            "fields. No class-specific retail body survives linking.",
        ),
        (
            "IVP_BetterStatisticsmanager_Data_Double_Array",
            0x30,
            "Imported public layout uses size/pointer at +0x00/+0x04, one "
            "8-byte IVP_DOUBLE at +0x08 and seven ints through +0x28. No "
            "class-specific retail body survives linking.",
        ),
        (
            "IVP_BetterStatisticsmanager_Callback_Interface",
            0x04,
            "Imported public interface is one x86 vptr with output_request, "
            "enable and disable pure slots and deliberately no destructor slot.",
        ),
        (
            "IVP_Controller_Golem",
            0x130,
            "Ballance imported layout agrees field-for-field with the nearby "
            "public type: the 0x88-byte Motion base, dynamic target state at "
            "+0x88..+0x117, and five float policy values at +0x118..+0x128. "
            "Every Golem-specific function body is link-stripped, so this is "
            "IDB/source layout evidence rather than retail-code confirmation.",
        ),
        (
            "IVP_Template_Constraint_Fixed_Keyframed",
            0x24,
            "Ballance imported layout is exactly the public Motion template "
            "base with no additional fields. Its default constructor is "
            "source-inline and has no retained retail body.",
        ),
        (
            "IVP_Constraint_Fixed_Keyframed",
            0xE0,
            "Ballance imported layout agrees field-for-field with the nearby "
            "public type, including the two aligned quaternion targets and "
            "the controlled-core vector at +0xD0. No class-specific body or "
            "vtable survives linking, so this remains IDB/source evidence.",
        ),
        (
            "IVP_Forcefield",
            0x10,
            "Ballance imported layout and nearby public declaration agree: "
            "listener base +0x00, independent-controller base +0x04, active "
            "core-set pointer +0x08 and owner flag +0x0C. No Forcefield-"
            "specific body or vtable survives, so this remains IDB/source "
            "evidence rather than retail-code confirmation.",
        ),
        (
            "IVP_Template_Car_System",
            0x304,
            "Ballance imported layout agrees with the nearby car-system "
            "configuration through both ten-wheel arrays and five-axis "
            "tails. Its zero/default constructor is source-inline and no "
            "class-specific retail body survives.",
        ),
        (
            "IVP_Wheel_Skid_Info",
            0x20,
            "Ballance imported layout: last contact float point +0x00, skid "
            "value +0x10 and aligned IVP_Time +0x18. This is IDB/source "
            "layout evidence rather than retained-code confirmation.",
        ),
        (
            "IVP_CarSystemDebugData_t",
            0x308,
            "Ballance imported debug block ends at 0x308 after raycasts, "
            "impacts and two 4x3 torque arrays. Four actuator vectors "
            "appended by the nearby 0x348 revision are not part of this ABI.",
        ),
        (
            "IVP_Car_System",
            0x04,
            "Ballance imported interface has one vptr and 28 virtual slots. "
            "Unlike the nearby revision it has one-argument do_steering and "
            "no set_powerslide, get_booster_time_to_go or "
            "event_object_deleted slots. No car-system vtable survives in "
            "physics_RT.dll, so this remains IDB/source version evidence.",
        ),
        (
            "IVP_Car_System_Real_Wheels",
            0x450,
            "Ballance imported concrete real-wheel car: the 28-slot Car "
            "System base is followed by environment/counts, body and ten "
            "wheel pointers, compatible constraint/actuator arrays, two "
            "persistent forces, wheel-lock constraints, tuning/timer state "
            "and the shortened debug block at +0x148. The nearby revision's "
            "two powerslide force pointers are absent, fixing total size at "
            "0x450. No class-specific body or vtable address survives, so "
            "behavior is selectively reconstructed from matching source.",
        ),
        (
            "IVP_Car_System_Real_Wheels_vtbl",
            0x74,
            "Imported Ballance type evidence describes the 28-slot Car "
            "System interface followed by the protected environment deletion "
            "callback. The original slot-0 GetCarSystemDebugData label was "
            "self-contradictory: the real debug getter already occupies slot "
            "27, so slot 0 is the virtual destructor. This is imported type "
            "evidence, not a retained retail vtable address.",
        ),
        (
            "IVP_Raycast_Car_Axis",
            0x04,
            "Ballance imported raycast-car axis contains only the stabilizer "
            "constant. No class-specific retail body survives.",
        ),
        (
            "IVP_Raycast_Car_Wheel",
            0x7C,
            "Ballance imported raycast-wheel layout agrees field-for-field "
            "with the nearby public declaration, including axis direction "
            "+0x4C and output surface speed +0x64. No class-specific retail "
            "body survives.",
        ),
        (
            "IVP_Raycast_Car_Wheel_Temp",
            0x98,
            "Ballance imported aligned raycast temporary layout agrees with "
            "the nearby public declaration; the IVP_U_Point begins at +0x20. "
            "No class-specific retail body survives.",
        ),
        (
            "IVP_Controller_Raycast_Car_Vector_of_Cores_1",
            0x0C,
            "Ballance imported layout and nearby source-inline declaration "
            "agree on an 0x08 IVP_U_Vector base plus one embedded core-pointer "
            "cell at +0x08. The constructor is link-stripped.",
        ),
        (
            "IVP_Controller_Raycast_Car",
            0x958,
            "Ballance imported layout: IVP_Car_System primary base +0x00, "
            "IVP_Controller_Dependent secondary base +0x04, inline core "
            "vector +0x08, twelve 0x7C wheel records +0x1C, six axis "
            "records +0x5EC, vehicle/control state +0x604 and the shortened "
            "0x308 debug block +0x650. No class-specific body or concrete "
            "vtable survives in physics_RT.dll; the reconstructed behavior "
            "therefore remains nearby-source based.",
        ),
        (
            "IVP_Controller_Raycast_Car_vtbl",
            0x74,
            "Imported Ballance type evidence describes 29 primary slots: "
            "the 28-slot Ballance IVP_Car_System interface followed only by "
            "do_raycasts. The nearby revision's virtual set_powerslide and "
            "get_booster_time_to_go slots are absent, and its two-argument "
            "do_steering differs from Ballance's one-float slot. This is "
            "imported type evidence, not a retained retail vtable address.",
        ),
        (
            "IVP_U_Point_4",
            0x10,
            "Four-float inverse-inertia vector used by the core-reaction "
            "solver; retained solver code accesses xyz and inverse mass at "
            "+0x0C.",
        ),
        (
            "IVP_Solver_Core_Reaction",
            0x148,
            "Retail translation setup, private transform setup and dim2 "
            "impulse code confirm the imported field offsets through the "
            "reaction matrix at +0xD8 and delta velocity at +0x138.",
        ),
        (
            "IVP_Object",
            0x1C,
            "Retail-confirmed Ballance base layout with a one-slot deleting-"
            "destructor vtable at VA 0x100633A0.",
        ),
        (
            "IVP_Cluster",
            0x20,
            "Retail root construction and complete destruction confirm the "
            "IVP_Object prefix, child-list head at +0x1C and one-slot table "
            "at VA 0x100633A4.",
        ),
        (
            "IVP_Hull_Manager_Base",
            0x38,
            "Retail complete construction and destruction own the embedded "
            "0x14-byte IVP_U_Min_List at +0x20. Calling those bodies from a "
            "wrapper with an automatically-lived member would double-own it.",
        ),
        (
            "IVP_U_Min_List",
            0x14,
            "Retail constructor RVA 0x300F0 stores a 16-bit capacity at "
            "+0x00, buffer pointer at +0x04 and list sentinels through +0x10; "
            "RVA 0x30170 frees that buffer.",
        ),
        (
            "IVP_Real_Object_Fast_Static",
            0x40,
            "Retail-confirmed IVP_Object-derived prefix through the aligned "
            "core/object shift at +0x30.",
        ),
        (
            "IVP_Real_Object_Fast",
            0x88,
            "Retail-confirmed fast-object prefix: cache +0x40, hull manager "
            "+0x48, flags +0x80.",
        ),
        (
            "IVP_Real_Object",
            0xB8,
            "Retail-confirmed four-level object layout. The three-slot table "
            "is destructor, quaternion update, matrix update.",
        ),
        (
            "IVP_Controller_Phantom",
            0x40,
            "Retail convert_to_phantom allocates exactly 0x40 bytes. The "
            "constructor RVA 0x10D00 and complete destructor RVA 0x108A0 "
            "confirm the listener vector at +0x08, embedded active mindist "
            "set at +0x10 and optional sets/counters at +0x28..+0x34. The "
            "imported client_data tail at +0x40 belongs to a later revision "
            "and has been removed.",
        ),
        (
            "IVP_Attacher_To_Cores_Buoyancy",
            0x70,
            "Retail constructor and six-slot vtable confirm the 0x1C public "
            "template-base prefix and full 0x70 derived layout.",
        ),
        (
            "IVP_Anomaly_Limits",
            0x14,
            "Retail constructor writes through +0x10. Its two-slot table is "
            "environment callback then deleting destructor.",
        ),
        (
            "IVP_Anomaly_Manager",
            0x08,
            "Retail constructor and seven-slot table confirm one vptr, one "
            "delete flag, six callbacks, then deleting destructor.",
        ),
        (
            "IVP_Application_Environment",
            0x30,
            "Retail constructor clears exactly twelve x86 dwords.",
        ),
        (
            "IVP_Material",
            0x0C,
            "Retail base vtable and derived constructor confirm the 0x0C "
            "prefix and Ballance adhesion slot.",
        ),
        (
            "IVP_Material_Simple",
            0x30,
            "Retail constructor writes the full four-double material payload "
            "and installs the verified six-slot table.",
        ),
        (
            "IVP_Liquid_Surface_Descriptor_Simple",
            0x24,
            "Retail constructor RVA 0x10820 installs vtable 0x10063558, "
            "copies the four-float Hesse plane to +0x04 and the three-float "
            "absolute current vector to +0x14; RVA 0x107D0 reads the same "
            "fields back.",
        ),
        (
            "IVP_Material_Manager",
            0x08,
            "Retail constructor writes the only data field; verified table "
            "includes adhesion at slot 3 and destructor at slot 4.",
        ),
        (
            "IVP_U_Active_Value_Manager",
            0x28,
            "Retail constructor initializes the published manager state and "
            "installs the verified fifteen-slot table.",
        ),
        (
            "IVP_U_Float_Hesse",
            0x10,
            "Retail plane calculation and normalization access four 32-bit "
            "float lanes; IVP_FLOAT is float.",
        ),
        (
            "IVP_U_Quat",
            0x20,
            "Retail matrix conversion and quaternion normalization bodies "
            "access four consecutive IVP_DOUBLE components at +0x00, "
            "+0x08, +0x10 and +0x18.",
        ),
        (
            "IVP_U_Hesse",
            0x20,
            "Retail calculation, projection and normalization bodies use "
            "the 0x18-byte IVP_U_Point prefix plus the IVP_DOUBLE plane "
            "coefficient at +0x18.",
        ),
        (
            "IVP_VHash_Store",
            0x14,
            "Retail constructor RVA 0x1DE80 writes size +0x00, mask +0x04, "
            "element count +0x08, store pointer +0x0C and external-storage "
            "sentinel +0x10; rehash and destruction corroborate ownership.",
        ),
        (
            "IVP_Cache_Object",
            0xD0,
            "Retail manager construction allocates 0xD0 bytes per cache "
            "entry. Retained transforms access the matrix at +0x30, its "
            "translation at +0x90 and stay within the confirmed boundary.",
        ),
        (
            "IVP_Cache_Object_Manager",
            0x0C,
            "Retail construction and destruction confirm cache count +0x00, "
            "reuse-loop index +0x04 and cache buffer +0x08.",
        ),
        (
            "IVP_U_Min_Hash",
            0x14,
            "Retail construction and destruction confirm size +0x00, tree "
            "+0x04, per-bucket minima +0x08, bucket heads +0x0C and counter "
            "+0x10. Add allocates 0x18-byte elements with sorted index +0x10 "
            "and payload +0x14.",
        ),
        (
            "IVP_U_Min_Hash_Elem",
            0x18,
            "Retail add RVA 0x371C0 allocates exactly 0x18 bytes and writes "
            "next +0x00, IVP_DOUBLE value +0x08, cmp_index +0x10 and payload "
            "+0x14; Ballance therefore enables SORT_MINDIST_ELEMENTS.",
        ),
        (
            "IVP_U_String_Hash",
            0x0C,
            "Retail construction, lookup and insertion corroborate the three "
            "published x86 fields.",
        ),
        (
            "IVP_Template_Two_Point",
            0x0C,
            "Retail constructor RVA 0x13E70 clears client_data at +0x00 and "
            "both anchor pointers at +0x04/+0x08, confirming the complete "
            "0x0C Ballance layout.",
        ),
        (
            "IVP_Template_Spring",
            0x38,
            "Retail constructor RVA 0x14200 clears the complete 0x38 object "
            "and writes float 1.0e20 at break_max_len +0x24. Ballance omits "
            "the nearby spring_force_only_on_stretch member.",
        ),
        (
            "IVP_Template_Object",
            0x04,
            "Retail constructor RVA 0x15E90 clears the sole name pointer at "
            "+0x00; set/destruct RVAs 0x15EC0/0x15EA0 replace, free and clear "
            "the same retail-owned string field.",
        ),
        (
            "IVP_Template_Point",
            0x20,
            "Ballance's retained pointsoup/polygon builders use the exact "
            "0x20 IVP_U_Point representation without an added derived field.",
        ),
        (
            "IVP_Template_Line",
            0x04,
            "Ballance's retained polygon transport stores exactly two "
            "unsigned 16-bit shared-point indices.",
        ),
        (
            "IVP_Template_Polygon",
            0x18,
            "Retail constructor RVA 0x3BF80 clears six count/pointer dwords; "
            "the destructor and convex builders use surfaces at +0x14.",
        ),
        (
            "IVP_Template_Surface",
            0x30,
            "Retail constructor RVA 0x3C000 clears twelve dwords. Close and "
            "index bodies prove templ_poly +0x20, line count +0x24, line "
            "indices +0x28 and reversal flags +0x2C.",
        ),
        (
            "IVP_Template_Anchor",
            0x28,
            "Retail setter writes the object pointer at +0x00 and aligned "
            "0x20-byte world point at +0x08.",
        ),
        (
            "IVP_Template_Phantom",
            0x14,
            "Retail constructor initializes through +0x10 and confirms the "
            "0x14 Ballance boundary.",
        ),
        (
            "IVP_Template_Real_Object",
            0x70,
            "RVA 0x9787 proves +0x10 is enable_piling_optimization: it is "
            "passed as IVP_Core's final constructor argument. Ballance omits "
            "the nearby following pinned field, yielding the 0x70 layout. "
            "Constructor RVA 0x15EF0 stores the double damping defaults by "
            "widening the source float constant 0.01f.",
        ),
        (
            "IVP_Object_Attach",
            0x01,
            "Imported empty public helper type. Its three static core and "
            "collision-state operations are link-stripped from Ballance.",
        ),
        (
            "IVP_Constraint_Car_Object",
            0xB0,
            "Imported real-wheel object layout agrees field-by-field with "
            "the neighboring public header. No class-specific retail body "
            "survives linking.",
        ),
        (
            "IVP_Constraint_Solver_Car",
            0xA0,
            "Imported early real-wheel solver layout agrees with the "
            "neighboring header. The concrete vtable and methods are "
            "link-stripped; this is IDB/source evidence, not a retail body.",
        ),
        (
            "IVP_Constraint_Solver_Car_Builder",
            0x30,
            "Imported effective-mass builder layout agrees with the "
            "neighboring header. No class-specific retail body survives.",
        ),
        (
            "IVP_Constraint_Solver_Car_vtbl",
            0x1C,
            "Seven-slot imported controller table type. No concrete table "
            "survives; slot order follows the verified IVP_Controller base "
            "ABI and the neighboring override set.",
        ),
    ):
        set_struct_comment(type_name, type_size, type_comment)

    for type_name, type_size, type_comment in (
        (
            "IVP_Core_Fast_Static",
            0x60,
            "Ballance Core static prefix with no vptr. Retail field accesses "
            "confirm rot_inertia +0x14, inv_rot_inertia +0x34, objects +0x50 "
            "and friction backlink +0x5c; the five public getters are inline.",
        ),
        (
            "IVP_Core_Fast_PSI",
            0x1A8,
            "Ballance PSI prefix derived from the 0x60 static prefix. Retail "
            "motion and next-PSI bodies confirm state +0x60 through matrix "
            "+0x128; total size is 0x1a8.",
        ),
        (
            "IVP_Core_Fast",
            0x1C8,
            "Ballance fast Core prefix derived from IVP_Core_Fast_PSI. The "
            "rotation axis begins at +0x1a8 and scalar motion cache ends at "
            "+0x1c4; the following IVP_Core member starts at +0x1c8.",
        ),
        (
            "IVP_Core",
            0x238,
            "Ballance complete Core. The restored public Fast_Static/Fast_PSI/"
            "Fast hierarchy preserves the retail 0x238 layout and all existing "
            "instruction-confirmed member offsets.",
        ),
    ):
        set_struct_comment(type_name, type_size, type_comment)

    set_struct_comment(
        "IVP_Multidimensional_Interpolator",
        0x40,
        "Ballance imported DEBUG-layout interpolation utility: pointer tables "
        "+0x00/+0x04, three signed 8-bit dimensions +0x08..+0x0a, runtime "
        "state +0x0c..+0x28 and four diagnostic pointers plus the exact-hit "
        "counter +0x2c..+0x3c. All class-specific bodies are link-stripped; "
        "the public implementation is a documented nearby-source "
        "reconstruction and has no fabricated retail RVA.",
    )
    set_struct_comment(
        "IVP_Statisticsmanager_Console_Callback",
        0x04,
        "Ballance imported stock console sink: exactly the three-slot Better "
        "Statistics callback-interface vptr with no additional storage. The "
        "constructor and private output/enable/disable bodies are "
        "link-stripped and selectively reconstructed from the complete nearby "
        "implementation; no concrete retail vtable or RVA is fabricated.",
    )
    set_struct_comment(
        "IVP_Halfspacesoup",
        0x08,
        "Ballance imported halfspace owner: exactly the 0x08 "
        "IVP_U_Vector<IVP_U_Hesse> base with no tail fields. All four public "
        "methods are link-stripped and selectively reconstructed from the "
        "complete nearby implementation; exact-DLL tetra round trips verify "
        "plane orientation and ownership without assigning fake RVAs.",
    )
    set_struct_comment(
        "IVP_SurfaceBuilder_Halfspacesoup",
        0x01,
        "Ballance imported stateless halfspace builder. Its three static "
        "bodies are link-stripped; the public reconstruction uses verified "
        "Hesse/intersection/vector primitives and retained retail Pointsoup "
        "and Ledge Soup compilation entries.",
    )
    set_struct_comment(
        "IVP_Compact_Modify",
        0x01,
        "Ballance imported stateless compact-geometry helper. Its three "
        "static bodies are link-stripped and selectively reconstructed by "
        "composing the exact retail Polygon surface-manager traversal with "
        "the verified Halfspacesoup, Pointsoup and Ledge Soup paths. No fake "
        "member RVA is assigned.",
    )
    set_struct_comment(
        "IVP_Vec_PCore",
        0x10,
        "Ballance imported direction helper: exactly the 0x10 "
        "IVP_U_Float_Point base with no tail storage or vptr. Its sole "
        "constructor is link-stripped and reconstructed as inverse rotation "
        "through the instruction-confirmed Core matrix at +0x128.",
    )
    for type_name, type_size, description in (
        ("IVP_Template_Grid_Axle_Descript", 0x0C,
         "Three 32-bit fields n_points/maps_to/invert_axis."),
        ("IVP_Template_Compact_Grid", 0x34,
         "Two axle descriptors, height mapping, field size and 0x10 origin."),
        ("IVP_Compact_Grid_Element", 0x04,
         "Two signed 16-bit compact-ledge indices."),
        ("IVP_Compact_Grid", 0xB0,
         "Variable-size aligned header ending in compact ledge offset table."),
        ("IVP_GridBuilder_Array", 0x440,
         "Heightfield compiler workspace including the 516-entry point map."),
        ("IVP_SurfaceManager_Grid", 0x08,
         "Surface-manager vptr plus compact_grid pointer at +0x04."),
    ):
        set_struct_comment(
            type_name,
            type_size,
            "Ballance imported Grid layout agrees field-for-field with the "
            "nearby public source. " + description + " No class-specific "
            "retail function or concrete Grid vtable has been located. The "
            "header accessors, SurfaceManager and conservative heightfield "
            "compiler are source reconstructions validated with retained "
            "retail compact ledges and Pointsoup triangle construction.",
        )
    set_struct_comment(
        "IVP_Compact_Surface",
        0x30,
        "Ballance imported compact collision-surface header. Exact Pointsoup "
        "allocation sizes and retained Polygon-manager field reads agree with "
        "the 0x30 layout. The two public endian bodies are link-stripped and "
        "selectively reconstructed with the neighboring MSVC/PowerPC packed-"
        "bitfield conversion; an exact-DLL tetra serialization validates all "
        "shared hull points, triangles and edges.",
    )
    for type_name, type_size, description in (
        ("IVP_Concave_Polyhedron_Face_Pointoffset", 0x04,
         "One signed 32-bit index into the polyhedron point vector."),
        ("IVP_Concave_Polyhedron_Face", 0x08,
         "One owning IVP_U_Vector of point-index records."),
        ("IVP_Concave_Polyhedron", 0x18,
         "Two IVP_U_BigVector members containing borrowed points and faces."),
        ("IVP_Convex_Subpart", 0x08,
         "One owning IVP_U_Vector of decomposed points."),
        ("IVP_Convex_Decompositor_Parameters", 0x0C,
         "Three float tolerances: tolin, angacc and rdacc."),
        ("IVP_SurfaceBuilder_Polyhedron_Concave", 0x01,
         "Stateless builder; all three adapters are restored."),
        ("IVP_Convex_Decompositor", 0x01,
         "Stateless decompositor with no surviving GEOMPACK workspace."),
    ):
        set_struct_comment(
            type_name,
            type_size,
            "Ballance imported Concave Polyhedron layout agrees field-for-"
            "field with nearby source. " + description + " Class-specific "
            "bodies are link-stripped; restored methods are documented "
            "nearby-source reconstructions using retained Pointsoup.",
        )
    set_struct_comment(
        "IVP_Template_SurfaceBuilder_3ds",
        0x04,
        "Ballance imported 3DS builder template: exactly one float scale at "
        "+0x00. Its constructor is link-stripped and reconstructed from the "
        "nearby public source with a 1.0 default.",
    )
    set_struct_comment(
        "IVP_SurfaceBuilder_3ds",
        0x01,
        "Ballance imported stateless 3DS collision-mesh adapter. No function "
        "body survives in physics_RT.dll. The public conversion is restored "
        "as a bounded category-2 chunk parser producing the confirmed "
        "IVP_Concave_Polyhedron layout; compact geometry remains compiled by "
        "retained Ballance Pointsoup and Ledge Soup bodies.",
    )
    for type_name, type_size, description in (
        ("dmodel_t", 0x40, "Quake model bounds, origin and four headnodes."),
        ("lump_t", 0x08, "Quake file offset and byte length."),
        ("dheader_t", 0x7C, "Version followed by fifteen lump records."),
        ("dplane_t", 0x14, "Float normal/dist followed by plane type."),
        ("dnode_t", 0x18, "Plane, two children, short bounds and face range."),
        ("dclipnode_t", 0x08, "Plane and two signed child indices."),
        ("IVP_q12_int", 0x04, "One integer sentinel value."),
        ("IVP_SurfaceBuilder_Q12", 0x6C,
         "Counts and BSP pointers through +0x20, sentinels and ownership at "
         "+0x24..+0x2c, conversion state at +0x30..+0x5c, path vector at "
         "+0x60 and halfspaces at +0x68."),
    ):
        set_struct_comment(
            type_name,
            type_size,
            "Ballance imported Q1/Q2 BSP layout agrees field-for-field with "
            "the nearby public source. " + description + " The seven public "
            "bodies are link-stripped and category-2 restored; convex output "
            "is still produced by retained Ballance Pointsoup/Ledge Soup.",
        )
    set_struct_comment(
        "IVP_Compact_Mopp",
        0x30,
        "Ballance imports this MOPP header field-for-field: float mass and "
        "inertia vectors, radius, MSVC 8/24 deviation-and-size bitfield, "
        "root/ledge/hull offsets and alignment dummy. Header accessors and "
        "header-only endian conversion are category-2 restored. byte_swap_all "
        "is deliberately unavailable because physics_RT.dll contains no "
        "hkMoppCode layout or MOPP endian walker; partially swapping embedded "
        "bytecode would corrupt geometry. MOPP manager/builder types are also "
        "absent and are not fabricated.",
    )

def apply_vtable_layouts() -> None:
    rename_struct_member(
        "IVP_Car_System_Real_Wheels_vtbl",
        0x74,
        "GetCarSystemDebugData",
        0x00,
        "dtr_IVP_Car_System_Real_Wheels",
        "Ballance Car System base slot 0 is the virtual destructor; the debug "
        "getter is independently present at slot 27.",
    )
    rename_struct_member(
        "IVP_Car_System_Real_Wheels_vtbl",
        0x74,
        "GetCarSystemDebugData_2",
        0x6C,
        "GetCarSystemDebugData",
        "Ballance Car System slot 27, immediately after SetCarSystemDebugData.",
    )
    set_struct_member_type(
        "IVP_Car_System_Real_Wheels_vtbl",
        0x74,
        "dtr_IVP_Car_System_Real_Wheels",
        0x00,
        "void (__thiscall *)(IVP_Car_System_Real_Wheels *)",
        "Virtual destructor slot inherited from IVP_Car_System.",
    )
    for car_vtable_name, car_vtable_size in (
        ("IVP_Car_System_vtbl", 0x70),
        ("IVP_Car_System_Real_Wheels_vtbl", 0x74),
    ):
        for member_name, member_offset, parameters in (
            (
                "get_body_speed", 0x3C,
                ", IVP_COORDINATE_INDEX",
            ),
            (
                "get_wheel_angular_velocity", 0x40,
                ", IVP_POS_WHEEL",
            ),
            ("get_orig_front_wheel_distance", 0x48, ""),
            ("get_orig_axles_distance", 0x4C, ""),
        ):
            set_struct_member_type(
                car_vtable_name,
                car_vtable_size,
                member_name,
                member_offset,
                "double (__thiscall *)(IVP_Car_System *" + parameters + ")",
                "Ballance IVP_DOUBLE is the x86 MSVC 64-bit double ABI; the "
                "imported long-double spelling was a type-library artifact.",
            )

    set_struct_comment(
        "IVP_PerformanceCounter_Simple",
        0xA8,
        "Ballance Windows six-slot performance counter. Constructor RVA "
        "0x14fa0 installs vtable 0x100636b0 and clears 0x29 dwords after the "
        "vptr; count_PSIs increments on the UNIVERSE pcount transition.",
    )
    set_struct_comment(
        "IVP_PerformanceCounter_Simple_vtbl",
        0x18,
        "Ballance retail slots: start, pcount, stop, environment deletion, "
        "reset/print, scalar deleting destructor. The imported slot-0 "
        "destructor label was stale.",
    )
    rename_struct_member(
        "IVP_PerformanceCounter_Simple_vtbl",
        0x18,
        "dtr_IVP_PerformanceCounter_Simple",
        0x00,
        "start_pcount",
        "Retail vtable 0x100636b0 slot 0 targets RVA 0x15050.",
    )
    rename_struct_member(
        "IVP_PerformanceCounter_Simple_vtbl",
        0x18,
        "dtr_IVP_PerformanceCounter",
        0x14,
        "scalar_deleting_destructor",
        "Retail vtable 0x100636b0 slot 5 targets RVA 0x14fc0.",
    )
    for member_name, member_offset, declaration, member_comment in (
        (
            "start_pcount", 0x00,
            "void (__thiscall *)(IVP_PerformanceCounter_Simple *)",
            "Retail slot 0, RVA 0x15050.",
        ),
        (
            "pcount", 0x04,
            "void (__thiscall *)(IVP_PerformanceCounter_Simple *, IVP_PERFORMANCE_ELEMENT)",
            "Retail slot 1, RVA 0x14fe0.",
        ),
        (
            "stop_pcount", 0x08,
            "void (__thiscall *)(IVP_PerformanceCounter_Simple *)",
            "Retail slot 2, shared no-op RVA 0x28220.",
        ),
        (
            "environment_is_going_to_be_deleted", 0x0C,
            "void (__thiscall *)(IVP_PerformanceCounter_Simple *, IVP_Environment *)",
            "Retail slot 3, RVA 0x14f60.",
        ),
        (
            "reset_and_print_performance_counters", 0x10,
            "void (__thiscall *)(IVP_PerformanceCounter_Simple *, IVP_Time)",
            "Retail slot 4, RVA 0x14e00 with an eight-byte by-value time.",
        ),
        (
            "scalar_deleting_destructor", 0x14,
            "void *(__thiscall *)(IVP_PerformanceCounter_Simple *, unsigned int)",
            "Retail slot 5, RVA 0x14fc0.",
        ),
    ):
        set_struct_member_type(
            "IVP_PerformanceCounter_Simple_vtbl",
            0x18,
            member_name,
            member_offset,
            declaration,
            member_comment,
        )

    rename_struct_member(
        "IVP_Constraint_Solver_Car_vtbl",
        0x1C,
        "get_controller_priority",
        0x00,
        "core_is_going_to_be_deleted_event",
        "Base slot 0. The imported derived label was wrong; the neighboring "
        "class overrides the verified IVP_Controller deletion callback here.",
    )
    rename_struct_member(
        "IVP_Constraint_Solver_Car_vtbl",
        0x1C,
        "get_controller_priority_2",
        0x14,
        "get_controller_priority",
        "Base slot 5. The duplicate suffix only resulted from the incorrect "
        "slot-0 import label.",
    )
    for member_name, member_offset, declaration, member_comment in (
        (
            "core_is_going_to_be_deleted_event",
            0x00,
            "void (__thiscall *)(IVP_Constraint_Solver_Car *, IVP_Core *)",
            "Base slot 0. The neighboring class overrides the verified "
            "IVP_Controller core-deletion callback; the imported priority "
            "return type was also wrong.",
        ),
        (
            "get_minimum_simulation_frequency",
            0x04,
            "double (__thiscall *)(IVP_Constraint_Solver_Car *)",
            "Base slot 1. Ballance IVP_DOUBLE is the MSVC 64-bit double "
            "ABI; the neighboring override returns 30.0.",
        ),
    ):
        set_struct_member_type(
            "IVP_Constraint_Solver_Car_vtbl",
            0x1C,
            member_name,
            member_offset,
            declaration,
            member_comment,
        )

    rename_struct_member(
        "IVP_Listener_Collision_vtbl",
        0x14,
        "event_post_collision",
        0x04,
        "event_collision_object_deleted",
        "Retail slot 1 is selected by callback bit 0x02 and receives the "
        "IVP_Real_Object being removed from collision detection.",
    )
    set_struct_member_type(
        "IVP_Listener_Collision_vtbl",
        0x14,
        "event_collision_object_deleted",
        0x04,
        "void (__thiscall *)(IVP_Listener_Collision *, IVP_Real_Object *)",
        "Retail slot 1 object-deletion callback; the imported collision-event "
        "parameter belonged to the stale source slot label.",
    )
    rename_struct_member(
        "IVP_Listener_Collision_vtbl",
        0x14,
        "event_pre_collision",
        0x00,
        "event_post_collision",
        "Retail slot 0 is the post-collision callback selected by bit 0x01; "
        "Ballance has no pre-collision slot.",
    )
    set_struct_comment(
        "IVP_Listener_Collision_vtbl",
        0x14,
        "Ballance five-slot listener table: post collision, object deleted, "
        "friction created, friction deleted, deleting destructor. The nearby "
        "pre-collision and friction-pair callbacks are absent.",
    )

def apply_names_and_vtable_notes() -> None:
    stage_friction_event_name_swaps()
    for correction in CORRECTIONS:
        set_function(*correction)

    set_data(
        0x10075DB0,
        "ivp_mindist_settings",
        "IVP_Mindist_Settings corrected",
        "Retail global IVP_Mindist_Settings, size 0x138. Static initializer "
        "at VA 0x10015FD0 loads this exact address.",
    )
    set_data(
        0x10077AD8,
        "ivp_debugmanager",
        "IVP_BetterDebugmanager corrected",
        "Retail process-global debug manager, size 0x2008. Static initializer "
        "at VA 0x10060C90 passes this exact address to the retained constructor.",
    )
    set_data(
        0x100763A8,
        "ivp_pointsoup_single_tri_ledge",
        "IVP_Compact_Ledge *corrected",
        "Retail cached canonical triangle pointer. Pointsoup triangle converter "
        "RVA 0x3AC00 reads and initializes this exact slot.",
    )
    set_data(
        0x100685B4,
        "IVP_RAND_SEED",
        "int corrected",
        "Retail process-global random seed. ivp_rand RVA 0x2FCD0 reads it, "
        "multiplies it by 75, writes it back, and scales the low 16 bits. "
        "The link-stripped public ivp_srand/ivp_srand_read compatibility "
        "functions must use this exact DLL-owned storage.",
    )

    set_named_address(
        0x10063D34,
        "??_7IVP_BetterDebugmanager@@6B@",
        "Ballance IVP_BetterDebugmanager vtable: output_function followed by "
        "the scalar deleting destructor.",
    )

    set_named_address(
        0x10063A00,
        "??_7IVP_OV_Element@@6B@",
        "Ballance final IVP_OV_Element five-slot vtable: hull type, hull-limit "
        "callback, hull-manager deletion callback, base reset callback and "
        "scalar deleting destructor.",
    )
    set_named_address(
        0x10063A24,
        "??_7IVP_ov_tree_hash@@6B@",
        "Ballance final IVP_ov_tree_hash two-slot vtable: node-data comparator "
        "followed by the concrete scalar deleting destructor. This table is "
        "owned by the helper allocated in IVP_OV_Tree_Manager construction; "
        "the manager itself is non-polymorphic.",
    )

    set_named_address(
        0x1006364C,
        "??_7IVP_Collision_Filter@@6B@",
        "Ballance abstract Collision Filter base vtable: two pure callbacks "
        "followed by its scalar deleting destructor.",
    )
    set_named_address(
        0x10063698,
        "??_7IVP_PerformanceCounter@@6B@",
        "Ballance abstract Performance Counter base vtable: five pure "
        "callbacks followed by its scalar deleting destructor.",
    )
    set_named_address(
        0x100637A0,
        "??_7IVP_Synapse@@6B@",
        "Ballance Synapse base vtable. The retained hull callbacks and "
        "slot-4 scalar deleting destructor identify this table independently "
        "of stale imported slot names.",
    )
    set_data(
        0x100637B4,
        "??_7IVP_Mindist@@6B@",
        "IVP_Mindist_vtbl corrected",
        "Ballance IVP_Mindist eight-slot primary vtable. Constructor RVA "
        "0x16290 installs this address point; the neighboring-source virtual "
        "is_recursive slot is absent.",
    )
    set_data(
        0x10063AC8,
        "??_7IVP_Mindist_Recursive@@6BIVP_Collision_Delegator@@@",
        "IVP_Mindist_Recursive_Collision_Delegator_vtbl corrected",
        "Ballance Recursive Mindist two-slot secondary-base table. ECX is "
        "adjusted to complete object +0x88 for both callback and deleting "
        "destructor thunk.",
    )
    set_data(
        0x10063AD0,
        "??_7IVP_Mindist_Recursive@@6BIVP_Mindist@@@",
        "IVP_Mindist_Recursive_vtbl corrected",
        "Ballance Recursive Mindist eight-slot primary table. The retained "
        "constructor installs it at complete object +0x00.",
    )
    set_data(
        0x10063A34,
        "??_7IVP_Collision_Delegator@@6B@",
        "IVP_Collision_Delegator_vtbl corrected",
        "Ballance abstract Collision Delegator two-slot base vtable, used by "
        "the secondary base subobjects in Recursive Mindist and OO Watcher.",
    )
    set_data(
        0x10063A3C,
        "??_7IVP_Collision_Delegator_Root_Mindist@@6B@",
        "IVP_Collision_Delegator_Root_Mindist_vtbl corrected",
        "Ballance concrete Root Mindist five-slot vtable. Constructor RVA "
        "0x2F5A0 installs this distinct address immediately after the "
        "two-slot Collision Delegator base table.",
    )
    set_named_address(
        0x10063C68,
        "??_7IVP_Triangle@@6B@",
        "Ballance one-slot IVP_Triangle vtable containing only the scalar "
        "deleting destructor.",
    )

    set_named_address(
        0x1006344C,
        "??_7IVP_Material@@6B@",
        "Ballance retail IVP_Material vtable: five pure virtual material "
        "property queries followed by the scalar deleting destructor.",
    )
    set_named_address(
        0x100633C0,
        "??_7IVP_Collision_Callback_Table_Hash@@6B@",
        "Ballance collision callback-table hash vtable: folded pointer "
        "comparison followed by its scalar deleting destructor.",
    )
    set_named_address(
        0x100633C8,
        "??_7IVP_Object_Callback_Table_Hash@@6B@",
        "Ballance object callback-table hash vtable: folded pointer "
        "comparison followed by its scalar deleting destructor.",
    )
    set_named_address(
        0x100633D0,
        "??_7IVP_Friction_System@@6B@",
        "Ballance IVP_Friction_System primary controller vtable, seven slots.",
    )
    set_named_address(
        0x100633EC,
        "??_7IVP_Friction_Sys_Energy@@6B@",
        "Ballance seven-slot energy-friction controller vtable. The owner "
        "embeds this 0x08 subobject at +0x10.",
    )
    set_named_address(
        0x10063408,
        "??_7IVP_Friction_Sys_Static@@6B@",
        "Ballance seven-slot static-friction controller vtable. The owner "
        "embeds this 0x08 subobject at +0x08.",
    )
    set_named_address(
        0x10063464,
        "??_7IVP_Material_Simple@@6B@",
        "Ballance retail IVP_Material_Simple vtable: friction, second "
        "friction, elasticity, adhesion, name, deleting destructor.",
    )
    set_named_address(
        0x10063240,
        "??_7PhysicsControllerForce@@6B@",
        "Ballance Building Block adapter vtable. This is not an "
        "IVP_Actuator_Force vtable.",
    )
    set_named_address(
        0x1006325C,
        "??_7IVP_Controller@@6B@",
        "Ballance retail IVP_Controller abstract base vtable, seven slots.",
    )
    set_named_address(
        0x10063390,
        "??_7IVP_Real_Object@@6B@",
        "Ballance retail IVP_Real_Object vtable: deleting destructor, "
        "set_new_quat_object_f_core, set_new_m_object_f_core.",
    )
    set_named_address(
        0x100633A0,
        "??_7IVP_Object@@6B@",
        "Ballance retail IVP_Object base vtable: one deleting-destructor slot.",
    )
    set_named_address(
        0x100633A4,
        "??_7IVP_Cluster@@6B@",
        "Ballance retail IVP_Cluster vtable: one scalar deleting-destructor slot.",
    )
    set_named_address(
        0x10063434,
        "??_7IVP_Material_Manager@@6B@",
        "Ballance retail IVP_Material_Manager vtable, six slots including "
        "the Ballance-only adhesion query at slot 3.",
    )
    set_named_address(
        0x10063524,
        "??_7IVP_Attacher_To_Cores_Buoyancy@@6B@",
        "Ballance retail buoyancy attacher vtable: three active-set "
        "callbacks, deleting destructor, parameters query, surface query.",
    )
    set_named_address(
        0x10063658,
        "??_7IVP_Collision_Filter_Coll_Group_Ident@@6B@",
        "Ballance retail collision-group-ident filter vtable: collision "
        "query, environment deletion, scalar deleting destructor.",
    )
    set_named_address(
        0x1006366C,
        "??_7IVP_Collision_Filter_Exclusive_Pair@@6B@",
        "Ballance retail exclusive-pair filter vtable: collision query, "
        "environment deletion, scalar deleting destructor.",
    )
    set_named_address(
        0x10063678,
        "??_7IVP_Meta_Collision_Filter@@6B@",
        "Ballance retail meta collision-filter vtable: collision query, "
        "environment deletion, scalar deleting destructor.",
    )
    set_named_address(
        0x10063A50,
        "??_7IVP_Anomaly_Limits@@6B@",
        "Ballance retail anomaly-limits vtable: environment callback then "
        "deleting destructor.",
    )
    set_named_address(
        0x10063A58,
        "??_7IVP_Anomaly_Manager@@6B@",
        "Ballance retail anomaly-manager vtable: six callbacks followed by "
        "the deleting destructor.",
    )
    set_named_address(
        0x10063598,
        "??_7IVP_Standard_Gravity_Controller@@6B@",
        "Ballance retail IVP_Standard_Gravity_Controller vtable, seven slots.",
    )
    set_named_address(
        0x100635C0,
        "??_7IVP_Actuator@@6B@",
        "Ballance retail IVP_Actuator abstract base vtable, eight slots.",
    )
    set_named_address(
        0x100635E0,
        "??_7IVP_Actuator_Two_Point@@6B@",
        "Ballance retail IVP_Actuator_Two_Point abstract vtable, eight slots.",
    )
    set_named_address(
        0x10063B20,
        "??_7IVP_Constraint@@6B@",
        "Ballance retail IVP_Constraint abstract base vtable, 23 slots. "
        "Only do_simulation_controller is pure; the sixteen mutation slots "
        "are retained diagnostic defaults.",
    )
    set_named_address(
        0x10063930,
        "??_7IVP_Constraint_Local@@6B@",
        "Ballance retail IVP_Constraint_Local vtable, 23 slots: seven "
        "controller slots followed by sixteen constraint mutation slots.",
    )
    set_named_address(
        0x100631E0,
        "??_7IVP_SurfaceManager_Polygon@@6B@",
        "Ballance retail IVP_SurfaceManager_Polygon vtable, 11 slots.",
    )
    set_named_address(
        0x10063890,
        "??_7IVP_SurfaceManager@@6B@",
        "Ballance retail IVP_SurfaceManager abstract base vtable, 11 slots.",
    )
    set_named_address(
        0x10063604,
        "??_7IVP_Actuator_Spring@@6B@",
        "Ballance retail IVP_Actuator_Spring primary vtable, eight slots.",
    )
    set_named_address(
        0x10063624,
        "??_7IVP_Actuator_Spring_Active@@6BIVP_U_Active_Float_Listener@@@",
        "IVP_Actuator_Spring_Active secondary listener vtable, one slot.",
    )
    set_named_address(
        0x10063628,
        "??_7IVP_Actuator_Spring_Active@@6BIVP_Actuator_Spring@@@",
        "IVP_Actuator_Spring_Active primary actuator vtable, eight slots.",
    )
    set_named_address(
        0x100636D0,
        "??_7IVP_Active_Value_Hash@@6B@",
        "Ballance retail IVP_Active_Value_Hash vtable: name comparison and "
        "scalar deleting destructor.",
    )
    set_named_address(
        0x100636D8,
        "??_7IVP_U_Active_Value_Manager@@6B@",
        "Ballance retail IVP_U_Active_Value_Manager vtable, 15 slots.",
    )
    for slot_address, slot_comment in (
        (0x100636D0, "slot 0: IVP_Active_Value_Hash::compare"),
        (0x100636D4, "slot 1: IVP_Active_Value_Hash scalar deleting destructor"),
        (0x100636D8, "slot 0: IVP_U_Active_Value_Manager scalar deleting destructor"),
        (0x100636DC, "slot 1: environment_will_be_deleted"),
        (0x100636E0, "slot 2: insert_active_float"),
        (0x100636E4, "slot 3: remove_active_float"),
        (0x100636E8, "slot 4: insert_active_int"),
        (0x100636EC, "slot 5: remove_active_int"),
        (0x100636F0, "slot 6: delay_active_float"),
        (0x100636F4, "slot 7: delay_active_int"),
        (0x100636F8, "slot 8: update_delayed_active_values"),
        (0x100636FC, "slot 9: init_active_values_generic"),
        (0x10063700, "slot 10: refresh_psi_active_values"),
        (0x10063704, "slot 11: install_active_float"),
        (0x10063708, "slot 12: create_active_float"),
        (0x1006370C, "slot 13: install_active_int"),
        (0x10063710, "slot 14: create_active_int"),
    ):
        ida_bytes.set_cmt(slot_address, slot_comment, True)
    for slot_address, slot_comment in (
        (0x10063D34, "slot 0: IVP_BetterDebugmanager::output_function"),
        (0x10063D38, "slot 1: IVP_BetterDebugmanager scalar deleting destructor"),
    ):
        ida_bytes.set_cmt(slot_address, slot_comment, True)
    for slot_address, slot_comment in (
        (0x100633C0, "slot 0: folded callback-table object-pointer compare"),
        (0x100633C4, "slot 1: collision callback-hash scalar deleting destructor"),
        (0x100633C8, "slot 0: folded callback-table object-pointer compare"),
        (0x100633CC, "slot 1: object callback-hash scalar deleting destructor"),
        (0x100633D0, "slot 0: core_is_going_to_be_deleted_event"),
        (0x100633D4, "slot 1: get_minimum_simulation_frequency"),
        (0x100633D8, "slot 2: get_associated_controlled_cores"),
        (0x100633DC, "slot 3: reset_time"),
        (0x100633E0, "slot 4: do_simulation_controller"),
        (0x100633E4, "slot 5: get_controller_priority"),
        (0x100633E8, "slot 6: scalar deleting destructor"),
        (0x1006344C, "slot 0: get_friction_factor (_purecall)"),
        (0x10063450, "slot 1: get_second_friction_factor (_purecall)"),
        (0x10063454, "slot 2: get_elasticity (_purecall)"),
        (0x10063458, "slot 3: get_adhesion (_purecall)"),
        (0x1006345C, "slot 4: get_name (_purecall)"),
        (0x10063460, "slot 5: IVP_Material scalar deleting destructor"),
        (0x10063464, "slot 0: get_friction_factor"),
        (0x10063468, "slot 1: get_second_friction_factor"),
        (0x1006346C, "slot 2: get_elasticity"),
        (0x10063470, "slot 3: get_adhesion"),
        (0x10063474, "slot 4: get_name"),
        (0x10063478, "slot 5: IVP_Material_Simple scalar deleting destructor"),
    ):
        ida_bytes.set_cmt(slot_address, slot_comment, True)
    for slot_address, slot_comment in (
        (0x100631E0, "slot 0: get_single_convex"),
        (0x100631E4, "slot 1: get_mass_center"),
        (0x100631E8, "slot 2: get_radius_and_radius_dev_to_given_center"),
        (0x100631EC, "slot 3: get_rotation_inertia"),
        (0x100631F0, "slot 4: get_all_ledges_within_radius"),
        (0x100631F4, "slot 5: get_all_terminal_ledges"),
        (0x100631F8, "slot 6: insert_all_ledges_hitting_ray"),
        (0x100631FC, "slot 7: add_reference_to_ledge"),
        (0x10063200, "slot 8: remove_reference_to_ledge"),
        (0x10063204, "slot 9: IVP_SurfaceManager_Polygon scalar deleting destructor"),
        (0x10063208, "slot 10: get_type"),
        (0x10063890, "slot 0: get_single_convex (_purecall)"),
        (0x10063894, "slot 1: get_mass_center (_purecall)"),
        (0x10063898, "slot 2: get_radius_and_radius_dev_to_given_center (_purecall)"),
        (0x1006389C, "slot 3: get_rotation_inertia (_purecall)"),
        (0x100638A0, "slot 4: get_all_ledges_within_radius (_purecall)"),
        (0x100638A4, "slot 5: get_all_terminal_ledges (_purecall)"),
        (0x100638A8, "slot 6: insert_all_ledges_hitting_ray (_purecall)"),
        (0x100638AC, "slot 7: add_reference_to_ledge (no-op)"),
        (0x100638B0, "slot 8: remove_reference_to_ledge (no-op)"),
        (0x100638B4, "slot 9: scalar deleting destructor (_purecall)"),
        (0x100638B8, "slot 10: get_type (_purecall)"),
    ):
        ida_bytes.set_cmt(slot_address, slot_comment, True)
    for slot_address, slot_comment in (
        (0x10063B20, "slot 0: IVP_Constraint::core_is_going_to_be_deleted_event (shared delete-this body)"),
        (0x10063B24, "slot 1: IVP_Constraint::get_minimum_simulation_frequency"),
        (0x10063B28, "slot 2: IVP_Constraint::get_associated_controlled_cores"),
        (0x10063B2C, "slot 3: IVP_Controller::reset_time"),
        (0x10063B30, "slot 4: do_simulation_controller (_purecall)"),
        (0x10063B34, "slot 5: IVP_Constraint::get_controller_priority"),
        (0x10063B38, "slot 6: IVP_Constraint scalar deleting destructor"),
        (0x10063B3C, "slot 7: change_fixing_point_Ros (diagnostic default)"),
        (0x10063B40, "slot 8: change_target_fixing_point_Ros (folded diagnostic default)"),
        (0x10063B44, "slot 9: change_translation_axes_Ros (diagnostic default)"),
        (0x10063B48, "slot 10: change_target_translation_axes_Ros (folded diagnostic default)"),
        (0x10063B4C, "slot 11: fix_translation_axis (diagnostic default)"),
        (0x10063B50, "slot 12: free_translation_axis (diagnostic default)"),
        (0x10063B54, "slot 13: limit_translation_axis (diagnostic default)"),
        (0x10063B58, "slot 14: change_max_translation_impulse (diagnostic default)"),
        (0x10063B5C, "slot 15: change_rotation_axes_Ros (diagnostic default)"),
        (0x10063B60, "slot 16: change_target_rotation_axes_Ros (folded diagnostic default)"),
        (0x10063B64, "slot 17: fix_rotation_axis (diagnostic default)"),
        (0x10063B68, "slot 18: free_rotation_axis (diagnostic default)"),
        (0x10063B6C, "slot 19: limit_rotation_axis (diagnostic default)"),
        (0x10063B70, "slot 20: change_max_rotation_impulse (diagnostic default)"),
        (0x10063B74, "slot 21: change_Aos_to_relaxe_constraint (diagnostic default)"),
        (0x10063B78, "slot 22: change_Ros_to_relaxe_constraint (folded diagnostic default)"),
    ):
        ida_bytes.set_cmt(slot_address, slot_comment, True)
    for slot_address, slot_comment in (
        (0x10063930, "slot 0: IVP_Constraint_Local::core_is_going_to_be_deleted_event (shared delete-this body)"),
        (0x10063934, "slot 1: IVP_Constraint::get_minimum_simulation_frequency"),
        (0x10063938, "slot 2: IVP_Constraint::get_associated_controlled_cores"),
        (0x1006393C, "slot 3: IVP_Controller::reset_time"),
        (0x10063940, "slot 4: IVP_Constraint_Local::do_simulation_controller"),
        (0x10063944, "slot 5: IVP_Constraint::get_controller_priority"),
        (0x10063948, "slot 6: IVP_Constraint_Local scalar deleting destructor"),
        (0x1006394C, "slot 7: change_fixing_point_Ros"),
        (0x10063950, "slot 8: change_target_fixing_point_Ros"),
        (0x10063954, "slot 9: change_translation_axes_Ros"),
        (0x10063958, "slot 10: change_target_translation_axes_Ros"),
        (0x1006395C, "slot 11: fix_translation_axis"),
        (0x10063960, "slot 12: free_translation_axis"),
        (0x10063964, "slot 13: limit_translation_axis"),
        (0x10063968, "slot 14: change_max_translation_impulse"),
        (0x1006396C, "slot 15: change_rotation_axes_Ros"),
        (0x10063970, "slot 16: change_target_rotation_axes_Ros"),
        (0x10063974, "slot 17: fix_rotation_axis"),
        (0x10063978, "slot 18: free_rotation_axis"),
        (0x1006397C, "slot 19: limit_rotation_axis"),
        (0x10063980, "slot 20: change_max_rotation_impulse"),
        (0x10063984, "slot 21: change_Aos_to_relaxe_constraint"),
        (0x10063988, "slot 22: change_Ros_to_relaxe_constraint"),
    ):
        ida_bytes.set_cmt(slot_address, slot_comment, True)
    for slot_address, slot_comment in (
        (0x10063598, "slot 0: IVP_Standard_Gravity_Controller::core_is_going_to_be_deleted_event"),
        (0x1006359C, "slot 1: IVP_Controller::get_minimum_simulation_frequency"),
        (0x100635A0, "slot 2: IVP_Controller_Independent::get_associated_controlled_cores"),
        (0x100635A4, "slot 3: IVP_Controller::reset_time"),
        (0x100635A8, "slot 4: IVP_Standard_Gravity_Controller::do_simulation_controller"),
        (0x100635AC, "slot 5: IVP_Standard_Gravity_Controller::get_controller_priority"),
        (0x100635B0, "slot 6: IVP_Standard_Gravity_Controller scalar deleting destructor"),
        (0x100635C0, "slot 0: IVP_Actuator::core_is_going_to_be_deleted_event"),
        (0x100635C4, "slot 1: IVP_Controller::get_minimum_simulation_frequency"),
        (0x100635C8, "slot 2: IVP_Actuator::get_associated_controlled_cores"),
        (0x100635CC, "slot 3: IVP_Controller::reset_time"),
        (0x100635D0, "slot 4: do_simulation_controller (_purecall)"),
        (0x100635D4, "slot 5: IVP_Actuator::get_controller_priority"),
        (0x100635D8, "slot 6: IVP_Actuator scalar deleting destructor"),
        (0x100635DC, "slot 7: IVP_Actuator::anchor_will_be_deleted_event"),
        (0x100635E0, "slot 0: IVP_Actuator::core_is_going_to_be_deleted_event"),
        (0x100635E4, "slot 1: IVP_Controller::get_minimum_simulation_frequency"),
        (0x100635E8, "slot 2: IVP_Actuator::get_associated_controlled_cores"),
        (0x100635EC, "slot 3: IVP_Controller::reset_time"),
        (0x100635F0, "slot 4: do_simulation_controller (_purecall)"),
        (0x100635F4, "slot 5: IVP_Actuator::get_controller_priority"),
        (0x100635F8, "slot 6: IVP_Actuator_Two_Point scalar deleting destructor"),
        (0x100635FC, "slot 7: IVP_Actuator::anchor_will_be_deleted_event"),
    ):
        ida_bytes.set_cmt(slot_address, slot_comment, True)
    for slot_address, slot_comment in (
        (0x10063240, "slot 0: PhysicsControllerForce::core_is_going_to_be_deleted_event"),
        (0x10063244, "slot 1: IVP_Controller::get_minimum_simulation_frequency"),
        (0x10063248, "slot 2: IVP_Controller_Independent::get_associated_controlled_cores"),
        (0x1006324C, "slot 3: IVP_Controller::reset_time"),
        (0x10063250, "slot 4: PhysicsControllerForce::do_simulation_controller"),
        (0x10063254, "slot 5: PhysicsControllerForce::get_controller_priority"),
        (0x10063258, "slot 6: PhysicsControllerForce scalar deleting destructor"),
        (0x1006325C, "slot 0: IVP_Controller::core_is_going_to_be_deleted_event"),
        (0x10063260, "slot 1: IVP_Controller::get_minimum_simulation_frequency"),
        (0x10063264, "slot 2: get_associated_controlled_cores (_purecall)"),
        (0x10063268, "slot 3: IVP_Controller::reset_time"),
        (0x1006326C, "slot 4: do_simulation_controller (_purecall)"),
        (0x10063270, "slot 5: get_controller_priority (_purecall)"),
        (0x10063274, "slot 6: IVP_Controller scalar deleting destructor"),
    ):
        ida_bytes.set_cmt(slot_address, slot_comment, True)

    ida_bytes.set_cmt(
        0x10063214,
        "Ballance retail IVP_Listener_Collision slots: "
        "0 post-collision, 1 object-deleted, 2 friction-created, "
        "3 friction-deleted, 4 deleting destructor. Nearby pre-collision and "
        "friction-pair slots are absent.",
        True,
    )
    for slot_address, slot_comment in (
        (0x10063218, "slot 1: event_collision_object_deleted"),
        (0x1006321C, "slot 2: event_friction_created"),
        (0x10063220, "slot 3: event_friction_deleted"),
        (0x10063224, "slot 4: scalar deleting destructor"),
    ):
        ida_bytes.set_cmt(slot_address, slot_comment, True)

def annotate_ownership_and_lifetimes() -> None:
    # These notes describe behavior observed in the retail image, rather than
    # class-specific allocation functions supplied by the public compatibility
    # headers.  Keeping that distinction explicit prevents the IDB from claiming
    # that an inline/source-reconstructed operator new/delete was linked here.
    for address, ownership_comment in (
        (
            0x1000BF00,
            "[BML cross-DLL ownership] The complete constructor installs a "
            "physics_RT vptr. Any object that can reach its scalar deleting "
            "destructor must therefore originate from the retail heap.",
        ),
        (
            0x1000BF50,
            "[BML cross-DLL ownership] When flags bit 0 is set, this scalar "
            "deleting destructor calls the retail MSVCRT operator-delete thunk "
            "at 0x10060756.",
        ),
        (
            0x10060C30,
            "[BML cross-DLL ownership] The complete constructor installs the "
            "retail vptr at 0x10063D34; scalar destruction can therefore return "
            "the outer allocation through physics_RT's CRT.",
        ),
        (
            0x10060C50,
            "[BML cross-DLL ownership] When flags bit 0 is set, this scalar "
            "deleting destructor calls the retail MSVCRT operator-delete thunk "
            "at 0x10060756.",
        ),
        (
            0x100104C0,
            "[BML cross-DLL ownership] This complete constructor installs a "
            "retail vptr; the active-set ownership path can later destroy and "
            "free the attacher inside physics_RT.",
        ),
        (
            0x10010790,
            "[BML cross-DLL ownership] When flags bit 0 is set, this scalar "
            "deleting destructor calls the retail MSVCRT operator-delete thunk "
            "at 0x10060756.",
        ),
        (
            0x1000D400,
            "[BML cross-DLL ownership] IVP_Core is non-polymorphic, but retail "
            "IVP_Real_Object construction and core-merge paths own its outer "
            "allocation; external construction must use the retail heap.",
        ),
        (
            0x1000D580,
            "[BML cross-DLL ownership] This is the complete destructor only. "
            "Retail object/core ownership paths perform the separate outer "
            "operator delete after it returns.",
        ),
        (
            0x10060756,
            "[BML cross-DLL ownership] Import thunk for the operator delete "
            "provided by the legacy MSVCRT used by physics_RT.dll.",
        ),
        (
            0x1006075C,
            "[BML cross-DLL ownership] Import thunk for the operator new "
            "provided by the legacy MSVCRT used by physics_RT.dll.",
        ),
    ):
        append_repeatable_function_comment(address, ownership_comment)

    # Complete retail bodies own these embedded-object lifetimes.  The public
    # C++ declarations therefore expose the fields through inactive union
    # storage until the exact constructor runs, and never let compiler-
    # generated member teardown repeat a retained complete destructor.
    for address, lifetime_comment in (
        (
            0x1001D340,
            "[BML complete-object lifetime] This body exclusively constructs "
            "the IVP_U_Vector at +0x00; the public wrapper supplies inactive "
            "union storage and does not preconstruct it.",
        ),
        (
            0x1002DB50,
            "[BML complete-object lifetime] This body exclusively constructs "
            "the two IVP_U_Vector members at +0x04/+0x0C; the public wrapper "
            "enters with both union slots inactive.",
        ),
        (
            0x10014D40,
            "[BML complete-object lifetime] This body exclusively constructs "
            "the filter vector at +0x08 after storing the deletion flag; the "
            "public wrapper must not preconstruct that vector.",
        ),
        (
            0x10014250,
            "[BML complete-object lifetime] This body exclusively constructs "
            "the Actuator and Two Point bases, both anchors, and the "
            "listeners_spring vector at +0x90. The public wrapper enters "
            "through storage-only base constructors and keeps that vector in "
            "inactive union storage until this body runs.",
        ),
        (
            0x100144D0,
            "[BML complete-object lifetime] This retained complete constructor "
            "owns the full Spring construction chain, installs both primary "
            "and +0x98 secondary vptrs, copies four Active Float pointers and "
            "registers the +0x98 listener subobject with each non-null value. "
            "The public wrapper performs no member preconstruction.",
        ),
        (
            0x100186E0,
            "[BML complete-object lifetime] This body clears the complete "
            "0x18-byte Mindist Manager, initializes the wheel-lookahead "
            "vector at +0x0C and stores the Environment backlink at +0x04. "
            "The public wrapper enters with the vector union slot inactive.",
        ),
        (
            0x10018710,
            "[BML complete-object lifetime] This retained complete destructor "
            "deletes the exact and invalid Mindist chains through their DLL "
            "vtable deleting destructors and tears down the +0x0C vector "
            "exactly once; outer manager deletion remains separate.",
        ),
        (
            0x10020140,
            "[BML complete-object lifetime] This retained complete destructor "
            "jumps to free_mem and owns teardown of all retail-allocated "
            "arena blocks exactly once. Caller-provided external first-block "
            "storage is deliberately not freed; outer object deletion remains "
            "a separate retail-heap operation.",
        ),
        (
            0x10014CE0,
            "[BML complete-object lifetime] This retained complete destructor "
            "owns the filter vector teardown at +0x08. The public destructor "
            "calls this body directly over inactive-union storage.",
        ),
        (
            0x10038290,
            "[BML complete-object lifetime] This body exclusively initializes "
            "all six Ledge Soup vectors at +0x20/+0x28/+0x30/+0x64/+0x6C/"
            "+0x94; the public wrapper performs no member preconstruction.",
        ),
        (
            0x10038340,
            "[BML complete-object lifetime] This retained complete destructor "
            "owns all six Ledge Soup vector teardowns; compiler-generated "
            "member destruction must not run after it.",
        ),
        (
            0x100104C0,
            "[BML complete-object lifetime] This body constructs the "
            "IVP_VHash_Store at +0x04, initializes the active-set base at "
            "+0x18 and copies the 0x4C buoyancy template to +0x1C. The public "
            "wrapper enters with all three storage regions untouched.",
        ),
        (
            0x10010650,
            "[BML complete-object lifetime] This retained complete destructor "
            "unregisters the active-set listener and destroys the hash exactly "
            "once; the public wrapper routes directly here and keeps its base "
            "destructor resource-free.",
        ),
        (
            0x1002DC00,
            "[BML complete-object lifetime] This body installs the final "
            "IVP_OV_Element vptr and exclusively constructs the 16-entry "
            "collision fvector at +0x28. The public wrapper supplies inactive "
            "union storage and allocates the outer object on the retail heap.",
        ),
        (
            0x1002DC70,
            "[BML complete-object lifetime] This retained complete destructor "
            "owns hull-manager unlinking, collision-removal notification, OV "
            "tree removal and collision-fvector teardown. Compiler-generated "
            "member destruction must not run after it.",
        ),
        (
            0x1002DFF0,
            "[BML complete-object lifetime] This body exclusively constructs "
            "the embedded IVP_OV_Node at +0x288 and its two compact vectors, "
            "allocates the IVP_ov_tree_hash on the retail heap, and initializes "
            "the 81-entry power table. The public wrapper enters with search_node "
            "held as inactive union storage.",
        ),
        (
            0x1002DEA0,
            "[BML complete-object lifetime] This body leaves the 0x14-byte node "
            "key untouched, clears parent at +0x14, and exclusively constructs "
            "the children/elements vectors at +0x18/+0x20. The compatibility "
            "class supplies both vector regions as inactive union storage.",
        ),
        (
            0x1002DEF0,
            "[BML complete-object lifetime] This private complete destructor "
            "unlinks the node from its parent, recursively destroys retail-owned "
            "children, and tears down both vectors exactly once; compiler-generated "
            "member destruction must not run after it.",
        ),
        (
            0x1002E0E0,
            "[BML complete-object lifetime] This retained complete destructor "
            "deletes the owned IVP_ov_tree_hash through its verified deleting "
            "destructor and destroys the embedded IVP_OV_Node at +0x288 exactly "
            "once; compiler-generated member teardown must not run after it.",
        ),
    ):
        append_repeatable_function_comment(address, lifetime_comment)

def main() -> None:
    # Order is significant: each stage may depend on names or layouts fixed by
    # the preceding stage. The transaction runner audits the final copy before
    # replacing the canonical database.
    check_and_annotate_retail_image()
    apply_type_definitions()
    apply_layout_corrections()
    annotate_types()
    apply_vtable_layouts()
    apply_names_and_vtable_notes()
    annotate_ownership_and_lifetimes()

    export_path = (
        idc.ARGV[1]
        if len(idc.ARGV) > 1
        else os.environ.get("BML_IVP_IDA_EXPORT_PATH")
    )
    if export_path:
        export_named_functions(Path(export_path).resolve())

    # Batch IDA normally saves on exit; save explicitly so a successful log is
    # also proof that the corrected copy was committed to disk.
    ida_loader.save_database(idc.get_idb_path(), ida_loader.DBFL_COMP)
    print(f"SAVED\t{idc.get_idb_path()}")
    ida_pro.qexit(0)


main()
