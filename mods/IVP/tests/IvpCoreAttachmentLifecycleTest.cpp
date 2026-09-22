#include "IvpTestAdapter.h"

#include "BML/IVP/Attacher.h"
#include "BML/IVP/Core.h"
#include "BML/IVP/Forcefield.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <vector>

namespace {

struct RawVHash {
    void *vtable;
    int sizeMm;
    std::uint32_t countAndOwnership;
    IVP_VHash_Elem *elements;
};

struct RawVHashStore {
    int size;
    int sizeMm;
    int count;
    IVP_VHash_Store_Elem *elements;
    void *dontFree;
};

static_assert(sizeof(RawVHash) == sizeof(IVP_VHash));
static_assert(sizeof(RawVHashStore) == sizeof(IVP_VHash_Store));

int VHashCount(const RawVHash *hash) {
    return static_cast<int>(hash->countAndOwnership & 0x00FFFFFFu);
}

void SetVHashCount(RawVHash *hash, int count) {
    hash->countAndOwnership =
        (hash->countAndOwnership & 0xFF000000u) |
        (static_cast<std::uint32_t>(count) & 0x00FFFFFFu);
}

void __fastcall VHashConstruct(IVP_VHash *hash, void *, int size) {
    auto *raw = reinterpret_cast<RawVHash *>(hash);
    raw->sizeMm = size - 1;
    raw->countAndOwnership = 0;
    raw->elements = static_cast<IVP_VHash_Elem *>(
        std::calloc(static_cast<std::size_t>(size), sizeof(IVP_VHash_Elem)));
}

void __fastcall VHashDestruct(IVP_VHash *hash, void *) {
    auto *raw = reinterpret_cast<RawVHash *>(hash);
    std::free(raw->elements);
    raw->elements = nullptr;
    raw->sizeMm = -1;
    SetVHashCount(raw, 0);
}

void __fastcall VHashAdd(
    IVP_VHash *hash, void *, const void *element, int hashIndex) {
    auto *raw = reinterpret_cast<RawVHash *>(hash);
    for (int index = 0; index <= raw->sizeMm; ++index) {
        if (raw->elements[index].elem)
            continue;
        raw->elements[index].hash_index =
            static_cast<std::uint32_t>(hashIndex);
        raw->elements[index].elem = element;
        SetVHashCount(raw, VHashCount(raw) + 1);
        return;
    }
    ADD_FAILURE() << "test VHash capacity exhausted";
}

void *__fastcall VHashFind(
    IVP_VHash *hash, void *, const void *element, unsigned int) {
    auto *raw = reinterpret_cast<RawVHash *>(hash);
    for (int index = 0; index <= raw->sizeMm; ++index) {
        if (raw->elements[index].elem == element)
            return const_cast<void *>(element);
    }
    return nullptr;
}

void *__fastcall VHashRemove(
    IVP_VHash *hash, void *, const void *element, unsigned int hashIndex) {
    auto *raw = reinterpret_cast<RawVHash *>(hash);
    for (int index = 0; index <= raw->sizeMm; ++index) {
        IVP_VHash_Elem &entry = raw->elements[index];
        if (entry.elem != element)
            continue;
        void *removed = const_cast<void *>(entry.elem);
        entry.hash_index = hashIndex;
        entry.elem = nullptr;
        SetVHashCount(raw, VHashCount(raw) - 1);
        return removed;
    }
    return nullptr;
}

void __fastcall VHashStoreConstruct(
    IVP_VHash_Store *hash, void *, int size) {
    auto *raw = reinterpret_cast<RawVHashStore *>(hash);
    raw->size = size;
    raw->sizeMm = size - 1;
    raw->count = 0;
    raw->elements = static_cast<IVP_VHash_Store_Elem *>(std::calloc(
        static_cast<std::size_t>(size), sizeof(IVP_VHash_Store_Elem)));
    raw->dontFree = nullptr;
}

void __fastcall VHashStoreDestruct(IVP_VHash_Store *hash, void *) {
    auto *raw = reinterpret_cast<RawVHashStore *>(hash);
    std::free(raw->elements);
    raw->elements = nullptr;
    raw->count = 0;
}

void __fastcall VHashStoreAdd(
    IVP_VHash_Store *hash, void *, void *key, void *element) {
    auto *raw = reinterpret_cast<RawVHashStore *>(hash);
    for (int index = 0; index < raw->size; ++index) {
        IVP_VHash_Store_Elem &entry = raw->elements[index];
        if (entry.key_elem)
            continue;
        entry.key_elem = key;
        entry.elem = element;
        ++raw->count;
        return;
    }
    ADD_FAILURE() << "test VHash store capacity exhausted";
}

void *__fastcall VHashStoreFind(
    IVP_VHash_Store *hash, void *, void *key) {
    auto *raw = reinterpret_cast<RawVHashStore *>(hash);
    for (int index = 0; index < raw->size; ++index) {
        if (raw->elements[index].key_elem == key)
            return raw->elements[index].elem;
    }
    return nullptr;
}

void *__fastcall VHashStoreRemove(
    IVP_VHash_Store *hash, void *, void *key) {
    auto *raw = reinterpret_cast<RawVHashStore *>(hash);
    for (int index = 0; index < raw->size; ++index) {
        IVP_VHash_Store_Elem &entry = raw->elements[index];
        if (entry.key_elem != key)
            continue;
        void *removed = entry.elem;
        entry = {};
        --raw->count;
        return removed;
    }
    return nullptr;
}

void *Allocate(unsigned int size) { return std::malloc(size); }
void Free(void *memory) { std::free(memory); }

int g_retailObjectAllocations = 0;
int g_retailObjectDeallocations = 0;

void *RetailObjectNew(unsigned int size) {
    ++g_retailObjectAllocations;
    return std::malloc(size);
}

void RetailObjectDelete(void *memory) {
    ++g_retailObjectDeallocations;
    std::free(memory);
}

void __fastcall IncrementVector(IVP_U_Vector_Base *vector, void *) {
    const std::uint16_t capacity = vector->memsize == 0
        ? 4
        : static_cast<std::uint16_t>(vector->memsize * 2);
    void **elements = static_cast<void **>(
        std::realloc(vector->elems, capacity * sizeof(void *)));
    ASSERT_NE(elements, nullptr);
    vector->elems = elements;
    vector->memsize = capacity;
}

void __cdecl AddControllerToCore(
    IVP_Controller_Independent *controller, IVP_Core *core);
void __cdecl RemoveControllerFromCore(
    IVP_Controller_Independent *controller, IVP_Core *core);
void __fastcall DestructController(IVP_Controller *controller, void *);
void __fastcall InitializeSingleObjectCore(
    IVP_Core *core, void *, IVP_Real_Object *object);
void __fastcall AddConstructedCoreSimulationUnit(
    IVP_Sim_Units_Manager *manager, void *, IVP_Simulation_Unit *unit);
void __fastcall DestructConstructedCore(IVP_Core *core, void *);

const BML::IVP::Test::RetailCallBinding kRetailCalls[] = {
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::OperatorNew, &RetailObjectNew),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::OperatorDelete, &RetailObjectDelete),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::Allocate, &Allocate),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::Free, &Free),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::VectorIncrementMemory, &IncrementVector),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::VHashConstruct, &VHashConstruct),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::VHashDestruct, &VHashDestruct),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::VHashAdd, &VHashAdd),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::VHashRemove, &VHashRemove),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::VHashFind, &VHashFind),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::VHashStoreConstruct, &VHashStoreConstruct),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::VHashStoreDestruct, &VHashStoreDestruct),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::VHashStoreAdd, &VHashStoreAdd),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::VHashStoreRemove, &VHashStoreRemove),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::VHashStoreFind, &VHashStoreFind),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ControllerManagerAddToCore, &AddControllerToCore),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ControllerManagerRemoveFromCore, &RemoveControllerFromCore),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ControllerDestruct, &DestructController),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::CoreInitialize, &InitializeSingleObjectCore),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::SimulationUnitsManagerAdd,
        &AddConstructedCoreSimulationUnit),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::CoreDestruct, &DestructConstructedCore),
};

