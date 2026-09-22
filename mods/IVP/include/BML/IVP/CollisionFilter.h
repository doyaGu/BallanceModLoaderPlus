#ifndef BML_IVP_COLLISION_FILTER_H
#define BML_IVP_COLLISION_FILTER_H

#include "BML/IVP/Object.h"

#include <cstddef>
#include <cstring>
#include <cstdint>

// The retail table has exactly three slots in this order. A Mod may derive
// from this interface and install the instance in IVP_Application_Environment
// before creating the environment.
class IVP_Collision_Filter {
public:
    BML_IVP_RETAIL_ALLOCATED_OBJECT;

    virtual IVP_BOOL check_objects_for_collision_detection(
        IVP_Real_Object *object0, IVP_Real_Object *object1) = 0;
    virtual void environment_will_be_deleted(IVP_Environment *environment) = 0;
    virtual ~IVP_Collision_Filter() {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::CollisionFilterDestruct,
            this, [] {});
    }
};

// Construction stays in the retail DLL, which installs its verified vtable.
// The method bodies remain as header-side fallbacks for derived Mod types.
class IVP_Collision_Filter_Coll_Group_Ident : public IVP_Collision_Filter {
public:
    explicit IVP_Collision_Filter_Coll_Group_Ident(
        IVP_BOOL deleteOnEnvironmentDelete) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CollisionFilterGroupIdentConstruct,
            this, deleteOnEnvironmentDelete);
    }
    ~IVP_Collision_Filter_Coll_Group_Ident() override = default;

    IVP_BOOL check_objects_for_collision_detection(
        IVP_Real_Object *object0, IVP_Real_Object *object1) override {
        return BML::IVP::ABI::InvokeThisOr<IVP_BOOL>(
            BML::IVP::ABI::Address::CollisionFilterGroupIdentCheck, this,
            [object0, object1]() {
                if (!object0 || !object1)
                    return IVP_TRUE;
                const char *ident0 = object0->nocoll_group_ident;
                const char *ident1 = object1->nocoll_group_ident;
                return ident0[0] == '\0' || ident1[0] == '\0' ||
                               std::strncmp(
                                   ident0, ident1,
                                   sizeof(object0->nocoll_group_ident)) != 0
                           ? IVP_TRUE
                           : IVP_FALSE;
            },
            object0, object1);
    }

    void environment_will_be_deleted(IVP_Environment *environment) override {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::CollisionFilterGroupIdentEnvironmentDelete,
            this,
            [this]() {
                if (delete_on_env_delete)
                    delete this;
            },
            environment);
    }

private:
    IVP_BOOL delete_on_env_delete;
};

class IVP_CFEP_Hash;

struct IVP_CFEP_Objectpair {
    IVP_Real_Object *object0;
    IVP_Real_Object *object1;
};

class IVP_Collision_Filter_Exclusive_Pair : public IVP_Collision_Filter {
public:
    IVP_Collision_Filter_Exclusive_Pair() {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::CollisionFilterExclusivePairConstruct,
            this, [this] { hash_table = nullptr; });
    }
    ~IVP_Collision_Filter_Exclusive_Pair() override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CollisionFilterExclusivePairDestruct,
            this);
    }

    void disable_collision_between_objects(
        IVP_Real_Object *object0, IVP_Real_Object *object1) {
        IVP_CFEP_Objectpair pair = make_pair(object0, object1);
        if (find_pair(&pair))
            return;
        auto *stored = static_cast<IVP_CFEP_Objectpair *>(
            BML::IVP::ABI::Invoke<void *>(
                BML::IVP::ABI::Address::OperatorNew,
                static_cast<unsigned int>(sizeof(pair))));
        if (!stored)
            return;
        *stored = pair;
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::VHashAdd,
            hash_table, stored, pair_hash(stored));
    }

    void enable_collision_between_objects(
        IVP_Real_Object *object0, IVP_Real_Object *object1) {
        IVP_CFEP_Objectpair pair = make_pair(object0, object1);
        if (!find_pair(&pair))
            return;
        void *removed = BML::IVP::ABI::InvokeThis<void *>(
            BML::IVP::ABI::Address::VHashRemove,
            hash_table, &pair, pair_hash(&pair));
        if (removed)
            BML::IVP::ABI::Invoke<void>(
                BML::IVP::ABI::Address::OperatorDelete, removed);
    }

    IVP_BOOL check_objects_for_collision_detection(
        IVP_Real_Object *object0, IVP_Real_Object *object1) override {
        return BML::IVP::ABI::InvokeThisOr<IVP_BOOL>(
            BML::IVP::ABI::Address::CollisionFilterExclusivePairCheck,
            this, [] { return IVP_TRUE; }, object0, object1);
    }

    void environment_will_be_deleted(IVP_Environment *environment) override {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::CollisionFilterExclusivePairEnvironmentDelete,
            this, [this]() { delete this; }, environment);
    }

