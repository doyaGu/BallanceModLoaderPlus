#include "physics_RT.h"
#include "HookUtils.h"

#include <cassert>
#include <stdexcept>

// ============================================================================
// METHOD POINTER MACROS
// ============================================================================

#define CP_METHOD_TYPE_NAME(Class, Name) \
    Class##_##Name##Func

#define CP_METHOD_PTR_NAME(Class, Name) \
    g_##Class##_##Name

#define CP_DECLARE_METHOD_PTR(Class, Ret, Name, Args) \
    typedef Ret(Class::*CP_METHOD_TYPE_NAME(Class, Name)) Args; \
    CP_METHOD_TYPE_NAME(Class, Name) CP_METHOD_PTR_NAME(Class, Name)

#define CP_CALL_METHOD_PTR(Ptr, Func, ...) \
    (Ptr->*Func)(__VA_ARGS__)

#define CP_LOAD_METHOD_PTR(Class, Name, Base, Offset) \
    CP_METHOD_PTR_NAME(Class, Name) = utils::ForceReinterpretCast<CP_METHOD_TYPE_NAME(Class, Name)>(Base, Offset)

// ============================================================================
// METHOD POINTER DECLARATIONS
// ============================================================================

CP_DECLARE_METHOD_PTR(IVP_U_Quat, void, set_quaternion, (const IVP_U_Matrix3 *mat));
CP_DECLARE_METHOD_PTR(IVP_Real_Object, void, ensure_in_simulation, ());
CP_DECLARE_METHOD_PTR(IVP_Real_Object, void, enable_collision_detection, (IVP_BOOL enable));
CP_DECLARE_METHOD_PTR(IVP_Real_Object, void, get_m_world_f_object_AT, (IVP_U_Matrix * m_world_f_object_out));
CP_DECLARE_METHOD_PTR(CKIpionManager, PhysicsObject *, get_physics_object_internal, (CK3dEntity *entity, int warn_if_missing));

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

static int *IVP_RAND_SEED = nullptr;
static float (*ivp_rand_ptr)() = nullptr;

static int *qh_rand_seed = nullptr;
static int (*qh_rand_ptr)() = nullptr;

static void *(*p_malloc_ptr)(unsigned int size);
static void (*p_free_ptr)(void *data);
static bool g_PhysicsAddressesInitialized = false;

// ============================================================================
// ADDRESS INITIALIZATION
// ============================================================================

void InitPhysicsAddresses() {
    if (g_PhysicsAddressesInitialized) {
        return;
    }

    void *base = utils::GetModuleBaseAddress("physics_RT.dll");
    if (!base) {
        throw std::runtime_error("physics_RT.dll is not loaded or base address unavailable");
    }

    // Load method pointers
    CP_LOAD_METHOD_PTR(IVP_U_Quat, set_quaternion, base, 0x191B0);
    CP_LOAD_METHOD_PTR(IVP_Real_Object, ensure_in_simulation, base, 0xA460);
    CP_LOAD_METHOD_PTR(IVP_Real_Object, enable_collision_detection, base, 0x9350);
    // Binary thunk that forwards to IVP_Real_Object::calc_at_matrix using the environment's current time.
    CP_LOAD_METHOD_PTR(IVP_Real_Object, get_m_world_f_object_AT, base, 0x9C40);
    // The binary method accepts an extra "warn if missing" flag that the wrapper suppresses.
    CP_LOAD_METHOD_PTR(CKIpionManager, get_physics_object_internal, base, 0x7800);

    // Load global variables
    IVP_RAND_SEED = utils::ForceReinterpretCast<decltype(IVP_RAND_SEED)>(base, 0x685B4);
    qh_rand_seed = utils::ForceReinterpretCast<decltype(qh_rand_seed)>(base, 0x70BF0);

    // Load function pointers
    ivp_rand_ptr = utils::ForceReinterpretCast<decltype(ivp_rand_ptr)>(base, 0x2FCD0);
    qh_rand_ptr = utils::ForceReinterpretCast<decltype(qh_rand_ptr)>(base, 0x52F50);

    p_malloc_ptr = utils::ForceReinterpretCast<decltype(p_malloc_ptr)>(base, 0x200C0);
    p_free_ptr = utils::ForceReinterpretCast<decltype(p_free_ptr)>(base, 0x60770);

    g_PhysicsAddressesInitialized = true;
}

