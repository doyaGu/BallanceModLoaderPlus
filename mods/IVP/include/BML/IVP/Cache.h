#ifndef BML_IVP_CACHE_H
#define BML_IVP_CACHE_H

#include "BML/IVP/Environment.h"

#include <cstddef>
#include <cstdint>

class IVP_Cache_Object {
    friend class IVP_Real_Object;
    friend class IVP_Cache_Object_Manager;
    friend class IVP_U_Matrix_Cache;

private:
    IVP_Time_CODE valid_until_time_code;
    std::int32_t reference_count;
    IVP_Real_Object *object;
    std::uint32_t reserved_0C;

public:
    IVP_U_Quat q_world_f_object;
    IVP_U_Matrix m_world_f_object;
    IVP_U_Point core_pos;

    void remove_reference() {
        --reference_count;
    }
    void update_cache_object() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CacheObjectUpdate, this);
    }
    void transform_position_to_object_coords(const IVP_U_Point *input,
                                             IVP_U_Point *output) const {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CacheTransformPositionToObject,
            const_cast<IVP_Cache_Object *>(this), input, output);
    }
    void transform_position_to_object_coords(
        const IVP_U_Point *input, IVP_U_Float_Point *output) const {
        m_world_f_object.inline_vimult4(input, output);
    }
    void transform_position_to_world_coords(const IVP_U_Float_Point *input,
                                            IVP_U_Point *output) const {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CacheTransformPositionToWorld,
            const_cast<IVP_Cache_Object *>(this), input, output);
    }
    void transform_vector_to_object_coords(const IVP_U_Point *input,
                                           IVP_U_Point *output) const {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CacheTransformVectorToObject,
            const_cast<IVP_Cache_Object *>(this), input, output);
    }
    void transform_vector_to_object_coords(const IVP_U_Float_Point *input,
                                           IVP_U_Float_Point *output) const {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CacheTransformFloatVectorToObject,
            const_cast<IVP_Cache_Object *>(this), input, output);
    }
    void transform_vector_to_world_coords(const IVP_U_Point *input,
                                          IVP_U_Point *output) const {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CacheTransformVectorToWorld,
            const_cast<IVP_Cache_Object *>(this), input, output);
    }
    void transform_vector_to_world_coords(const IVP_U_Float_Point *input,
                                          IVP_U_Float_Point *output) const {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CacheTransformFloatVectorToWorld,
            const_cast<IVP_Cache_Object *>(this), input, output);
    }
};

class IVP_Cache_Object_Manager {
    friend struct BML_IvpCacheObjectManagerLayoutCheck;

private:
    std::int32_t n_cache_objects;
    std::int32_t reuse_loop_index;
    char *cache_objects_buffer;

public:
    explicit IVP_Cache_Object_Manager(int numberOfCacheElements) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CacheManagerConstruct,
            this, numberOfCacheElements);
    }
    ~IVP_Cache_Object_Manager() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CacheManagerDestruct, this);
    }

    IVP_Cache_Object *cache_object_at(int index) {
        constexpr int stride = (sizeof(IVP_Cache_Object) + 0xF) & ~0xF;
        return reinterpret_cast<IVP_Cache_Object *>(
            cache_objects_buffer + stride * index);
    }
    IVP_Cache_Object *get_cache_object(IVP_Real_Object *value) {
        return BML::IVP::ABI::InvokeThis<IVP_Cache_Object *>(
            BML::IVP::ABI::Address::CacheManagerGetObject,
            this, value);
    }
    static void invalid_cache_object(IVP_Real_Object *object) {
        BML::IVP::ABI::Invoke<void>(
            BML::IVP::ABI::Address::CacheManagerInvalidObject, object);
    }
};

// Collision event prediction samples at most twenty transforms between two
// PSI boundaries. Ballance retains the private initializer in physics_RT.dll,
// while the lookup and refresh methods are compiler-inlined at every caller.
inline constexpr int IVP_3D_SOLVER_MAX_STEPS_PER_PSI = 20;

