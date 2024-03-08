#ifndef BML_PHYSICS_RT_H
#define BML_PHYSICS_RT_H

#include "VxMatrix.h"
#include "CK3dEntity.h"

class IVP_Environment;
class IVP_Real_Object;

enum IVP_BOOL {
    IVP_FALSE = 0,
    IVP_TRUE = 1
};

enum IVP_OBJECT_TYPE {
    IVP_NONE,
    IVP_CLUSTER,
    IVP_POLYGON,
    IVP_BALL,
    IVP_OBJECT
};

enum IVP_Movement_Type {
    IVP_MT_UNDEFINED = 0,
    IVP_MT_MOVING = 0x01,
    IVP_MT_SLOW = 0x02,
    IVP_MT_CALM = 0x03,
    IVP_MT_NOT_SIM = 0x08,
    IVP_MT_STATIC_PHANTOM = 0x09,
    IVP_MT_STATIC = 0x10,
    IVP_MT_GET_MINDIST = 0x21
};

class IVP_U_Vector_Base {
public:
    unsigned short memsize;
    unsigned short n_elems;
    void **elems;

    void increment_mem() {
        int i;
        assert(memsize != 0xFFFF);
        void **new_elems = (void **) malloc(sizeof(void *) * 2 * (memsize + 1));
        int newMemsize = memsize * 2 + 1;
        if (newMemsize > 0xFFFF) {
            memsize = 0xFFFF;
        } else {
            memsize = newMemsize;
        }

        for (i = 0; i < n_elems; i++) {
            new_elems[i] = elems[i];
        }
        if (elems != (void **) (this + 1)) {
            free(elems);
        }
        elems = new_elems;
    }
};

template<class T>
class IVP_U_Vector : public IVP_U_Vector_Base {
public:
    void ensure_capacity() {
        if (n_elems >= memsize) {
            increment_mem();
        }
    }

public:
    explicit IVP_U_Vector(int size = 0) : IVP_U_Vector_Base() {
        memsize = size;
        n_elems = 0;
        if (size) {
            elems = (void **) malloc(size * sizeof(void *));
        } else {
            elems = (void **) nullptr;
        }
    }

    IVP_U_Vector(void **ielems, int size) : IVP_U_Vector_Base() {
        assert(ielems == (void **) (this + 1));
        elems = ielems;
        memsize = size;
        n_elems = 0;
    }

    ~IVP_U_Vector() {
        clear();
    }

    void clear() {
        if (elems != (void **) (this + 1)) {
            void *dummy = (void *) elems;
            free(dummy);
            elems = 0;
            memsize = 0;
        }
        n_elems = 0;
    }

    void remove_all() {
        n_elems = 0;
    }

    int len() const {
        return n_elems;
    }

    int index_of(T *elem) {
        int i;
        for (i = n_elems - 1; i >= 0; i--) {
            if (elems[i] == elem)
                break;
        }
        return i;
    }

    int add(T *elem) {
        ensure_capacity();
        elems[n_elems] = (void *) elem;
        return n_elems++;
    }

    int install(T *elem) {
        int old_index = index_of(elem);
        if (old_index != -1) return old_index;
        ensure_capacity();
        elems[n_elems] = (void *) elem;
        return n_elems++;
    }

    void swap_elems(int index1, int index2) {
        assert((index1 >= 0) && (index1 < n_elems));
        assert((index2 >= 0) && (index2 < n_elems));
        void *buffer = elems[index1];
        elems[index1] = elems[index2];
        elems[index2] = buffer;
    }

    void insert_after(int index, T *elem) {
        assert((index >= 0) && (index < n_elems));
        index++;
        ensure_capacity();
        int j = n_elems;
        while (j > index) {
            elems[j] = elems[--j];
        }
        elems[index] = (void *) elem;
        n_elems++;
    }