// ============================================================================
// MEMORY MANAGEMENT FUNCTIONS AND MACROS
// ============================================================================

void *p_malloc(unsigned int size) {
    assert(p_malloc_ptr && "InitPhysicsAddresses() must be called before p_malloc");
    if (!p_malloc_ptr) {
        return nullptr;
    }
    return p_malloc_ptr(size);
}

void p_free(void *data) {
    if (p_free_ptr && data) {
        p_free_ptr(data);
    }
}

// ============================================================================
// RANDOM NUMBER FUNCTIONS
// ============================================================================

void ivp_srand(int seed) {
    assert(IVP_RAND_SEED && "InitPhysicsAddresses() must be called before ivp_srand");
    if (!IVP_RAND_SEED) {
        return;
    }
    if (seed == 0) seed = 1;
    *IVP_RAND_SEED = seed;
}

int ivp_srand_read() {
    assert(IVP_RAND_SEED && "InitPhysicsAddresses() must be called before ivp_srand_read");
    if (!IVP_RAND_SEED) {
        return 1;
    }
    return *IVP_RAND_SEED;
}

float ivp_rand() {
    assert(ivp_rand_ptr && "InitPhysicsAddresses() must be called before ivp_rand");
    if (!ivp_rand_ptr) {
        return 0.0f;
    }
    return ivp_rand_ptr();
}

void qh_srand(int seed) {
    assert(qh_rand_seed && "InitPhysicsAddresses() must be called before qh_srand");
    if (!qh_rand_seed) {
        return;
    }
    if (seed < 1)
        *qh_rand_seed = 1;
    else if (seed >= qh_rand_m)
        *qh_rand_seed = qh_rand_m - 1;
    else
        *qh_rand_seed = seed;
}

int qh_srand_read() {
    assert(qh_rand_seed && "InitPhysicsAddresses() must be called before qh_srand_read");
    if (!qh_rand_seed) {
        return 1;
    }
    return *qh_rand_seed;
}

int qh_rand() {
    assert(qh_rand_ptr && "InitPhysicsAddresses() must be called before qh_rand");
    if (!qh_rand_ptr) {
        return 1;
    }
    return qh_rand_ptr();
}

// ============================================================================
// IVP CLASS METHOD IMPLEMENTATIONS
// ============================================================================

void IVP_U_Vector_Base::increment_mem() {
    int i;
    void **new_elems = (void **) p_malloc(sizeof(void *) * 2 * (memsize + 1));
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
        P_FREE(elems);
    }
    elems = new_elems;
}

void IVP_U_Quat::set_quaternion(const IVP_U_Matrix3 *mat) {
    CP_CALL_METHOD_PTR(this, CP_METHOD_PTR_NAME(IVP_U_Quat, set_quaternion), mat);
}

void IVP_Real_Object::ensure_in_simulation() {
    CP_CALL_METHOD_PTR(this, CP_METHOD_PTR_NAME(IVP_Real_Object, ensure_in_simulation));
}

void IVP_Real_Object::enable_collision_detection(IVP_BOOL enable) {
    CP_CALL_METHOD_PTR(this, CP_METHOD_PTR_NAME(IVP_Real_Object, enable_collision_detection), enable);
}

void IVP_Real_Object::get_m_world_f_object_AT(IVP_U_Matrix *m_world_f_object_out) {
    CP_CALL_METHOD_PTR(this, CP_METHOD_PTR_NAME(IVP_Real_Object, get_m_world_f_object_AT), m_world_f_object_out);
}

inline IVP_BOOL IVP_Environment::must_perform_movement_check() {
    next_movement_check--;
    if (next_movement_check == 0) {
        next_movement_check = IVP_MOVEMENT_CHECK_COUNT * 3 / 2
            + (short) (ivp_rand() * (IVP_MOVEMENT_CHECK_COUNT / 2));
        return IVP_TRUE;
    } else {
        return IVP_FALSE;
    }
}

// ============================================================================
// VECTOR CONVERSION FUNCTIONS
// ============================================================================

inline void VxConvertVector(const IVP_U_Point &in, VxVector &out) {
    out.x = (float)in.k[0];
    out.y = (float)in.k[1];
    out.z = (float)in.k[2];
}

