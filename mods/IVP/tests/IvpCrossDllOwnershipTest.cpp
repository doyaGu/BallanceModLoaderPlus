#include "IvpTestAdapter.h"

#include "BML/IVP/IVP.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <new>
#include <type_traits>

namespace {

struct AllocationState {
    int Allocations = 0;
    int Deallocations = 0;
    bool FailNextAllocation = false;
} State;

bool ExpectRawStringHashStorage = false;
bool SawRawStringHashStorage = false;
bool ExpectRawMaterialSimpleFields = false;
bool SawRawMaterialSimpleFields = false;
bool ExpectRawMaterialManagerField = false;
bool SawRawMaterialManagerField = false;
bool ExpectRawExclusivePairField = false;
bool SawRawExclusivePairField = false;
bool ExpectRawBuoyancyAttacherFields = false;
bool SawRawBuoyancyAttacherFields = false;
bool ExpectRawOVElementFields = false;
bool SawRawOVElementFields = false;
int OVElementDestructorCalls = 0;
bool ExpectRawOVNodeFields = false;
bool SawRawOVNodeFields = false;
int OVNodeDestructorCalls = 0;
bool ExpectRawOVTreeManagerFields = false;
bool SawRawOVTreeManagerFields = false;
int OVTreeManagerDestructorCalls = 0;
bool ExpectRawSpringFields = false;
bool SawRawSpringFields = false;
bool ExpectRawActiveSpringFields = false;
bool SawRawActiveSpringFields = false;
bool ExpectRawMindistManagerFields = false;
bool SawRawMindistManagerFields = false;
int MindistManagerDestructorCalls = 0;

struct ExactBaseDestructorState {
    int BetterDebug = 0;
    int CollisionDelegator = 0;
    int CollisionFilter = 0;
    int Material = 0;
    int PerformanceCounter = 0;
    int SurfaceManager = 0;
    int Synapse = 0;
    int ActiveValue = 0;
} ExactBaseDestructors;

void *BML_CDECL RetailNew(unsigned int size) {
    if (State.FailNextAllocation) {
        State.FailNextAllocation = false;
        return nullptr;
    }
    ++State.Allocations;
    return std::malloc(size);
}

void BML_CDECL RetailDelete(void *memory) {
    if (!memory)
        return;
    ++State.Deallocations;
    std::free(memory);
}

void __fastcall ConstructNoArguments(void *, void *) {}
void __fastcall ConstructWithFlag(void *, void *, IVP_BOOL) {}
void __fastcall ConstructStringHash(
    IVP_U_String_Hash *hash, void *, int bucketCount,
    void *notFoundValue) {
    if (ExpectRawStringHashStorage) {
        const auto *bytes = reinterpret_cast<const std::byte *>(hash);
        SawRawStringHashStorage = std::all_of(
            bytes, bytes + sizeof(*hash), [](std::byte value) {
                return value == std::byte{0xA5};
            });
    }
    hash->size = bucketCount;
    hash->not_found_value = notFoundValue;
    hash->elems = nullptr;
}
void __fastcall ConstructMaterialManager(
    void *manager, void *, IVP_BOOL deleteOnEnvironmentDelete) {
    auto *bytes = reinterpret_cast<std::byte *>(manager);
    if (ExpectRawMaterialManagerField) {
        SawRawMaterialManagerField = std::all_of(
            bytes + 0x04, bytes + 0x08, [](std::byte value) {
                return value == std::byte{0xA5};
            });
    }
    std::memcpy(bytes + 0x04, &deleteOnEnvironmentDelete,
                sizeof(deleteOnEnvironmentDelete));
}
void __fastcall ConstructExclusivePair(void *filter, void *) {
    auto *bytes = reinterpret_cast<std::byte *>(filter);
    if (ExpectRawExclusivePairField) {
        SawRawExclusivePairField = std::all_of(
            bytes + 0x04, bytes + 0x08, [](std::byte value) {
                return value == std::byte{0xA5};
            });
    }
    std::memset(bytes + 0x04, 0, sizeof(void *));
}
void __fastcall ConstructRange(
    void *, void *, IVP_Environment *, IVP_BOOL) {}
void __fastcall ConstructMaterial(
    void *material, void *, IVP_DOUBLE, IVP_DOUBLE) {
    if (ExpectRawMaterialSimpleFields) {
        const auto *bytes = reinterpret_cast<const std::byte *>(material);
        SawRawMaterialSimpleFields = std::all_of(
            bytes + 0x04, bytes + sizeof(IVP_Material_Simple),
            [](std::byte value) { return value == std::byte{0xA5}; });
    }
}
void __fastcall ConstructCore(
    void *, void *, IVP_Real_Object *, const IVP_U_Quat *,
    const IVP_U_Point *, IVP_BOOL, IVP_BOOL) {}
void __fastcall ConstructBuoyancyAttacher(
    void *attacher, void *, IVP_Template_Buoyancy *definition,
    IVP_U_Set_Active<IVP_Core> *cores,
    IVP_Liquid_Surface_Descriptor *surfaceDescriptor) {
    auto *bytes = reinterpret_cast<std::byte *>(attacher);
    if (ExpectRawBuoyancyAttacherFields) {
        SawRawBuoyancyAttacherFields = std::all_of(
            bytes + 0x04,
            bytes + sizeof(IVP_Attacher_To_Cores_Buoyancy),
            [](std::byte value) { return value == std::byte{0xA5}; });
    }

    // Reproduce enough of RVA 0x104C0 for the host-side destruction path:
    // the test has no cores, so a static empty hash has the same ownership
    // state without allocating from either CRT.
    ::new (static_cast<void *>(bytes + 0x04))
        IVP_VHash_Store(static_cast<IVP_VHash_Store_Elem *>(nullptr), 0);
    std::memcpy(bytes + 0x18, &cores, sizeof(cores));
    std::memcpy(bytes + 0x1C, definition, sizeof(*definition));
    std::memcpy(bytes + 0x68, &cores, sizeof(cores));
    std::memcpy(bytes + 0x6C, &surfaceDescriptor,
                sizeof(surfaceDescriptor));
}
void __fastcall ConstructOVElement(
    IVP_OV_Element *element, void *, IVP_Real_Object *object) {
    auto *bytes = reinterpret_cast<std::byte *>(element);
    if (ExpectRawOVElementFields) {
        SawRawOVElementFields = std::all_of(
            bytes + 0x04, bytes + sizeof(*element), [](std::byte value) {
                return value == std::byte{0xA5};
            });
    }
    element->node = nullptr;
    element->hull_manager = nullptr;
    element->center.set_to_zero();
    element->radius = -1.0f;
    element->real_object = object;
}
void __fastcall ConstructOVTreeManager(
    IVP_OV_Tree_Manager *manager, void *) {
    if (ExpectRawOVTreeManagerFields) {
        const auto *bytes = reinterpret_cast<const std::byte *>(manager);
        SawRawOVTreeManagerFields = std::all_of(
            bytes, bytes + sizeof(*manager), [](std::byte value) {
                return value == std::byte{0xA5};
            });
    }
}
void __fastcall ConstructOVNode(IVP_OV_Node *node, void *) {
    if (ExpectRawOVNodeFields) {
        const auto *bytes = reinterpret_cast<const std::byte *>(node);
        SawRawOVNodeFields = std::all_of(
            bytes, bytes + sizeof(*node), [](std::byte value) {
                return value == std::byte{0xA5};
            });
    }
}
void __fastcall ConstructSpring(
    IVP_Actuator_Spring *spring, void *, IVP_Environment *,
    IVP_Template_Spring *, IVP_ACTUATOR_TYPE) {
    auto *bytes = reinterpret_cast<std::byte *>(spring);
    if (ExpectRawSpringFields) {
        SawRawSpringFields = std::all_of(
            bytes + 0x04, bytes + sizeof(IVP_Actuator_Spring),
            [](std::byte value) { return value == std::byte{0xA5}; });
    }
    std::memset(bytes + 0x04, 0, sizeof(IVP_Actuator_Spring) - 0x04);
    ::new (static_cast<void *>(bytes + 0x04)) IVP_U_Vector<IVP_Core>();
    ::new (static_cast<void *>(bytes + 0x0C)) IVP_Anchor();
    ::new (static_cast<void *>(bytes + 0x3C)) IVP_Anchor();
    ::new (static_cast<void *>(bytes + 0x90))
        IVP_U_Vector<IVP_Listener_Spring>();
    const IVP_FLOAT unitFactor = 1.0f;
    std::memcpy(bytes + 0x78, &unitFactor, sizeof(unitFactor));
}
void __fastcall ConstructActiveSpring(
    IVP_Actuator_Spring_Active *spring, void *, IVP_Environment *environment,
    IVP_Template_Spring *definition) {
    auto *bytes = reinterpret_cast<std::byte *>(spring);
    if (ExpectRawActiveSpringFields) {
        const bool primaryTailUntouched = std::all_of(
            bytes + 0x04, bytes + 0x98,
            [](std::byte value) { return value == std::byte{0xA5}; });
        const bool activePointersUntouched = std::all_of(
            bytes + 0x9C, bytes + sizeof(IVP_Actuator_Spring_Active),
            [](std::byte value) { return value == std::byte{0xA5}; });
        SawRawActiveSpringFields =
            primaryTailUntouched && activePointersUntouched;
    }
    ConstructSpring(
        spring, nullptr, environment, definition,
        IVP_ACTUATOR_TYPE_SPRING);
    std::memset(bytes + 0x9C, 0, 0x10);
}
void __fastcall ConstructMindistManager(
    IVP_Mindist_Manager *manager, void *, IVP_Environment *environment) {
    auto *bytes = reinterpret_cast<std::byte *>(manager);
    if (ExpectRawMindistManagerFields) {
        SawRawMindistManagerFields = std::all_of(
            bytes, bytes + sizeof(*manager), [](std::byte value) {
                return value == std::byte{0xA5};
            });
    }
    std::memset(bytes, 0, sizeof(*manager));
    ::new (static_cast<void *>(&manager->wheel_look_ahead_mindists))
        IVP_U_Vector<IVP_Mindist>();
    manager->environment = environment;
}
void __fastcall Destruct(void *, void *) {}
void __fastcall DestructOVElement(void *, void *) {
    ++OVElementDestructorCalls;
}
void __fastcall DestructOVNode(void *, void *) {
    ++OVNodeDestructorCalls;
}
void __fastcall DestructOVTreeManager(void *, void *) {
    ++OVTreeManagerDestructorCalls;
}
void __fastcall DestructMindistManager(
    IVP_Mindist_Manager *manager, void *) {
    ++MindistManagerDestructorCalls;
    manager->wheel_look_ahead_mindists.~IVP_U_Vector<IVP_Mindist>();
}
void __fastcall DestructBetterDebug(void *, void *) {
    ++ExactBaseDestructors.BetterDebug;
}
void __fastcall DestructCollisionDelegator(void *, void *) {
    ++ExactBaseDestructors.CollisionDelegator;
}
void __fastcall DestructCollisionFilter(void *, void *) {
    ++ExactBaseDestructors.CollisionFilter;
}
void __fastcall DestructMaterial(void *, void *) {
    ++ExactBaseDestructors.Material;
}
void __fastcall DestructPerformanceCounter(void *, void *) {
    ++ExactBaseDestructors.PerformanceCounter;
}
void __fastcall DestructSurfaceManager(void *, void *) {
    ++ExactBaseDestructors.SurfaceManager;
}
void __fastcall DestructSynapse(void *, void *) {
    ++ExactBaseDestructors.Synapse;
}
void __fastcall DestructActiveValue(void *, void *) {
    ++ExactBaseDestructors.ActiveValue;
}

class ModCollisionDelegator final : public IVP_Collision_Delegator {
public:
    void collision_is_going_to_be_deleted_event(IVP_Collision *) override {}
};

class ModCollisionFilter final : public IVP_Collision_Filter {
public:
    IVP_BOOL check_objects_for_collision_detection(
        IVP_Real_Object *, IVP_Real_Object *) override {
        return IVP_TRUE;
    }
    void environment_will_be_deleted(IVP_Environment *) override {}
};

class ModMaterial final : public IVP_Material {
public:
    IVP_DOUBLE get_friction_factor() override { return 0.0; }
    IVP_DOUBLE get_second_friction_factor() override { return 0.0; }
    IVP_DOUBLE get_elasticity() override { return 0.0; }
    IVP_DOUBLE get_adhesion() override { return 0.0; }
    const char *get_name() override { return "Mod material"; }
};

class ModPerformanceCounter final : public IVP_PerformanceCounter {
public:
    void start_pcount() override {}
    void pcount(IVP_PERFORMANCE_ELEMENT) override {}
    void stop_pcount() override {}
    void environment_is_going_to_be_deleted(IVP_Environment *) override {}
    void reset_and_print_performance_counters(IVP_Time) override {}
};

class ModSurfaceManager final : public IVP_SurfaceManager {
public:
    const IVP_Compact_Ledge *get_single_convex() const override {
        return nullptr;
    }
    void get_mass_center(IVP_U_Float_Point *) const override {}
    void get_radius_and_radius_dev_to_given_center(
        const IVP_U_Float_Point *, IVP_FLOAT *, IVP_FLOAT *) const override {}
    void get_rotation_inertia(IVP_U_Float_Point *) const override {}
    void get_all_ledges_within_radius(
        const IVP_U_Point *, IVP_DOUBLE, const IVP_Compact_Ledge *,
        IVP_Real_Object *, const IVP_Compact_Ledge *,
        IVP_U_BigVector<IVP_Compact_Ledge> *) override {}
    void get_all_terminal_ledges(
        IVP_U_BigVector<IVP_Compact_Ledge> *) override {}
    void insert_all_ledges_hitting_ray(
        IVP_Ray_Solver *, IVP_Real_Object *) override {}
    IVP_SURMAN_TYPE get_type() override { return IVP_SURMAN_POLYGON; }
};

class ModSynapse final : public IVP_Synapse {
public:
    ModSynapse() = default;
};

class ModActiveValue final : public IVP_U_Active_Value {
public:
    ModActiveValue() : IVP_U_Active_Value(nullptr) {}
};

class ModController final : public IVP_Controller {
public:
    IVP_U_Vector<IVP_Core> *get_associated_controlled_cores() override {
        return &cores;
    }
    void do_simulation_controller(
        IVP_Event_Sim *, IVP_U_Vector<IVP_Core> *) override {}
    IVP_CONTROLLER_PRIORITY get_controller_priority() override {
        return IVP_CP_ACTUATOR;
    }

private:
    IVP_U_Vector<IVP_Core> cores;
};

class ModForcefield final : public IVP_Forcefield {
public:
    ModForcefield(IVP_Environment *environment,
                  IVP_U_Set_Active<IVP_Core> *cores)
        : IVP_Forcefield(environment, cores, IVP_FALSE) {}

private:
    void do_simulation_controller(
        IVP_Event_Sim *, IVP_U_Vector<IVP_Core> *) override {}
};

class ModRaycastCar final : public IVP_Controller_Raycast_Car {
public:
    ModRaycastCar(IVP_Environment *environment,
                  const IVP_Template_Car_System *definition)
        : IVP_Controller_Raycast_Car(environment, definition) {}

private:
    void do_raycasts(IVP_Event_Sim *, int, IVP_Ray_Solver_Template *,
                     IVP_Ray_Hit *, IVP_FLOAT *) override {}
};

class ModSpring final : public IVP_Actuator_Spring {
public:
    ModSpring(
        IVP_Environment *environment, IVP_Template_Spring *definition)
        : IVP_Actuator_Spring(
              environment, definition, IVP_ACTUATOR_TYPE_SPRING) {}
};

class ModActiveSpring final : public IVP_Actuator_Spring_Active {
public:
    ModActiveSpring(
        IVP_Environment *environment, IVP_Template_Spring *definition)
        : IVP_Actuator_Spring_Active(environment, definition) {}
};

static_assert(std::is_same_v<
              decltype(new ModForcefield(nullptr, nullptr)),
              ModForcefield *>);
static_assert(std::is_same_v<
              decltype(new ModRaycastCar(nullptr, nullptr)),
              ModRaycastCar *>);

template <typename Object>
void ExerciseRetailAllocationPair() {
    void *memory = Object::operator new(sizeof(Object));
    ASSERT_NE(memory, nullptr);
    Object::operator delete(memory);
}

template <typename Object>
void ExerciseRetailDeallocation() {
    void *memory = RetailNew(static_cast<unsigned int>(sizeof(Object)));
    ASSERT_NE(memory, nullptr);
    Object::operator delete(memory);
}

} // namespace