    void remove_at(int index) {
        assert((index >= 0) && (index < n_elems));
        int j = index;
        while (j < n_elems - 1) {
            elems[j] = elems[j + 1];
            j++;
        }
        n_elems--;
    }

    void reverse() {
        for (int i = 0; i < (n_elems / 2); i++) {
            swap_elems(i, n_elems - 1 - i);
        }
    }

    void remove_at_and_allow_resort(int index) {
        assert((index >= 0) && (index < n_elems));
        n_elems--;
        elems[index] = elems[n_elems];
    }

    void remove_allow_resort(T *elem) {
        int index = index_of(elem);
        assert(index >= 0);
        n_elems--;
        elems[index] = elems[n_elems];
    }

    void remove(T *elem) {
        int index = index_of(elem);
        assert(index >= 0);
        n_elems--;
        while (index < n_elems) {
            elems[index] = (elems + 1)[index];
            index++;
        }
    }

    T *element_at(int index) const {
        assert(index >= 0 && index < n_elems);
        return (T *) elems[index];
    }
};

class IVP_U_Point {
public:
    double k[3];
    double hesse_val;
};

class IVP_U_Float_Point {
public:
    float k[3];
    float hesse_val;
};

class IVP_U_Float_Hesse : public IVP_U_Float_Point {};

class IVP_U_Matrix3 {
public:
    IVP_U_Point rows[3];

    double get_elem(int row, int col) const { return rows[row].k[col];};
    void set_elem(int row, int col, double val){ rows[row].k[col] = val; };
};

class IVP_U_Matrix : public IVP_U_Matrix3 {
public:
    IVP_U_Point vv;

    IVP_U_Point *get_position() { return &vv; };
    const IVP_U_Point *get_position() const { return &vv; };
};

class IVP_U_Quat {
public:
    double x, y, z, w;

    void set_quaternion(const IVP_U_Matrix3 *mat);
};

class IVP_U_Float_Quat {
public:
    float x, y, z, w;
};

class IVP_U_Min_List_Element {
public:
    unsigned short long_next;
    unsigned short long_prev;
    unsigned short next;
    unsigned short prev;
    float value;
    void *element;
};

class IVP_U_Min_List {
public:
    unsigned short malloced_size;
    unsigned short free_list;
    IVP_U_Min_List_Element *elems;
    float min_value;
    unsigned short first_long;
    unsigned short first_element;
    unsigned short counter;
};

class IVP_Time {
public:
    IVP_Time() : seconds(0) {}
    explicit IVP_Time(double time) { seconds = time; };

    IVP_Time operator+(double val) const {
        IVP_Time result;
        result.seconds = this->seconds + val;
        return result;
    }
    double operator-(const IVP_Time &b) const { return float(this->seconds - b.seconds); }
    void operator+=(double val) { seconds += val; }
    void operator-=(const IVP_Time b) { this->seconds -= b.seconds; }

    double get_seconds() const { return seconds; };
    double get_time() const { return seconds; };

private:
    double seconds;
};

class IVP_Vector_of_Objects : public IVP_U_Vector<IVP_Real_Object> {
public:
    IVP_Real_Object *elem_buffer[1];

    IVP_Vector_of_Objects() : IVP_U_Vector<IVP_Real_Object>((void **) &elem_buffer[0], 1) { ; };

    void reset() {
        elems = (void **) &elem_buffer[0];
        memsize = 1;
    }
};

union IVP_Core_Friction_Info {
    class IVP_Friction_Hash *l_friction_info_hash;
    class IVP_Friction_Info_For_Core *moveable_core_friction_info;
};

class IVP_Old_Sync_Rot_Z {
public:
    IVP_U_Float_Point old_sync_rot_speed;
    IVP_U_Quat old_sync_q_world_f_core_next_psi;
    IVP_BOOL was_pushed_during_i_s;
};