private:
    static IVP_CFEP_Objectpair make_pair(
        IVP_Real_Object *object0, IVP_Real_Object *object1) {
        return reinterpret_cast<std::uintptr_t>(object1) >
                       reinterpret_cast<std::uintptr_t>(object0)
                   ? IVP_CFEP_Objectpair{object0, object1}
                   : IVP_CFEP_Objectpair{object1, object0};
    }
    static unsigned int pair_hash(const IVP_CFEP_Objectpair *pair) {
        return BML::IVP::ABI::InvokeStdcall<unsigned int>(
            BML::IVP::ABI::Address::CollisionFilterExclusivePairHash,
            reinterpret_cast<const char *>(pair));
    }
    void *find_pair(const IVP_CFEP_Objectpair *pair) const {
        return BML::IVP::ABI::InvokeThis<void *>(
            BML::IVP::ABI::Address::VHashFind, hash_table,
            pair, pair_hash(pair));
    }

    IVP_CFEP_Hash *hash_table;
};

// The constructor and add operation use their retail entries. Remaining short
// mutations are reconstructed over the verified 0x08 vector layout.
class IVP_Meta_Collision_Filter : public IVP_Collision_Filter {
public:
    explicit IVP_Meta_Collision_Filter(IVP_BOOL deleteOnEnvironmentDelete) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::CollisionFilterMetaConstruct,
            this,
            [this, deleteOnEnvironmentDelete] {
                delete_on_env_delete = deleteOnEnvironmentDelete;
                ::new (static_cast<void *>(&filter_set))
                    IVP_U_Vector<IVP_Collision_Filter>();
            },
            deleteOnEnvironmentDelete);
    }
    ~IVP_Meta_Collision_Filter() override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CollisionFilterMetaDestruct, this);
    }

    void add_collision_filter(IVP_Collision_Filter *filter) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::CollisionFilterMetaAdd, this, filter);
    }
    void remove_collision_filter(IVP_Collision_Filter *filter) {
        const int index = filter_set.index_of(filter);
        if (index >= 0)
            filter_set.remove_at(index);
    }

    IVP_BOOL check_objects_for_collision_detection(
        IVP_Real_Object *object0, IVP_Real_Object *object1) override {
        return BML::IVP::ABI::InvokeThisOr<IVP_BOOL>(
            BML::IVP::ABI::Address::CollisionFilterMetaCheck, this,
            [this, object0, object1]() {
                int doCollision = IVP_TRUE;
                for (int index = filter_set.len() - 1; index >= 0; --index) {
                    doCollision &= filter_set.element_at(index)
                                       ->check_objects_for_collision_detection(
                                           object0, object1);
                }
                return static_cast<IVP_BOOL>(doCollision);
            },
            object0, object1);
    }

    void environment_will_be_deleted(IVP_Environment *environment) override {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::CollisionFilterMetaEnvironmentDelete,
            this,
            [this, environment]() {
                for (int index = filter_set.len() - 1; index >= 0; --index) {
                    filter_set.element_at(index)->environment_will_be_deleted(
                        environment);
                    filter_set.remove_at(index);
                }
                if (delete_on_env_delete)
                    delete this;
            },
            environment);
    }

private:
    IVP_BOOL delete_on_env_delete;
    union {
        IVP_U_Vector<IVP_Collision_Filter> filter_set;
    };
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_Collision_Filter) == 0x04);
static_assert(sizeof(IVP_Collision_Filter_Coll_Group_Ident) == 0x08);
static_assert(sizeof(IVP_Collision_Filter_Exclusive_Pair) == 0x08);
static_assert(sizeof(IVP_Meta_Collision_Filter) == 0x10);
#endif

#endif // BML_IVP_COLLISION_FILTER_H