extern "C" uintptr_t BML_IvpTestResolveRetailCall(
    std::uint32_t rva) noexcept {
    using BML::IVP::ABI::Address;
    const BML::IVP::Test::RetailCallBinding bindings[] = {
        BML::IVP::Test::Bind(Address::OperatorNew, &RetailNew),
        BML::IVP::Test::Bind(Address::OperatorDelete, &RetailDelete),
        BML::IVP::Test::Bind(
            Address::MaterialSimpleConstruct, &ConstructMaterial),
        BML::IVP::Test::Bind(
            Address::MaterialManagerConstruct, &ConstructMaterialManager),
        BML::IVP::Test::Bind(
            Address::AnomalyLimitsConstruct, &ConstructWithFlag),
        BML::IVP::Test::Bind(
            Address::AnomalyLimitsDestruct, &Destruct),
        BML::IVP::Test::Bind(
            Address::AnomalyManagerConstruct, &ConstructWithFlag),
        BML::IVP::Test::Bind(
            Address::AnomalyManagerDestruct, &Destruct),
        BML::IVP::Test::Bind(
            Address::RangeManagerConstruct, &ConstructRange),
        BML::IVP::Test::Bind(
            Address::PerformanceCounterSimpleConstruct,
            &ConstructNoArguments),
        BML::IVP::Test::Bind(
            Address::ActiveValueManagerConstruct, &ConstructWithFlag),
        BML::IVP::Test::Bind(
            Address::ActiveValueManagerDestruct, &Destruct),
        BML::IVP::Test::Bind(
            Address::CollisionFilterGroupIdentConstruct,
            &ConstructWithFlag),
        BML::IVP::Test::Bind(
            Address::CollisionFilterExclusivePairConstruct,
            &ConstructExclusivePair),
        BML::IVP::Test::Bind(
            Address::CollisionFilterExclusivePairDestruct, &Destruct),
        BML::IVP::Test::Bind(Address::StringHashConstruct,
                             &ConstructStringHash),
        BML::IVP::Test::Bind(Address::ControllerDestruct, &Destruct),
        BML::IVP::Test::Bind(Address::BetterDebugCtor,
                             &ConstructNoArguments),
        BML::IVP::Test::Bind(Address::CoreConstruct, &ConstructCore),
        BML::IVP::Test::Bind(Address::CoreDestruct, &Destruct),
        BML::IVP::Test::Bind(Address::BuoyancyAttacherConstruct,
                             &ConstructBuoyancyAttacher),
        BML::IVP::Test::Bind(Address::OVElementConstruct,
                             &ConstructOVElement),
        BML::IVP::Test::Bind(Address::OVElementDestruct,
                             &DestructOVElement),
        BML::IVP::Test::Bind(Address::OVNodeConstruct,
                             &ConstructOVNode),
        BML::IVP::Test::Bind(Address::OVNodeDestruct,
                             &DestructOVNode),
        BML::IVP::Test::Bind(Address::OVTreeManagerConstruct,
                             &ConstructOVTreeManager),
        BML::IVP::Test::Bind(Address::OVTreeManagerDestruct,
                             &DestructOVTreeManager),
        BML::IVP::Test::Bind(Address::ActuatorSpringConstruct,
                             &ConstructSpring),
        BML::IVP::Test::Bind(Address::ActuatorSpringActiveConstruct,
                             &ConstructActiveSpring),
        BML::IVP::Test::Bind(Address::MindistManagerConstruct,
                             &ConstructMindistManager),
        BML::IVP::Test::Bind(Address::MindistManagerDestruct,
                             &DestructMindistManager),
        BML::IVP::Test::Bind(Address::ActuatorDestruct, &Destruct),
        BML::IVP::Test::Bind(Address::BetterDebugDestruct,
                             &DestructBetterDebug),
        BML::IVP::Test::Bind(Address::CollisionDelegatorDestruct,
                             &DestructCollisionDelegator),
        BML::IVP::Test::Bind(Address::CollisionFilterDestruct,
                             &DestructCollisionFilter),
        BML::IVP::Test::Bind(Address::MaterialDestruct,
                             &DestructMaterial),
        BML::IVP::Test::Bind(Address::PerformanceCounterDestruct,
                             &DestructPerformanceCounter),
        BML::IVP::Test::Bind(Address::SurfaceManagerDestruct,
                             &DestructSurfaceManager),
        BML::IVP::Test::Bind(Address::SynapseDestruct,
                             &DestructSynapse),
        BML::IVP::Test::Bind(Address::ActiveValueDestruct,
                             &DestructActiveValue),
    };
    return BML::IVP::Test::Resolve(rva, bindings);
}