inline void VxConvertVector(const IVP_U_Float_Point &in, VxVector &out) {
    out.x = in.k[0];
    out.y = in.k[1];
    out.z = in.k[2];
}

inline void VxConvertVector(const VxVector &in, IVP_U_Point &out) {
    out.k[0] = in.x;
    out.k[1] = in.y;
    out.k[2] = in.z;
}

inline void VxConvertVector(const VxVector &in, IVP_U_Float_Point &out) {
    out.k[0] = in.x;
    out.k[1] = in.y;
    out.k[2] = in.z;
}

// ============================================================================
// QUATERNION CONVERSION FUNCTIONS
// ============================================================================

inline void VxConvertQuaternion(const IVP_U_Quat &in, VxQuaternion &out) {
    out.x = (float)in.x;
    out.y = (float)in.y;
    out.z = (float)in.z;
    out.w = (float)in.w;
}

inline void VxConvertQuaternion(const VxQuaternion &in, IVP_U_Quat &out) {
    out.x = in.x;
    out.y = in.y;
    out.z = in.z;
    out.w = in.w;
}

// ============================================================================
// MATRIX CONVERSION FUNCTIONS
// ============================================================================

inline void VxConvertMatrix(const IVP_U_Matrix &in, VxMatrix &out) {
    // Transpose
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            out[j][i] = (float)in.get_elem(i, j);
    out[0][3] = 0.0f;
    out[1][3] = 0.0f;
    out[2][3] = 0.0f;

    out[3][0] = (float)in.vv.k[0];
    out[3][1] = (float)in.vv.k[1];
    out[3][2] = (float)in.vv.k[2];
    out[3][3] = 1.0f;
}

inline void VxConvertMatrix(const VxMatrix &in, IVP_U_Matrix &out) {
    // Transpose
    out.set_elem(0, 0, in[0][0]);
    out.set_elem(0, 1, in[1][0]);
    out.set_elem(0, 2, in[2][0]);
    out.set_elem(1, 0, in[0][1]);
    out.set_elem(1, 1, in[1][1]);
    out.set_elem(1, 2, in[2][1]);
    out.set_elem(2, 0, in[0][2]);
    out.set_elem(2, 1, in[1][2]);
    out.set_elem(2, 2, in[2][2]);

    out.vv.k[0] = in[3][0];
    out.vv.k[1] = in[3][1];
    out.vv.k[2] = in[3][2];
}

inline void VxConvertMatrix(const VxMatrix &in, IVP_U_Matrix3 &out) {
    // Transpose
    out.set_elem(0, 0, in[0][0]);
    out.set_elem(0, 1, in[1][0]);
    out.set_elem(0, 2, in[2][0]);
    out.set_elem(1, 0, in[0][1]);
    out.set_elem(1, 1, in[1][1]);
    out.set_elem(1, 2, in[2][1]);
    out.set_elem(2, 0, in[0][2]);
    out.set_elem(2, 1, in[1][2]);
    out.set_elem(2, 2, in[2][2]);
}

// ============================================================================
// PHYSICS OBJECT IMPLEMENTATIONS
// ============================================================================

void PhysicsObject::Wake() {
    if (m_RealObject) {
        m_RealObject->ensure_in_simulation();
    }
}

void PhysicsObject::EnableCollisions(bool enable) {
    if (!m_RealObject) {
        return;
    }

    if (enable) {
        m_RealObject->enable_collision_detection(IVP_TRUE);
    } else {
        m_RealObject->enable_collision_detection(IVP_FALSE);
    }
}

float PhysicsObject::GetMass() const {
    return m_RealObject ? m_RealObject->get_core()->get_mass() : 0.0f;
}

float PhysicsObject::GetInvMass() const {
    return m_RealObject ? m_RealObject->get_core()->get_inv_mass() : 0.0f;
}

void PhysicsObject::GetInertia(VxVector &inertia) const {
    inertia.Set(0.0f, 0.0f, 0.0f);
    if (!m_RealObject) {
        return;
    }

    const IVP_U_Float_Point *pRI = m_RealObject->get_core()->get_rot_inertia();
    VxConvertVector(*pRI, inertia);
}

void PhysicsObject::GetInvInertia(VxVector &inertia) const {
    inertia.Set(0.0f, 0.0f, 0.0f);
    if (!m_RealObject) {
        return;
    }

    const IVP_U_Float_Point *pRI = m_RealObject->get_core()->get_inv_rot_inertia();
    VxConvertVector(*pRI, inertia);
}