class IVP_Hull_Manager_Base_Gradient {
public:
    IVP_Time last_vpsi_time;
    float gradient;
    float center_gradient;
    float hull_value_last_vpsi;
    float hull_center_value_last_vpsi;
    float hull_value_next_psi;
    int time_of_next_reset;
};

class IVP_Hull_Manager_Base : public IVP_Hull_Manager_Base_Gradient {
public:
    IVP_U_Min_List sorted_synapses;
};

class IVP_Anchor {
public:
    IVP_Anchor *anchor_next_in_object;
    IVP_Anchor *anchor_prev_in_object;
    IVP_Real_Object *l_anchor_object;
    IVP_U_Float_Point object_pos;
    IVP_U_Float_Point core_pos;
    class IVP_Actuator *l_actuator;
};

class IVP_Core_Fast_Static {
public:
    IVP_BOOL fast_piling_allowed_flag: 2;
    IVP_BOOL physical_unmoveable: 2;
    IVP_BOOL is_in_wakeup_vec: 2;
    IVP_BOOL rot_inertias_are_equal: 2;
    IVP_BOOL pinned: 2;

    float upper_limit_radius;
    float max_surface_deviation;

    IVP_Environment *environment;

    class IVP_Constraint_Car_Object *car_wheel;

    IVP_U_Float_Hesse rot_inertia;
    IVP_U_Float_Point rot_speed_damp_factor;
    IVP_U_Float_Hesse inv_rot_inertia;

    float speed_damp_factor;
    float inv_object_diameter;
    IVP_U_Float_Point *spin_clipping;

    IVP_Vector_of_Objects objects;

    IVP_Core_Friction_Info core_friction_info;

    const class IVP_U_Point_4 *get_inv_masses() { return (class IVP_U_Point_4 *) &inv_rot_inertia; }
    float get_mass() const { return rot_inertia.hesse_val; };
    float get_inv_mass() const { return inv_rot_inertia.hesse_val; };

    const IVP_U_Float_Point *get_rot_inertia() const { return &rot_inertia; };
    const IVP_U_Float_Point *get_inv_rot_inertia() const { return &inv_rot_inertia; };

};

class IVP_Core_Fast_PSI : public IVP_Core_Fast_Static {
public:
    IVP_Movement_Type movement_state: 8;
    IVP_BOOL temporarily_unmovable: 8;

    short impacts_since_last_PSI;
    IVP_Time time_of_last_psi;

    float i_delta_time;

    IVP_U_Float_Point rot_speed_change;
    IVP_U_Float_Point speed_change;

    IVP_U_Float_Point rot_speed;
    IVP_U_Float_Point speed;

    IVP_U_Point pos_world_f_core_last_psi;
    IVP_U_Float_Point delta_world_f_core_psis;

    IVP_U_Quat q_world_f_core_last_psi;
    IVP_U_Quat q_world_f_core_next_psi;

    IVP_U_Matrix m_world_f_core_last_psi;
};

class IVP_Core_Fast : public IVP_Core_Fast_PSI {
public:
    IVP_U_Float_Point rotation_axis_world_space;
    float current_speed;
    float abs_omega;
    float max_surface_rot_speed;
};

class IVP_Core : public IVP_Core_Fast {  //
public:
    IVP_U_Vector<class IVP_Controller> controllers_of_core;
    class IVP_Core_Merged *merged_core_which_replace_this_core;
    class IVP_Simulation_Unit *sim_unit_of_core;

    IVP_Time time_of_calm_reference[2];
    IVP_U_Float_Quat q_world_f_core_calm_reference[2];
    IVP_U_Float_Point position_world_f_core_calm_reference[2];

    IVP_Core *union_find_father;
    IVP_Old_Sync_Rot_Z *old_sync_info;

    int mindist_event_already_done;
};

class IVP_Object {
public:
    virtual ~IVP_Object() = 0;

    IVP_OBJECT_TYPE get_type() const { return object_type; };
    const char *get_name() const { return name; };
    IVP_Environment *get_environment() const { return environment; };