TEST(IvpCrossDllOwnership,
     ResourceOwningAndResourceFreeBaseLayersUseExactRetailDestructorsOnce) {
    ExactBaseDestructors = {};

    { IVP_BetterDebugmanager value; }
    { ModCollisionDelegator value; }
    { ModCollisionFilter value; }
    { ModMaterial value; }
    { ModPerformanceCounter value; }
    { ModSurfaceManager value; }
    { ModSynapse value; }
    { ModActiveValue value; }

    EXPECT_EQ(ExactBaseDestructors.BetterDebug, 1);
    EXPECT_EQ(ExactBaseDestructors.CollisionDelegator, 1);
    EXPECT_EQ(ExactBaseDestructors.CollisionFilter, 1);
    EXPECT_EQ(ExactBaseDestructors.Material, 1);
    EXPECT_EQ(ExactBaseDestructors.PerformanceCounter, 1);
    EXPECT_EQ(ExactBaseDestructors.SurfaceManager, 1);
    EXPECT_EQ(ExactBaseDestructors.Synapse, 1);
    EXPECT_EQ(ExactBaseDestructors.ActiveValue, 1);
}

TEST(IvpCrossDllOwnership, HeapConstructiblePolymorphicTypesUseRetailPair) {
    State = {};

    delete new IVP_Material_Simple(0.4, 0.2);
    delete new IVP_Material_Manager(IVP_FALSE);
    delete new IVP_Anomaly_Limits(IVP_FALSE);
    delete new IVP_Anomaly_Manager(IVP_FALSE);
    delete new IVP_Range_Manager(nullptr, IVP_FALSE);
    delete new IVP_PerformanceCounter_Simple();
    delete new IVP_U_Active_Value_Manager(IVP_FALSE);
    delete new IVP_Collision_Filter_Coll_Group_Ident(IVP_FALSE);
    delete new ModController();
    delete new IVP_BetterDebugmanager();

    // These two non-environment fixtures are still owned by retail code in a
    // real game: Core through Real Object teardown, and the buoyancy attacher
    // through its active-set listener. Stubbed complete ctors let this test
    // isolate the allocation/deallocation ABI rather than fake those systems.
    delete new IVP_Core(nullptr, nullptr, nullptr, IVP_FALSE, IVP_FALSE);
    IVP_Template_Buoyancy buoyancyDefinition;
    auto *attacher = new IVP_Attacher_To_Cores_Buoyancy(
        buoyancyDefinition, nullptr, nullptr);
    delete attacher;
    delete new IVP_Mindist_Manager(nullptr);

    EXPECT_EQ(State.Allocations, 13);
    EXPECT_EQ(State.Deallocations, State.Allocations);
}

