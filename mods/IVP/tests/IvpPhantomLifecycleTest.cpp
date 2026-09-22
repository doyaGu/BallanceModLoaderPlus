#include "IvpTestAdapter.h"

#include "BML/IVP/Buoyancy.h"
#include "BML/IVP/Listeners.h"
#include "BML/IVP/Templates.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <string_view>
#include <vector>

namespace {

int g_phantomConstructCount = 0;
int g_phantomDestructCount = 0;
int g_vhashDestructCount = 0;
int g_vectorFreeCount = 0;
int g_ownedObjectSetDeleteCount = 0;
int g_ownedCoreSetDeleteCount = 0;
int g_objectCounterDeleteCount = 0;
int g_coreCounterDeleteCount = 0;
std::vector<int> g_listenerOrder;

struct ObjectPrefix {
    std::byte bytes[0x1C];
    IVP_Controller_Phantom *controllerPhantom;
};

struct VHashLayout {
    void *vtable;
    int sizeMask;
    std::uint32_t elementState;
    IVP_VHash_Elem *elements;
};

static_assert(offsetof(ObjectPrefix, controllerPhantom) == 0x1C);
static_assert(sizeof(VHashLayout) == 0x10);

class PhantomHarness final : public IVP_Controller_Phantom {
public:
    PhantomHarness(
        IVP_Real_Object *object,
        const IVP_Template_Phantom *configuration)
        : IVP_Controller_Phantom(object, configuration) {}
};

class TrackingListener final : public IVP_Listener_Phantom {
public:
    explicit TrackingListener(int identifier) : id(identifier) {}

    void mindist_entered_volume(
        IVP_Controller_Phantom *, IVP_Mindist_Base *) override {}
    void mindist_left_volume(
        IVP_Controller_Phantom *, IVP_Mindist_Base *) override {}
    void core_entered_volume(
        IVP_Controller_Phantom *, IVP_Core *) override {}
    void core_left_volume(
        IVP_Controller_Phantom *, IVP_Core *) override {}
    void phantom_is_going_to_be_deleted_event(
        IVP_Controller_Phantom *) override {
        g_listenerOrder.push_back(id);
    }

private:
    int id;
};

void __fastcall ConstructVHash(IVP_VHash *hash, void *, int size) {
    auto *layout = reinterpret_cast<VHashLayout *>(hash);
    layout->sizeMask = size - 1;
    layout->elementState = 0;
    layout->elements = static_cast<IVP_VHash_Elem *>(
        std::calloc(static_cast<std::size_t>(size), sizeof(IVP_VHash_Elem)));
}

void __fastcall DestructVHash(IVP_VHash *hash, void *) {
    ++g_vhashDestructCount;
    auto *layout = reinterpret_cast<VHashLayout *>(hash);
    std::free(layout->elements);
    layout->elements = nullptr;
    layout->sizeMask = -1;
    layout->elementState = 0;
}

void __fastcall IncrementVector(IVP_U_Vector_Base *vector, void *) {
    const int oldCapacity = vector->memsize;
    const int newCapacity = std::max(2, oldCapacity * 2);
    vector->elems = static_cast<void **>(std::realloc(
        vector->elems,
        static_cast<std::size_t>(newCapacity) * sizeof(void *)));
    vector->memsize = static_cast<std::uint16_t>(newCapacity);
}

void __cdecl FreeVector(void *memory) {
    ++g_vectorFreeCount;
    std::free(memory);
}

void __fastcall ConstructTemplatePhantom(
    IVP_Template_Phantom *configuration, void *) {
    configuration->manage_intruding_objects = IVP_FALSE;
    configuration->manage_intruding_cores = IVP_FALSE;
    configuration->dont_check_for_unmoveables = IVP_FALSE;
    configuration->exit_policy_extra_radius = 0.5f;
    configuration->exit_policy_extra_time = 0.5f;
}

void __fastcall ConstructPhantom(
    IVP_Controller_Phantom *phantom, void *, IVP_Real_Object *object,
    const IVP_Template_Phantom *configuration) {
    ++g_phantomConstructCount;
    phantom->object = object;
    phantom->exit_policy_extra_radius =
        configuration->exit_policy_extra_radius;
    new (&phantom->listeners) IVP_U_Vector<IVP_Listener_Phantom>();
    new (&phantom->set_of_mindists) IVP_U_Set_Active<IVP_Mindist_Base>(16);
    phantom->set_of_objects = configuration->manage_intruding_objects
        ? reinterpret_cast<IVP_U_Set_Active<IVP_Real_Object> *>(0x1010u)
        : nullptr;
    phantom->mindist_object_counter = configuration->manage_intruding_objects
        ? reinterpret_cast<IVP_VHash_Store *>(0x2020u)
        : nullptr;
    phantom->mindist_core_counter = configuration->manage_intruding_cores
        ? reinterpret_cast<IVP_VHash_Store *>(0x3030u)
        : nullptr;
    phantom->set_of_cores = configuration->manage_intruding_cores
        ? reinterpret_cast<IVP_U_Set_Active<IVP_Core> *>(0x4040u)
        : nullptr;
    phantom->time_of_last_set_transformation = IVP_Time{};

    auto *prefix = reinterpret_cast<ObjectPrefix *>(object);
    prefix->controllerPhantom = phantom;
}

void __fastcall DestructPhantom(
    IVP_Controller_Phantom *phantom, void *) {
    ++g_phantomDestructCount;
    for (int index = phantom->listeners.len() - 1; index >= 0; --index) {
        phantom->listeners.element_at(index)
            ->phantom_is_going_to_be_deleted_event(phantom);
    }

    auto *prefix = reinterpret_cast<ObjectPrefix *>(phantom->object);
    prefix->controllerPhantom = nullptr;

    if (phantom->mindist_core_counter)
        ++g_coreCounterDeleteCount;
    if (phantom->mindist_object_counter)
        ++g_objectCounterDeleteCount;
    if (phantom->set_of_objects)
        ++g_ownedObjectSetDeleteCount;
    if (phantom->set_of_cores)
        ++g_ownedCoreSetDeleteCount;

    phantom->mindist_core_counter = nullptr;
    phantom->mindist_object_counter = nullptr;
    phantom->set_of_objects = nullptr;
    phantom->set_of_cores = nullptr;
    phantom->set_of_mindists.~IVP_U_Set_Active<IVP_Mindist_Base>();
    phantom->listeners.~IVP_U_Vector<IVP_Listener_Phantom>();
}

const BML::IVP::Test::RetailCallBinding kRetailCalls[] = {
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ControllerPhantomConstruct, &ConstructPhantom),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ControllerPhantomDestruct, &DestructPhantom),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::TemplatePhantomConstruct, &ConstructTemplatePhantom),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::VHashConstruct, &ConstructVHash),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::VHashDestruct, &DestructVHash),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::VectorIncrementMemory, &IncrementVector),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::Free, &FreeVector),
};