uintptr_t ResolveRetailCall(std::uint32_t rva) noexcept {
    return BML::IVP::Test::Resolve(rva, kRetailCalls);
}


class TrackingAttachment;
using TrackingAttacherBase = IVP_Attacher_To_Cores<TrackingAttachment>;

int g_liveAttachments = 0;
int g_destroyedAttachments = 0;
int g_destroyedAttachers = 0;
int g_destroyedForcefields = 0;
int g_controllerBaseDestructs = 0;
std::vector<IVP_Core *> g_controllerAdds;
std::vector<IVP_Core *> g_controllerRemoves;
std::vector<IVP_Controller_Independent *> g_controllerIdentities;
IVP_Real_Object *g_initializedObject = nullptr;
IVP_Core *g_initializedCore = nullptr;
IVP_Sim_Units_Manager *g_addedToManager = nullptr;
IVP_Simulation_Unit *g_addedSimulationUnit = nullptr;
IVP_Core *g_destructedCore = nullptr;
std::vector<std::string_view> g_coreLifecycle;
bool g_expectRawControllerVectorStorage = false;

alignas(void *) std::array<std::byte, 0x0C> g_constructorEnvironment{};
alignas(void *) std::array<std::byte, 1> g_constructorSimulationUnit{};

void __fastcall InitializeSingleObjectCore(
    IVP_Core *core, void *, IVP_Real_Object *object) {
    g_coreLifecycle.emplace_back("init");
    g_initializedCore = core;
    g_initializedObject = object;
    if (g_expectRawControllerVectorStorage) {
        const auto *bytes = reinterpret_cast<const unsigned char *>(core);
        for (std::size_t offset = 0x1C8; offset < 0x1D0; ++offset)
            EXPECT_EQ(bytes[offset], 0xA5u) << "offset 0x" << std::hex << offset;
    }
    core->controllers_of_core.memsize = 0;
    core->controllers_of_core.n_elems = 0;
    core->controllers_of_core.elems = nullptr;
    core->environment = reinterpret_cast<IVP_Environment *>(
        g_constructorEnvironment.data());
    core->sim_unit_of_core = reinterpret_cast<IVP_Simulation_Unit *>(
        g_constructorSimulationUnit.data());
    core->movement_state = IVP_MT_NOT_SIM;
    core->objects.reset();
    core->objects.add(object);
}

