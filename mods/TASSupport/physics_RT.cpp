#include "physics_RT.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <Psapi.h>

#define CP_METHOD_TYPE_NAME(Class, Name) \
    Class##_##Name##Func

#define CP_METHOD_PTR_NAME(Class, Name) \
    g_##Class##_##Name

#define CP_DECLARE_METHOD_PTR(Class, Ret, Name, Args) \
    typedef Ret(Class::*CP_METHOD_TYPE_NAME(Class, Name)) Args; \
    CP_METHOD_TYPE_NAME(Class, Name) CP_METHOD_PTR_NAME(Class, Name)

#define CP_CALL_METHOD_PTR(Ptr, Func, ...) \
    (Ptr->*Func)(__VA_ARGS__)

template<typename T>
inline T ForceReinterpretCast(void *base, size_t offset) {
    void *p = reinterpret_cast<char *>(base) + offset;
    return *reinterpret_cast<T *>(&p);
}

CP_DECLARE_METHOD_PTR(IVP_U_Quat, void, set_quaternion, (const IVP_U_Matrix3 *mat));
CP_DECLARE_METHOD_PTR(IVP_Real_Object, void, ensure_in_simulation, ());
CP_DECLARE_METHOD_PTR(IVP_Real_Object, void, enable_collision_detection, (IVP_BOOL enable));
CP_DECLARE_METHOD_PTR(IVP_Real_Object, void, get_m_world_f_object_AT, (IVP_U_Matrix * m_world_f_object_out));

void InitPhysicsMethodPointers() {
    HMODULE hModule = ::GetModuleHandleA("physics_RT.dll");

    MODULEINFO moduleInfo;
    ::GetModuleInformation(::GetCurrentProcess(), hModule, &moduleInfo, sizeof(moduleInfo));

    void *base = moduleInfo.lpBaseOfDll;

#define CP_LOAD_METHOD_PTR(Class, Name, Base, Offset) \
    CP_METHOD_PTR_NAME(Class, Name) = ForceReinterpretCast<CP_METHOD_TYPE_NAME(Class, Name)>(Base, Offset)

    CP_LOAD_METHOD_PTR(IVP_U_Quat, set_quaternion, base, 0x191B0);
    CP_LOAD_METHOD_PTR(IVP_Real_Object, ensure_in_simulation, base, 0xA460);
    CP_LOAD_METHOD_PTR(IVP_Real_Object, enable_collision_detection, base, 0x9350);
    CP_LOAD_METHOD_PTR(IVP_Real_Object, get_m_world_f_object_AT, base, 0x9C40);

#undef CP_LOAD_METHOD_PTR
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

inline void VxConvertVector(const IVP_U_Point &in, VxVector &out)
{
    out.x = (float)in.k[0];
    out.y = (float)in.k[1];
    out.z = (float)in.k[2];
}

inline void VxConvertVector(const IVP_U_Float_Point &in, VxVector &out)
{
    out.x = in.k[0];
    out.y = in.k[1];
    out.z = in.k[2];
}

inline void VxConvertVector(const VxVector &in, IVP_U_Point &out)
{
    out.k[0] = in.x;
    out.k[1] = in.y;
    out.k[2] = in.z;
}

inline void VxConvertVector(const VxVector &in, IVP_U_Float_Point &out)
{
    out.k[0] = in.x;
    out.k[1] = in.y;
    out.k[2] = in.z;
}

inline void VxConvertQuaternion(const IVP_U_Quat &in, VxQuaternion &out)
{
    out.x = (float)in.x;
    out.y = (float)in.y;
    out.z = (float)in.z;
    out.w = (float)in.w;
}

inline void VxConvertQuaternion(const VxQuaternion &in, IVP_U_Quat &out)
{
    out.x = in.x;
    out.y = in.y;
    out.z = in.z;
    out.w = in.w;
}

inline void VxConvertMatrix(const IVP_U_Matrix &in, VxMatrix &out)
{
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

inline void VxConvertMatrix(const VxMatrix &in, IVP_U_Matrix &out)
{
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

inline void VxConvertMatrix(const VxMatrix &in, IVP_U_Matrix3 &out)
{
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

void PhysicsObject::Wake() {
    m_RealObject->ensure_in_simulation();
}

void PhysicsObject::EnableCollisions(bool enable) {
    if (enable) {
        m_RealObject->enable_collision_detection(IVP_TRUE);
    } else {
        m_RealObject->enable_collision_detection(IVP_FALSE);
    }
}

float PhysicsObject::GetMass() const {
    return m_RealObject->get_core()->get_mass();
}

float PhysicsObject::GetInvMass() const {
    return m_RealObject->get_core()->get_inv_mass();
}

void PhysicsObject::GetInertia(VxVector &inertia) const {
    const IVP_U_Float_Point *pRI = m_RealObject->get_core()->get_rot_inertia();
    VxConvertVector(*pRI, inertia);
}

void PhysicsObject::GetInvInertia(VxVector &inertia) const {
    const IVP_U_Float_Point *pRI = m_RealObject->get_core()->get_inv_rot_inertia();
    VxConvertVector(*pRI, inertia);
}

void PhysicsObject::GetDamping(float *speed, float *rot) {
    IVP_Core *core = m_RealObject->get_core();
    if (speed) {
        *speed = core->speed_damp_factor;
    }
    if (rot) {
        *rot = core->rot_speed_damp_factor.k[0];
    }
}

void PhysicsObject::GetPosition(VxVector *worldPosition, VxVector *angles) {
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
    IVP_U_Matrix matrix;
    m_RealObject->get_m_world_f_object_AT(&matrix);
    VxConvertMatrix(matrix, positionMatrix);
}

void PhysicsObject::GetVelocity(VxVector *velocity, VxVector *angularVelocity) {
    if (!velocity && !angularVelocity)
        return;

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
    if (m_RealObject->get_core()->physical_unmoveable)
        return true;
    return false;
}