uintptr_t ResolveRetailCall(std::uint32_t rva) noexcept {
    return BML::IVP::Test::Resolve(rva, kRetailCalls);
}


void ResetObservations() {
    g_phantomConstructCount = 0;
    g_phantomDestructCount = 0;
    g_vhashDestructCount = 0;
    g_vectorFreeCount = 0;
    g_ownedObjectSetDeleteCount = 0;
    g_ownedCoreSetDeleteCount = 0;
    g_objectCounterDeleteCount = 0;
    g_coreCounterDeleteCount = 0;
    g_listenerOrder.clear();
}

} // namespace

extern "C" uintptr_t BML_IvpTestResolveRetailCall(
    std::uint32_t rva) noexcept {
    return ResolveRetailCall(rva);
}

namespace {

TEST(IvpPhantomLifecycle,
     ConversionTeardownNotifiesListenersAndReleasesOwnedTrackingOnce) {
    ResetObservations();

    ObjectPrefix object{};
    IVP_Template_Phantom configuration;
    configuration.manage_intruding_objects = IVP_TRUE;
    configuration.manage_intruding_cores = IVP_TRUE;

    TrackingListener first(1);
    TrackingListener second(2);
    {
        PhantomHarness phantom(
            reinterpret_cast<IVP_Real_Object *>(&object), &configuration);
        ASSERT_EQ(object.controllerPhantom, &phantom);
        phantom.add_listener_phantom(&first);
        phantom.add_listener_phantom(&second);
        EXPECT_EQ(phantom.get_intruding_objects(), phantom.set_of_objects);
        EXPECT_EQ(phantom.get_intruding_cores(), phantom.set_of_cores);
        EXPECT_EQ(phantom.get_object(),
                  reinterpret_cast<IVP_Real_Object *>(&object));
    }

    EXPECT_EQ(object.controllerPhantom, nullptr);
    EXPECT_EQ(g_listenerOrder, (std::vector<int>{2, 1}));
    EXPECT_EQ(g_phantomConstructCount, 1);
    EXPECT_EQ(g_phantomDestructCount, 1);
    EXPECT_EQ(g_objectCounterDeleteCount, 1);
    EXPECT_EQ(g_coreCounterDeleteCount, 1);
    EXPECT_EQ(g_ownedObjectSetDeleteCount, 1);
    EXPECT_EQ(g_ownedCoreSetDeleteCount, 1);
    EXPECT_EQ(g_vhashDestructCount, 1);
    EXPECT_EQ(g_vectorFreeCount, 1);
}

} // namespace
