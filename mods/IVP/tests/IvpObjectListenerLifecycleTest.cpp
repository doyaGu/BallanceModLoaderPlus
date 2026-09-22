#include "IvpTestAdapter.h"

#include "BML/IVP/Environment.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace {

using ObjectListenerTable = BML::IVP::Detail::ObjectListenerTable;

ObjectListenerTable *g_objectListenerTable = nullptr;
int g_tableAllocations = 0;
int g_tableDeletes = 0;
int g_lastHashIndex = 0;

void *OperatorNew(unsigned int size) {
    ++g_tableAllocations;
    return std::malloc(size);
}

void OperatorDelete(void *memory) {
    ++g_tableDeletes;
    std::free(memory);
}

void Free(void *memory) { std::free(memory); }

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

ObjectListenerTable *__fastcall FindObjectListenerTable(
    IVP_VHash *, void *, IVP_Real_Object *object) {
    return g_objectListenerTable &&
                   g_objectListenerTable->real_object == object
        ? g_objectListenerTable
        : nullptr;
}

ObjectListenerTable *__fastcall RemoveObjectListenerTable(
    IVP_VHash *, void *, IVP_Real_Object *object) {
    ObjectListenerTable *removed = FindObjectListenerTable(nullptr, nullptr,
                                                            object);
    if (removed)
        g_objectListenerTable = nullptr;
    return removed;
}

void __fastcall AddHashElement(
    IVP_VHash *, void *, const void *element, int hashIndex) {
    ASSERT_EQ(g_objectListenerTable, nullptr);
    g_objectListenerTable = const_cast<ObjectListenerTable *>(
        static_cast<const ObjectListenerTable *>(element));
    g_lastHashIndex = hashIndex;
}

const BML::IVP::Test::RetailCallBinding kRetailCalls[] = {
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ObjectCallbackTableFind, &FindObjectListenerTable),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ObjectCallbackTableRemove, &RemoveObjectListenerTable),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::VHashAdd, &AddHashElement),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::OperatorNew, &OperatorNew),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::OperatorDelete, &OperatorDelete),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::VectorIncrementMemory, &IncrementVector),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::Free, &Free),
};

uintptr_t ResolveRetailCall(std::uint32_t rva) noexcept {
    return BML::IVP::Test::Resolve(rva, kRetailCalls);
}


class RecordingListener final : public IVP_Listener_Object {
public:
    RecordingListener(IVP_Real_Object *object, bool removeWhenFrozen)
        : object_(object), remove_when_frozen_(removeWhenFrozen) {}

    void event_object_deleted(IVP_Event_Object *) override {
        ++deleted_count;
    }
    void event_object_created(IVP_Event_Object *) override {
        ++created_count;
    }
    void event_object_revived(IVP_Event_Object *) override {
        ++revived_count;
    }
    void event_object_frozen(IVP_Event_Object *) override {
        ++frozen_count;
        if (remove_when_frozen_)
            object_->remove_listener_object(this);
    }

    int deleted_count = 0;
    int created_count = 0;
    int revived_count = 0;
    int frozen_count = 0;

private:
    IVP_Real_Object *object_;
    bool remove_when_frozen_;
};

enum class ObjectEvent {
    Frozen,
    Revived,
};

void DeliverObjectEvent(IVP_Event_Object *event, ObjectEvent kind) {
    ObjectListenerTable *table = g_objectListenerTable;
    if (!table)
        return;

    for (int index = table->listeners.len() - 1; index >= 0; --index) {
        IVP_Listener_Object *listener = table->listeners.element_at(index);
        if (kind == ObjectEvent::Frozen)
            listener->event_object_frozen(event);
        else
            listener->event_object_revived(event);

        // Ballance performs this lookup after every callback except the last,
        // allowing a callback to unregister the final table safely.
        if (index > 0 && !g_objectListenerTable)
            break;
    }
}

} // namespace

extern "C" uintptr_t BML_IvpTestResolveRetailCall(
    std::uint32_t rva) noexcept {
    return ResolveRetailCall(rva);
}

namespace {

TEST(IvpObjectListenerLifecycle,
     ListenerCanUnregisterDuringFreezeWithoutLosingPeerCallbacks) {
    g_objectListenerTable = nullptr;
    g_tableAllocations = 0;
    g_tableDeletes = 0;
    g_lastHashIndex = 0;

    std::array<std::byte, sizeof(IVP_Cluster_Manager)> clusterStorage{};
    std::array<std::byte, sizeof(IVP_Environment)> environmentStorage{};
    std::array<std::byte, sizeof(IVP_Real_Object)> objectStorage{};
    std::uint32_t callbackHashIdentity = 0;

    auto *cluster = reinterpret_cast<IVP_Cluster_Manager *>(
        clusterStorage.data());
    auto *environment = reinterpret_cast<IVP_Environment *>(
        environmentStorage.data());
    auto *object = reinterpret_cast<IVP_Real_Object *>(objectStorage.data());

    *reinterpret_cast<void **>(clusterStorage.data() + 0x08) =
        &callbackHashIdentity;
    environment->cluster_manager = cluster;
    object->environment = environment;
    object->flags = 0;

    RecordingListener persistent(object, false);
    RecordingListener oneShot(object, true);

    environment->add_listener_object_private(object, &persistent);
    object->add_listener_object(&oneShot);

    ASSERT_NE(g_objectListenerTable, nullptr);
    EXPECT_EQ(g_objectListenerTable->listeners.len(), 2);
    EXPECT_NE(object->flags & 0x00001000u, 0u);
    EXPECT_EQ(g_tableAllocations, 1);
    EXPECT_EQ(g_lastHashIndex,
              BML::IVP::Detail::ObjectListenerHashIndex(object));

    IVP_Event_Object event{environment, object};
    DeliverObjectEvent(&event, ObjectEvent::Frozen);

    ASSERT_NE(g_objectListenerTable, nullptr);
    EXPECT_EQ(oneShot.frozen_count, 1);
    EXPECT_EQ(persistent.frozen_count, 1);
    EXPECT_EQ(g_objectListenerTable->listeners.len(), 1);

    DeliverObjectEvent(&event, ObjectEvent::Revived);
    EXPECT_EQ(oneShot.revived_count, 0);
    EXPECT_EQ(persistent.revived_count, 1);

    environment->remove_listener_object_private(object, &persistent);
    EXPECT_EQ(g_objectListenerTable, nullptr);
    EXPECT_EQ(object->flags & 0x00001000u, 0u);
    EXPECT_EQ(g_tableDeletes, 1);
}

} // namespace