    IVP_OBJECT_TYPE object_type;    // type of this object, e.g. used for correct casts...
    IVP_Object *next_in_cluster;    // next object in this cluster
    IVP_Object *prev_in_cluster;
    class IVP_Cluster *father_cluster;    // the father cluster
    const char *name;            // the name of the object, helps debugging
    IVP_Environment *environment;
};

class IVP_Real_Object_Fast_Static : public IVP_Object {
public:
    class IVP_Controller_Phantom *controller_phantom;
    class IVP_Synapse_Real *exact_synapses;
    class IVP_Synapse_Real *invalid_synapses;
    class IVP_Synapse_Friction *friction_synapses;
    IVP_U_Quat *q_core_f_object;
    IVP_U_Float_Point shift_core_f_object;
};

class IVP_Real_Object_Fast : public IVP_Real_Object_Fast_Static {
public:
    class IVP_Cache_Object *cache_object;
    IVP_Hull_Manager_Base hull_manager;
    struct {
        IVP_Movement_Type object_movement_state: 8;
        IVP_BOOL collision_detection_enabled: 2;
        IVP_BOOL shift_core_f_object_is_zero: 2;
        unsigned int object_listener_exists: 1;
        unsigned int collision_listener_exists: 1;
        unsigned int collision_listener_listens_to_friction: 1;
    } flags;
};

class IVP_Real_Object : public IVP_Real_Object_Fast {
public:
    virtual void set_new_quat_object_f_core(const IVP_U_Quat *new_quat_object_f_core, const IVP_U_Point *trans_object_f_core) = 0;
    virtual void set_new_m_object_f_core(const IVP_U_Matrix *new_m_object_f_core) = 0;

    IVP_Core *get_core() const { return physical_core; };
    IVP_Core *get_original_core() const { return original_core; };

    IVP_BOOL is_collision_detection_enabled() {
        return (IVP_BOOL)flags.collision_detection_enabled;
    };

    IVP_Anchor *anchors;
    class IVP_SurfaceManager *surface_manager;

    char nocoll_group_ident[8];
    class IVP_Material *l_default_material;
    class IVP_OV_Element *ov_element;
    float extra_radius;

    IVP_Core *physical_core;
    IVP_Core *friction_core;
    IVP_Core *original_core;

    void *client_data;

    void ensure_in_simulation();
    void enable_collision_detection(IVP_BOOL enable);
    void get_m_world_f_object_AT(IVP_U_Matrix *m_world_f_object_out);
};

struct PhysicsObject {
    CKBehavior *m_Behavior;
    IVP_Real_Object *m_RealObject;
    int field_8;
    int field_C;
    int field_10;
    int field_14;
    CKDWORD m_FrictionCount;
    int m_1C;
    IVP_Time m_FrictionTime;
    int field_28;
    void *m_ContactData;

    const char *GetName() const { return m_RealObject->get_name(); }

    CK3dEntity *GetEntity() const { return (CK3dEntity *)m_RealObject->client_data; }

    void Wake();

    bool IsStatic() const;

    void EnableCollisions(bool enable);

    float GetMass() const;
    float GetInvMass() const;

    void GetInertia(VxVector &inertia) const;
    void GetInvInertia(VxVector &inertia) const;

    void GetDamping(float *speed, float *rot);

    void GetPosition(VxVector *worldPosition, VxVector *angles);
    void GetPositionMatrix(VxMatrix &positionMatrix);

    void GetVelocity(VxVector *velocity, VxVector *angularVelocity);
    void SetVelocity(const VxVector *velocity, const VxVector *angularVelocity);
};

class IPhysicsObject
{
public:
    virtual const char *GetName() const = 0;
    virtual CK3dEntity *GetEntity() const = 0;

    virtual void SetGameData(void *data) = 0;
    virtual void *GetGameData() const = 0;

    virtual void SetGameFlags(unsigned int flags) = 0;
    virtual unsigned int GetGameFlags() const = 0;