class alignas(8) IVP_U_Matrix_Cache {
public:
    IVP_Real_Object *object;
    IVP_Core *core;
    IVP_Time base_time;
    IVP_Time_CODE base_time_code;
    IVP_U_Matrix *m_world_f_object[
        IVP_3D_SOLVER_MAX_STEPS_PER_PSI + 1];
    IVP_U_Matrix matrizes[IVP_3D_SOLVER_MAX_STEPS_PER_PSI + 1];

    explicit IVP_U_Matrix_Cache(IVP_Cache_Object *cacheObject) {
        p_init(cacheObject);
    }

    IVP_U_Matrix *calc_matrix_at(IVP_Time time, int index) {
        IVP_U_Matrix *&matrix = m_world_f_object[index];
        if (!matrix) {
            matrix = &matrizes[index];
            object->calc_at_matrix(time, matrix);
        }
        return matrix;
    }

    IVP_U_Matrix *calc_matrix_at_now(IVP_Time, int) {
        return m_world_f_object[0];
    }

    void calc_calc_matrix_cache(IVP_Cache_Object *cacheObject) {
        if (base_time_code != cacheObject->valid_until_time_code)
            p_init(cacheObject);
    }

private:
    void p_init(IVP_Cache_Object *cacheObject) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MatrixCacheInitialize,
            this, cacheObject);
    }
};

struct BML_IvpCacheObjectManagerLayoutCheck {
    static constexpr std::size_t count =
        offsetof(IVP_Cache_Object_Manager, n_cache_objects);
    static constexpr std::size_t reuse =
        offsetof(IVP_Cache_Object_Manager, reuse_loop_index);
    static constexpr std::size_t buffer =
        offsetof(IVP_Cache_Object_Manager, cache_objects_buffer);
};

inline IVP_Cache_Object *IVP_Real_Object::get_cache_object_no_lock() {
    if (!cache_object) {
        IVP_Environment *env = get_environment();
        cache_object = env->get_cache_object_manager()->get_cache_object(this);
    }
    if (get_movement_state() < IVP_MT_NOT_SIM) {
        IVP_Environment *env = get_environment();
        if (env->get_current_time_code() >
            cache_object->valid_until_time_code) {
            cache_object->update_cache_object();
        }
    }
    return cache_object;
}

inline IVP_Cache_Object *IVP_Real_Object::get_cache_object() {
    if (!cache_object) {
        IVP_Environment *env = get_environment();
        cache_object = env->get_cache_object_manager()->get_cache_object(this);
    }
    ++cache_object->reference_count;
    if (get_movement_state() < IVP_MT_NOT_SIM) {
        IVP_Environment *env = get_environment();
        if (env->get_current_time_code() >
            cache_object->valid_until_time_code) {
            cache_object->update_cache_object();
        }
    }
    return cache_object;
}

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_Cache_Object) == 0xD0);
static_assert(offsetof(IVP_Cache_Object, q_world_f_object) == 0x10);
static_assert(offsetof(IVP_Cache_Object, m_world_f_object) == 0x30);
static_assert(offsetof(IVP_Cache_Object, core_pos) == 0xB0);
static_assert(sizeof(IVP_Cache_Object_Manager) == 0x0C);
static_assert(BML_IvpCacheObjectManagerLayoutCheck::count == 0x00);
static_assert(BML_IvpCacheObjectManagerLayoutCheck::reuse == 0x04);
static_assert(BML_IvpCacheObjectManagerLayoutCheck::buffer == 0x08);
static_assert(sizeof(IVP_U_Matrix_Cache) == 0xAE8);
static_assert(alignof(IVP_U_Matrix_Cache) == 0x08);
static_assert(offsetof(IVP_U_Matrix_Cache, object) == 0x00);
static_assert(offsetof(IVP_U_Matrix_Cache, core) == 0x04);
static_assert(offsetof(IVP_U_Matrix_Cache, base_time) == 0x08);
static_assert(offsetof(IVP_U_Matrix_Cache, base_time_code) == 0x10);
static_assert(offsetof(IVP_U_Matrix_Cache, m_world_f_object) == 0x14);
static_assert(offsetof(IVP_U_Matrix_Cache, matrizes) == 0x68);
#endif

#endif // BML_IVP_CACHE_H