void PhysicsObject::GetDamping(float *speed, float *rot) {
    if (!m_RealObject) {
        if (speed) {
            *speed = 0.0f;
        }
        if (rot) {
            *rot = 0.0f;
        }
        return;
    }

    IVP_Core *core = m_RealObject->get_core();
    if (speed) {
        *speed = core->speed_damp_factor;
    }
    if (rot) {
        *rot = core->rot_speed_damp_factor.k[0];
    }
}

void PhysicsObject::GetPosition(VxVector *worldPosition, VxVector *angles) {
    if (!m_RealObject) {
        if (worldPosition) {
            worldPosition->Set(0.0f, 0.0f, 0.0f);
        }
        if (angles) {
            angles->Set(0.0f, 0.0f, 0.0f);
        }
        return;
    }

    IVP_U_Matrix matrix;
    m_RealObject->get_m_world_f_object_AT(&matrix);

    if (worldPosition) {
        IVP_U_Point *pt = matrix.get_position();
        worldPosition->Set((float) pt->k[0], (float) pt->k[1], (float) pt->k[2]);
    }

    if (angles) {
        VxMatrix mat;
        VxConvertMatrix(matrix, mat);

        VxQuaternion quat;
        quat.FromMatrix(mat);
        quat.ToEulerAngles(&angles->x, &angles->y, &angles->z);
    }
}

void PhysicsObject::GetPositionMatrix(VxMatrix &positionMatrix) {
    if (!m_RealObject) {
        positionMatrix.Identity();
        return;
    }

    IVP_U_Matrix matrix;
    m_RealObject->get_m_world_f_object_AT(&matrix);
    VxConvertMatrix(matrix, positionMatrix);
}

void PhysicsObject::GetVelocity(VxVector *velocity, VxVector *angularVelocity) {
    if (!velocity && !angularVelocity)
        return;
    if (!m_RealObject) {
        if (velocity) {
            velocity->Set(0.0f, 0.0f, 0.0f);
        }
        if (angularVelocity) {
            angularVelocity->Set(0.0f, 0.0f, 0.0f);
        }
        return;
    }

    IVP_Core *core = m_RealObject->get_core();

    if (velocity) {
        float x = core->speed.k[0] + core->speed_change.k[0];
        float y = core->speed.k[1] + core->speed_change.k[1];
        float z = core->speed.k[2] + core->speed_change.k[2];
        velocity->Set(x, y, z);
    }

    if (angularVelocity) {
        float x = core->rot_speed.k[0] + core->rot_speed_change.k[0];
        float y = core->rot_speed.k[1] + core->rot_speed_change.k[1];
        float z = core->rot_speed.k[2] + core->rot_speed_change.k[2];
        angularVelocity->Set(x, y, z);
    }
}

void PhysicsObject::SetVelocity(const VxVector *velocity, const VxVector *angularVelocity) {
    if (!m_RealObject) {
        return;
    }

    IVP_Core *core = m_RealObject->get_core();

    if (velocity) {
        core->speed_change.k[0] = velocity->x;
        core->speed_change.k[1] = velocity->y;
        core->speed_change.k[2] = velocity->z;
        core->speed.k[0] = 0.0f;
        core->speed.k[1] = 0.0f;
        core->speed.k[2] = 0.0f;
    }

    if (angularVelocity) {
        core->rot_speed_change.k[0] = angularVelocity->x;
        core->rot_speed_change.k[1] = angularVelocity->y;
        core->rot_speed_change.k[2] = angularVelocity->z;
        core->rot_speed.k[0] = 0.0f;
        core->rot_speed.k[1] = 0.0f;
        core->rot_speed.k[2] = 0.0f;
    }
}

bool PhysicsObject::IsStatic() const {
    return m_RealObject && m_RealObject->get_core()->physical_unmoveable;
}

// ============================================================================
// IPION MANAGER IMPLEMENTATIONS
// ============================================================================

PhysicsObject *CKIpionManager::GetPhysicsObject(CK3dEntity *entity) {
    if (!entity)
        return nullptr;
    return CP_CALL_METHOD_PTR(this, CP_METHOD_PTR_NAME(CKIpionManager, get_physics_object_internal), entity, 0);
}