    virtual void Wake() = 0;
    virtual void Sleep() = 0;

    virtual bool IsStatic() const = 0;
    virtual bool IsMovable() const = 0;
    virtual bool IsCollisionEnabled() const = 0;
    virtual bool IsGravityEnabled() const = 0;
    virtual bool IsMotionEnabled() const = 0;

    virtual void EnableCollisions(bool enable) = 0;
    virtual void EnableGravity(bool enable) = 0;
    virtual void EnableMotion(bool enable) = 0;

    virtual void RecheckCollisionFilter() = 0;

    virtual float GetMass() const = 0;
    virtual float GetInvMass() const = 0;
    virtual void SetMass(float mass) = 0;

    virtual void GetInertia(VxVector &inertia) const = 0;
    virtual void GetInvInertia(VxVector &inertia) const = 0;
    virtual void SetInertia(const VxVector &inertia) = 0;

    virtual void GetDamping(float *speed, float *rot) = 0;
    virtual void SetDamping(const float *speed, const float *rot) = 0;

    virtual void ApplyForceCenter(const VxVector &forceVector) = 0;
    virtual void ApplyForceOffset(const VxVector &forceVector, const VxVector &worldPosition) = 0;
    virtual void ApplyTorqueCenter(const VxVector &torqueImpulse) = 0;

    virtual void CalculateForceOffset(const VxVector &forceVector, const VxVector &worldPosition, VxVector &centerForce, VxVector &centerTorque) = 0;
    virtual void CalculateVelocityOffset(const VxVector &forceVector, const VxVector &worldPosition, VxVector &centerVelocity, VxVector &centerAngularVelocity) = 0;

    virtual void GetPosition(VxVector *worldPosition, VxVector *angles) = 0;
    virtual void GetPositionMatrix(VxMatrix& positionMatrix) = 0;

    virtual void SetPosition(const VxVector &worldPosition, const VxVector &angles, bool isTeleport) = 0;
    virtual void SetPositionMatrix(const VxMatrix& matrix, bool isTeleport) = 0;

    virtual void GetVelocity(VxVector *velocity, VxVector *angularVelocity) = 0;
    virtual void GetVelocityAtPoint(const VxVector &worldPosition, VxVector &velocity) = 0;
    virtual void SetVelocity(const VxVector *velocity, const VxVector *angularVelocity) = 0;
    virtual void AddVelocity(const VxVector *velocity, const VxVector *angularVelocity) = 0;

    virtual float GetEnergy() = 0;
};

class CKIpionManager : public CKBaseManager {
public:
    virtual void Reset() = 0;

    virtual IPhysicsObject *GetPhysicsObject(CK3dEntity *entity) = 0;

    virtual void ResetSimulationClock() = 0;

    virtual double GetSimulationTime() const = 0;

    virtual float GetSimulationTimeStep() const = 0;
    virtual void SetSimulationTimeStep(float step) = 0;

    virtual float GetDeltaTime() const = 0;
    virtual void SetDeltaTime(float delta) = 0;

    virtual float GetTimeFactor() const = 0;
    virtual void SetTimeFactor(float factor) = 0;

    virtual void GetGravity(VxVector &gravity) const = 0;
    virtual void SetGravity(const VxVector &gravity) = 0;

    PhysicsObject *GetPhysicsObject0(CK3dEntity *entity) {
        if (!entity)
            return nullptr;

        typedef XNHashTable<PhysicsObject, CK_ID> PhysicsObjectTable;
        auto *objs = reinterpret_cast<PhysicsObjectTable *>(reinterpret_cast<CKBYTE *>(this) + 0x2CD8);
        PhysicsObjectTable::Iterator it = objs->Find(entity->GetID());
        if (it == objs->End())
            return nullptr;

        return &*it;
    }
};

extern void InitPhysicsMethodPointers();

#endif // BML_PHYSICS_RT_H