void __fastcall AddConstructedCoreSimulationUnit(
    IVP_Sim_Units_Manager *manager, void *, IVP_Simulation_Unit *unit) {
    g_coreLifecycle.emplace_back("register simulation unit");
    g_addedToManager = manager;
    g_addedSimulationUnit = unit;
}

void __fastcall DestructConstructedCore(IVP_Core *core, void *) {
    g_coreLifecycle.emplace_back("destruct");
    g_destructedCore = core;
}

void __cdecl AddControllerToCore(
    IVP_Controller_Independent *controller, IVP_Core *core) {
    g_controllerAdds.push_back(core);
    g_controllerIdentities.push_back(controller);
}

void __cdecl RemoveControllerFromCore(
    IVP_Controller_Independent *controller, IVP_Core *core) {
    g_controllerRemoves.push_back(core);
    g_controllerIdentities.push_back(controller);
}

void __fastcall DestructController(IVP_Controller *, void *) {
    ++g_controllerBaseDestructs;
}

class TrackingAttachment {
public:
    TrackingAttachment(TrackingAttacherBase *owner, IVP_Core *core)
        : owner_(owner), core_(core) {
        ++g_liveAttachments;
    }

    ~TrackingAttachment() {
        owner_->attachment_is_going_to_be_deleted(this, core_);
        --g_liveAttachments;
        ++g_destroyedAttachments;
    }

private:
    TrackingAttacherBase *owner_;
    IVP_Core *core_;
};