TEST(IvpCrossDllOwnership,
     CompleteConstructorsReceiveUntouchedRetailOwnedFields) {
    alignas(IVP_Material_Simple)
        std::array<std::byte, sizeof(IVP_Material_Simple)> materialStorage;
    std::fill(materialStorage.begin(), materialStorage.end(),
              std::byte{0xA5});
    ExpectRawMaterialSimpleFields = true;
    SawRawMaterialSimpleFields = false;
    auto *material = ::new (materialStorage.data())
        IVP_Material_Simple(0.72, 0.18);
    EXPECT_TRUE(SawRawMaterialSimpleFields);
    material->~IVP_Material_Simple();
    ExpectRawMaterialSimpleFields = false;

    int notFound = 0;
    alignas(IVP_U_String_Hash)
        std::array<std::byte, sizeof(IVP_U_String_Hash)> hashStorage;
    std::fill(hashStorage.begin(), hashStorage.end(), std::byte{0xA5});
    ExpectRawStringHashStorage = true;
    SawRawStringHashStorage = false;
    auto *hash = ::new (hashStorage.data()) IVP_U_String_Hash(16, &notFound);
    EXPECT_TRUE(SawRawStringHashStorage);
    EXPECT_EQ(hash->size, 16);
    EXPECT_EQ(hash->not_found_value, &notFound);
    EXPECT_EQ(hash->elems, nullptr);
    hash->~IVP_U_String_Hash();
    ExpectRawStringHashStorage = false;

    alignas(IVP_Material_Manager)
        std::array<std::byte, sizeof(IVP_Material_Manager)> managerStorage;
    std::fill(managerStorage.begin(), managerStorage.end(), std::byte{0xA5});
    ExpectRawMaterialManagerField = true;
    SawRawMaterialManagerField = false;
    auto *manager =
        ::new (managerStorage.data()) IVP_Material_Manager(IVP_TRUE);
    EXPECT_TRUE(SawRawMaterialManagerField);
    IVP_BOOL deleteOnEnvironmentDelete = IVP_FALSE;
    std::memcpy(&deleteOnEnvironmentDelete, managerStorage.data() + 0x04,
                sizeof(deleteOnEnvironmentDelete));
    EXPECT_EQ(deleteOnEnvironmentDelete, IVP_TRUE);
    manager->~IVP_Material_Manager();
    ExpectRawMaterialManagerField = false;

    alignas(IVP_Collision_Filter_Exclusive_Pair)
        std::array<std::byte,
                   sizeof(IVP_Collision_Filter_Exclusive_Pair)> pairStorage;
    std::fill(pairStorage.begin(), pairStorage.end(), std::byte{0xA5});
    ExpectRawExclusivePairField = true;
    SawRawExclusivePairField = false;
    auto *pair = ::new (pairStorage.data())
        IVP_Collision_Filter_Exclusive_Pair();
    EXPECT_TRUE(SawRawExclusivePairField);
    void *hashTable = reinterpret_cast<void *>(1);
    std::memcpy(&hashTable, pairStorage.data() + 0x04, sizeof(hashTable));
    EXPECT_EQ(hashTable, nullptr);
    pair->~IVP_Collision_Filter_Exclusive_Pair();
    ExpectRawExclusivePairField = false;

    IVP_Template_Buoyancy buoyancyDefinition;
    alignas(IVP_Attacher_To_Cores_Buoyancy)
        std::array<std::byte,
                   sizeof(IVP_Attacher_To_Cores_Buoyancy)> attacherStorage;
    std::fill(attacherStorage.begin(), attacherStorage.end(),
              std::byte{0xA5});
    ExpectRawBuoyancyAttacherFields = true;
    SawRawBuoyancyAttacherFields = false;
    auto *attacher = ::new (attacherStorage.data())
        IVP_Attacher_To_Cores_Buoyancy(
            buoyancyDefinition, nullptr, nullptr);
    EXPECT_TRUE(SawRawBuoyancyAttacherFields);
    attacher->~IVP_Attacher_To_Cores_Buoyancy();
    ExpectRawBuoyancyAttacherFields = false;

    alignas(IVP_OV_Element)
        std::array<std::byte, sizeof(IVP_OV_Element)> ovElementStorage;
    std::fill(ovElementStorage.begin(), ovElementStorage.end(),
              std::byte{0xA5});
    ExpectRawOVElementFields = true;
    SawRawOVElementFields = false;
    OVElementDestructorCalls = 0;
    auto *ovElement = ::new (ovElementStorage.data())
        IVP_OV_Element(reinterpret_cast<IVP_Real_Object *>(0x12340000u));
    EXPECT_TRUE(SawRawOVElementFields);
    EXPECT_EQ(ovElement->real_object,
              reinterpret_cast<IVP_Real_Object *>(0x12340000u));
    ovElement->~IVP_OV_Element();
    EXPECT_EQ(OVElementDestructorCalls, 1);
    ExpectRawOVElementFields = false;

    alignas(IVP_OV_Node)
        std::array<std::byte, sizeof(IVP_OV_Node)> ovNodeStorage;
    std::fill(ovNodeStorage.begin(), ovNodeStorage.end(), std::byte{0xA5});
    ExpectRawOVNodeFields = true;
    SawRawOVNodeFields = false;
    OVNodeDestructorCalls = 0;
    auto *ovNode = ::new (ovNodeStorage.data()) IVP_OV_Node();
    EXPECT_TRUE(SawRawOVNodeFields);
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::OVNodeDestruct, ovNode);
    EXPECT_EQ(OVNodeDestructorCalls, 1);
    ExpectRawOVNodeFields = false;

    alignas(IVP_OV_Tree_Manager)
        std::array<std::byte, sizeof(IVP_OV_Tree_Manager)> ovTreeStorage;
    std::fill(ovTreeStorage.begin(), ovTreeStorage.end(), std::byte{0xA5});
    ExpectRawOVTreeManagerFields = true;
    SawRawOVTreeManagerFields = false;
    OVTreeManagerDestructorCalls = 0;
    auto *ovTree = ::new (ovTreeStorage.data()) IVP_OV_Tree_Manager();
    EXPECT_TRUE(SawRawOVTreeManagerFields);
    ovTree->~IVP_OV_Tree_Manager();
    EXPECT_EQ(OVTreeManagerDestructorCalls, 1);
    ExpectRawOVTreeManagerFields = false;

    IVP_Template_Spring springDefinition;
    alignas(ModSpring)
        std::array<std::byte, sizeof(ModSpring)> springStorage;
    std::fill(springStorage.begin(), springStorage.end(), std::byte{0xA5});
    ExpectRawSpringFields = true;
    SawRawSpringFields = false;
    auto *spring = ::new (springStorage.data())
        ModSpring(nullptr, &springDefinition);
    EXPECT_TRUE(SawRawSpringFields);
    spring->~ModSpring();
    ExpectRawSpringFields = false;

    alignas(ModActiveSpring)
        std::array<std::byte, sizeof(ModActiveSpring)> activeSpringStorage;
    std::fill(
        activeSpringStorage.begin(), activeSpringStorage.end(),
        std::byte{0xA5});
    ExpectRawSpringFields = false;
    ExpectRawActiveSpringFields = true;
    SawRawActiveSpringFields = false;
    auto *activeSpring = ::new (activeSpringStorage.data())
        ModActiveSpring(nullptr, &springDefinition);
    EXPECT_TRUE(SawRawActiveSpringFields);
    activeSpring->~ModActiveSpring();
    ExpectRawActiveSpringFields = false;

    alignas(IVP_Mindist_Manager)
        std::array<std::byte, sizeof(IVP_Mindist_Manager)>
            mindistManagerStorage;
    std::fill(mindistManagerStorage.begin(), mindistManagerStorage.end(),
              std::byte{0xA5});
    ExpectRawMindistManagerFields = true;
    SawRawMindistManagerFields = false;
    MindistManagerDestructorCalls = 0;
    auto *environment = reinterpret_cast<IVP_Environment *>(0x13572468u);
    auto *mindistManager = ::new (mindistManagerStorage.data())
        IVP_Mindist_Manager(environment);
    EXPECT_TRUE(SawRawMindistManagerFields);
    EXPECT_EQ(mindistManager->environment, environment);
    EXPECT_EQ(mindistManager->exact_mindists, nullptr);
    EXPECT_EQ(mindistManager->wheel_look_ahead_mindists.len(), 0);
    EXPECT_EQ(mindistManager->invalid_mindists, nullptr);
    mindistManager->~IVP_Mindist_Manager();
    EXPECT_EQ(MindistManagerDestructorCalls, 1);
    ExpectRawMindistManagerFields = false;
}