class TrackingAttacher final : public TrackingAttacherBase {
public:
    explicit TrackingAttacher(IVP_U_Set_Active<IVP_Core> *cores)
        : TrackingAttacherBase(cores) {}

    ~TrackingAttacher() override { ++g_destroyedAttachers; }

    TrackingAttachment *find_attachment(IVP_Core *core) {
        return static_cast<TrackingAttachment *>(
            core_to_attachment_hash.find_elem(core));
    }
};

class TrackingForcefield final : public IVP_Forcefield {
public:
    TrackingForcefield(
        IVP_U_Set_Active<IVP_Core> *cores, IVP_BOOL ownerOfSet)
        : IVP_Forcefield(nullptr, cores, ownerOfSet) {}

    ~TrackingForcefield() override { ++g_destroyedForcefields; }

    IVP_Controller_Independent *controller_identity() {
        return static_cast<IVP_Controller_Independent *>(this);
    }

    std::ptrdiff_t listener_offset() {
        return reinterpret_cast<std::byte *>(
                   static_cast<IVP_Listener_Set_Active<IVP_Core> *>(this)) -
               reinterpret_cast<std::byte *>(this);
    }

    std::ptrdiff_t controller_offset() {
        return reinterpret_cast<std::byte *>(controller_identity()) -
               reinterpret_cast<std::byte *>(this);
    }

    std::ptrdiff_t core_set_offset() {
        return reinterpret_cast<std::byte *>(&set_of_cores) -
               reinterpret_cast<std::byte *>(this);
    }

    std::ptrdiff_t owner_flag_offset() {
        return reinterpret_cast<std::byte *>(&i_am_owner_of_set_of_cores) -
               reinterpret_cast<std::byte *>(this);
    }

    IVP_CONTROLLER_PRIORITY priority() {
        return get_controller_priority();
    }

private:
    void do_simulation_controller(
        IVP_Event_Sim *, IVP_U_Vector<IVP_Core> *) override {}
};

IVP_Core *CoreFrom(std::array<std::byte, sizeof(IVP_Core)> &storage) {
    return reinterpret_cast<IVP_Core *>(storage.data());
}

class ConstructibleSingleObjectCore final : public IVP_Core {
public:
    explicit ConstructibleSingleObjectCore(IVP_Real_Object *object)
        : IVP_Core(object) {}
};

static_assert(sizeof(ConstructibleSingleObjectCore) == sizeof(IVP_Core));

} // namespace

extern "C" uintptr_t BML_IvpTestResolveRetailCall(
    std::uint32_t rva) noexcept {
    return ResolveRetailCall(rva);
}