TEST(IvpCrossDllOwnership, NothrowAllocationNeverFallsBackToModHeap) {
    State = {};
    State.FailNextAllocation = true;

    auto *material = new (std::nothrow) IVP_Material_Simple();

    EXPECT_EQ(material, nullptr);
    EXPECT_EQ(State.Allocations, 0);
    EXPECT_EQ(State.Deallocations, 0);
}

TEST(IvpCrossDllOwnership, AuditedPublicOwnershipSurfaceUsesRetailHeap) {
    State = {};

    ExerciseRetailAllocationPair<IVP_Draw_Vector_Debug>();
    ExerciseRetailAllocationPair<IVP_Material>();
    ExerciseRetailAllocationPair<IVP_Material_Manager>();
    ExerciseRetailAllocationPair<IVP_Controller_Raycast_Car>();
    ExerciseRetailAllocationPair<IVP_Collision_Filter>();
    ExerciseRetailAllocationPair<IVP_PerformanceCounter>();
    ExerciseRetailAllocationPair<IVP_Core>();
    ExerciseRetailAllocationPair<IVP_Simulation_Unit>();
    ExerciseRetailAllocationPair<IVP_Controller>();
    ExerciseRetailAllocationPair<IVP_Anomaly_Limits>();
    ExerciseRetailAllocationPair<IVP_Anomaly_Manager>();
    ExerciseRetailAllocationPair<IVP_Range_Manager>();
    ExerciseRetailAllocationPair<IVP_U_Active_Value_Manager>();
    ExerciseRetailAllocationPair<IVP_U_Active_Value>();
    ExerciseRetailAllocationPair<IVP_BetterDebugmanager>();
    ExerciseRetailAllocationPair<IVP_Forcefield>();
    ExerciseRetailAllocationPair<IVP_Liquid_Surface_Descriptor_Simple>();
    ExerciseRetailAllocationPair<IVP_Attacher_To_Cores_Buoyancy>();
    ExerciseRetailAllocationPair<IVP_Actuator>();
    ExerciseRetailAllocationPair<IVP_Actuator_Check_Dist>();
    ExerciseRetailAllocationPair<IVP_Collision_Delegator_Root_Mindist>();
    ExerciseRetailAllocationPair<IVP_OV_Element>();
    ExerciseRetailAllocationPair<IVP_OV_Tree_Manager>();
    ExerciseRetailAllocationPair<IVP_Mindist_Manager>();
    ExerciseRetailAllocationPair<IVP_Hash>();
    ExerciseRetailAllocationPair<IVP_U_Memory>();

    // These factory-only types cannot be caller-constructed, but a delete
    // expression generated in a consuming Mod must still return their retail
    // allocations to physics_RT.
    ExerciseRetailDeallocation<IVP_Environment>();
    ExerciseRetailDeallocation<IVP_Controller_Phantom>();

    EXPECT_EQ(State.Allocations, 28);
    EXPECT_EQ(State.Deallocations, State.Allocations);
}