namespace {

TEST(IvpCoreAttachmentLifecycle,
     SingleObjectCoreRegistersItsInitializedSimulationUnit) {
    alignas(void *) std::array<std::byte, 1> objectStorage{};
    alignas(void *) std::array<std::byte, 1> managerStorage{};
    auto *object = reinterpret_cast<IVP_Real_Object *>(objectStorage.data());
    auto *manager = reinterpret_cast<IVP_Sim_Units_Manager *>(
        managerStorage.data());
    std::memcpy(g_constructorEnvironment.data() + 0x08,
                &manager, sizeof(manager));

    g_initializedObject = nullptr;
    g_initializedCore = nullptr;
    g_addedToManager = nullptr;
    g_addedSimulationUnit = nullptr;
    g_destructedCore = nullptr;
    g_coreLifecycle.clear();
    g_expectRawControllerVectorStorage = false;

    {
        ConstructibleSingleObjectCore core(object);

        EXPECT_EQ(g_initializedCore, &core);
        EXPECT_EQ(g_initializedObject, object);
        ASSERT_EQ(core.objects.len(), 1);
        EXPECT_EQ(core.objects.element_at(0), object);
        EXPECT_EQ(core.movement_state, IVP_MT_NOT_SIM);
        EXPECT_EQ(g_addedToManager, manager);
        EXPECT_EQ(g_addedSimulationUnit, core.sim_unit_of_core);
        EXPECT_EQ(g_coreLifecycle,
                  (std::vector<std::string_view>{
                      "init", "register simulation unit"}));
    }

    EXPECT_EQ(g_destructedCore, g_initializedCore);
    EXPECT_EQ(g_coreLifecycle,
              (std::vector<std::string_view>{
                  "init", "register simulation unit", "destruct"}));
}

TEST(IvpCoreAttachmentLifecycle,
     ReconstructedCoreConstructorLeavesControllerVectorToRetailInitializer) {
    alignas(ConstructibleSingleObjectCore)
        std::array<unsigned char, sizeof(ConstructibleSingleObjectCore)> storage{};
    alignas(void *) std::array<std::byte, 1> objectStorage{};
    alignas(void *) std::array<std::byte, 1> managerStorage{};
    auto *object = reinterpret_cast<IVP_Real_Object *>(objectStorage.data());
    auto *manager = reinterpret_cast<IVP_Sim_Units_Manager *>(managerStorage.data());
    std::memcpy(g_constructorEnvironment.data() + 0x08,
                &manager, sizeof(manager));
    storage.fill(0xA5u);
    g_coreLifecycle.clear();
    g_expectRawControllerVectorStorage = true;

    auto *core = ::new (storage.data()) ConstructibleSingleObjectCore(object);

    EXPECT_EQ(core->controllers_of_core.memsize, 0);
    EXPECT_EQ(core->controllers_of_core.n_elems, 0);
    EXPECT_EQ(core->controllers_of_core.elems, nullptr);
    core->~ConstructibleSingleObjectCore();
    EXPECT_EQ(g_coreLifecycle,
              (std::vector<std::string_view>{
                  "init", "register simulation unit", "destruct"}));
    g_expectRawControllerVectorStorage = false;
}

TEST(IvpCoreAttachmentLifecycle,
     TracksControllerAttachmentsAcrossCoreActivationAndShutdown) {
    std::array<std::byte, sizeof(IVP_Core)> firstStorage{};
    std::array<std::byte, sizeof(IVP_Core)> secondStorage{};
    std::array<std::byte, sizeof(IVP_Core)> thirdStorage{};
    IVP_Core *first = CoreFrom(firstStorage);
    IVP_Core *second = CoreFrom(secondStorage);
    IVP_Core *third = CoreFrom(thirdStorage);

    g_liveAttachments = 0;
    g_destroyedAttachments = 0;
    g_destroyedAttachers = 0;

    {
        IVP_U_Set_Active<IVP_Core> activeCores(16);
        activeCores.add_element(first);
        activeCores.add_element(second);

        auto *attacher = new TrackingAttacher(&activeCores);
        EXPECT_EQ(g_liveAttachments, 2);
        EXPECT_NE(attacher->find_attachment(first), nullptr);
        EXPECT_NE(attacher->find_attachment(second), nullptr);

        activeCores.add_element(third);
        EXPECT_EQ(g_liveAttachments, 3);
        EXPECT_NE(attacher->find_attachment(third), nullptr);

        activeCores.remove_element(second);
        EXPECT_EQ(g_liveAttachments, 2);
        EXPECT_EQ(g_destroyedAttachments, 1);
        EXPECT_EQ(attacher->find_attachment(second), nullptr);

        // Destroying the active set removes the remaining per-core
        // attachments and then deletes the source-compatible attacher.
    }

    EXPECT_EQ(g_liveAttachments, 0);
    EXPECT_EQ(g_destroyedAttachments, 3);
    EXPECT_EQ(g_destroyedAttachers, 1);
}

TEST(IvpCoreAttachmentLifecycle,
     ForcefieldTracksAChangingCoreSetThroughControllerAbi) {
    std::array<std::byte, sizeof(IVP_Core)> firstStorage{};
    std::array<std::byte, sizeof(IVP_Core)> secondStorage{};
    std::array<std::byte, sizeof(IVP_Core)> thirdStorage{};
    IVP_Core *first = CoreFrom(firstStorage);
    IVP_Core *second = CoreFrom(secondStorage);
    IVP_Core *third = CoreFrom(thirdStorage);

    g_destroyedForcefields = 0;
    g_controllerBaseDestructs = 0;
    g_controllerAdds.clear();
    g_controllerRemoves.clear();
    g_controllerIdentities.clear();

    IVP_U_Set_Active<IVP_Core> activeCores(16);
    activeCores.add_element(first);
    activeCores.add_element(second);
    {
        TrackingForcefield forcefield(&activeCores, IVP_FALSE);
        IVP_Controller_Independent *controller =
            forcefield.controller_identity();

        EXPECT_EQ(forcefield.listener_offset(), 0);
        EXPECT_EQ(forcefield.controller_offset(), 4);
        EXPECT_EQ(forcefield.core_set_offset(), 8);
        EXPECT_EQ(forcefield.owner_flag_offset(), 12);
        EXPECT_EQ(forcefield.priority(), IVP_CP_ACTUATOR);
        EXPECT_THAT(g_controllerAdds,
                    testing::UnorderedElementsAre(first, second));
        ASSERT_EQ(activeCores.get_listeners().n_elems, 1);

        activeCores.add_element(third);
        activeCores.remove_element(second);
        EXPECT_THAT(g_controllerAdds,
                    testing::UnorderedElementsAre(first, second, third));
        EXPECT_EQ(g_controllerRemoves,
                  (std::vector<IVP_Core *>{second}));

        ASSERT_EQ(g_controllerIdentities.size(), 4u);
        for (IVP_Controller_Independent *identity : g_controllerIdentities)
            EXPECT_EQ(identity, controller);
    }

    EXPECT_THAT(g_controllerRemoves,
                testing::UnorderedElementsAre(second, first, third));
    EXPECT_EQ(activeCores.get_listeners().n_elems, 0);
    EXPECT_EQ(g_destroyedForcefields, 1);
    EXPECT_EQ(g_controllerBaseDestructs, 1);
}

TEST(IvpCoreAttachmentLifecycle,
     OwningForcefieldDiesWithItsActiveCoreSet) {
    std::array<std::byte, sizeof(IVP_Core)> coreStorage{};
    IVP_Core *core = CoreFrom(coreStorage);

    g_destroyedForcefields = 0;
    g_controllerBaseDestructs = 0;
    g_controllerAdds.clear();
    g_controllerRemoves.clear();
    g_controllerIdentities.clear();
    g_retailObjectAllocations = 0;
    g_retailObjectDeallocations = 0;

    auto *activeCores = new IVP_U_Set_Active<IVP_Core>(16);
    activeCores->add_element(core);
    auto *forcefield =
        new TrackingForcefield(activeCores, IVP_TRUE);
    IVP_Controller_Independent *controller =
        forcefield->controller_identity();

    delete activeCores;

    EXPECT_EQ(g_controllerAdds, (std::vector<IVP_Core *>{core}));
    EXPECT_EQ(g_controllerRemoves, (std::vector<IVP_Core *>{core}));
    ASSERT_EQ(g_controllerIdentities.size(), 2u);
    EXPECT_EQ(g_controllerIdentities[0], controller);
    EXPECT_EQ(g_controllerIdentities[1], controller);
    EXPECT_EQ(g_destroyedForcefields, 1);
    EXPECT_EQ(g_controllerBaseDestructs, 1);
    EXPECT_EQ(g_retailObjectAllocations, 1);
    EXPECT_EQ(g_retailObjectDeallocations, 1);
}

} // namespace
