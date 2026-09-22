#include "IvpTestAdapter.h"

#include "BML/IVP/Object.h"
#include "BML/IVP/Templates.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include <string_view>

namespace {

int g_objectConstructCount = 0;
int g_rootConstructCount = 0;
int g_objectDestructCount = 0;
int g_hullConstructCount = 0;
int g_hullDestructCount = 0;
int g_minListConstructCount = 0;
int g_minListDestructCount = 0;
int g_leafDestructCount = 0;
int g_childClusterDestructCount = 0;
bool g_expectPoisonedHullStorage = false;
bool g_sawPoisonedHullStorage = false;

class ClusterAccess : public IVP_Cluster {
public:
    static IVP_Object *&children(IVP_Cluster *cluster) {
        return reinterpret_cast<ClusterAccess *>(cluster)->objects;
    }
};

void InitializeObject(IVP_Object *object, IVP_Environment *environment) {
    object->object_type = IVP_NONE;
    object->next_in_cluster = nullptr;
    object->prev_in_cluster = nullptr;
    object->father_cluster = nullptr;
    object->name = nullptr;
    object->environment = environment;
}

void __fastcall ConstructObject(
    IVP_Object *object, void *, IVP_Cluster *father,
    const IVP_Template_Object *) {
    ++g_objectConstructCount;
    InitializeObject(object, father->get_environment());

    IVP_Object *&head = ClusterAccess::children(father);
    object->next_in_cluster = head;
    if (head)
        head->prev_in_cluster = object;
    head = object;
    object->father_cluster = father;
}

void __fastcall ConstructRootObject(
    IVP_Object *object, void *, IVP_Environment *environment) {
    ++g_rootConstructCount;
    InitializeObject(object, environment);
}

void __fastcall DestructObject(IVP_Object *object, void *) {
    ++g_objectDestructCount;
    IVP_Cluster *father = object->father_cluster;
    if (father) {
        if (object->prev_in_cluster) {
            object->prev_in_cluster->next_in_cluster = object->next_in_cluster;
        } else {
            ClusterAccess::children(father) = object->next_in_cluster;
        }
        if (object->next_in_cluster)
            object->next_in_cluster->prev_in_cluster = object->prev_in_cluster;
    }
    object->next_in_cluster = nullptr;
    object->prev_in_cluster = nullptr;
    object->father_cluster = nullptr;
    object->environment = nullptr;
}

void __fastcall ConstructHullManager(IVP_Hull_Manager_Base *manager, void *) {
    ++g_hullConstructCount;
    if (g_expectPoisonedHullStorage) {
        const auto *bytes = reinterpret_cast<const std::byte *>(manager);
        g_sawPoisonedHullStorage =
            std::all_of(bytes, bytes + sizeof(IVP_Time), [](std::byte value) {
                return value == std::byte{0xA5};
            });
    }
}

void __fastcall DestructHullManager(IVP_Hull_Manager_Base *, void *) {
    ++g_hullDestructCount;
}

void __fastcall ConstructMinList(IVP_U_Min_List *, void *, int) {
    ++g_minListConstructCount;
}

void __fastcall DestructMinList(IVP_U_Min_List *, void *) {
    ++g_minListDestructCount;
}

const BML::IVP::Test::RetailCallBinding kRetailCalls[] = {
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ObjectConstruct, &ConstructObject),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ObjectConstructRoot, &ConstructRootObject),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ObjectDestruct, &DestructObject),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::HullManagerBaseConstruct, &ConstructHullManager),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::HullManagerBaseDestruct, &DestructHullManager),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::MinListConstruct, &ConstructMinList),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::MinListDestruct, &DestructMinList),
};

uintptr_t ResolveRetailCall(std::uint32_t rva) noexcept {
    return BML::IVP::Test::Resolve(rva, kRetailCalls);
}


class RootCluster final : public IVP_Cluster {
public:
    explicit RootCluster(IVP_Environment *environment)
        : IVP_Cluster(environment) {}
};

class TrackedCluster final : public IVP_Cluster {
public:
    TrackedCluster(IVP_Cluster *father, IVP_Template_Cluster *configuration)
        : IVP_Cluster(father, configuration) {}
    ~TrackedCluster() override { ++g_childClusterDestructCount; }
};

class LeafObject final : public IVP_Object {
public:
    LeafObject(IVP_Cluster *father, const IVP_Template_Object *configuration)
        : IVP_Object(father, configuration) {
        object_type = IVP_OBJECT;
    }
    ~LeafObject() override { ++g_leafDestructCount; }
};

void ResetObservations() {
    g_objectConstructCount = 0;
    g_rootConstructCount = 0;
    g_objectDestructCount = 0;
    g_hullConstructCount = 0;
    g_hullDestructCount = 0;
    g_minListConstructCount = 0;
    g_minListDestructCount = 0;
    g_leafDestructCount = 0;
    g_childClusterDestructCount = 0;
}

} // namespace

extern "C" uintptr_t BML_IvpTestResolveRetailCall(
    std::uint32_t rva) noexcept {
    return ResolveRetailCall(rva);
}

namespace {

TEST(IvpObjectHierarchy, CompleteHullConstructorReceivesUntouchedBaseStorage) {
    alignas(IVP_Hull_Manager_Base)
        std::array<std::byte, sizeof(IVP_Hull_Manager_Base)> storage;
    std::fill(storage.begin(), storage.end(), std::byte{0xA5});
    g_expectPoisonedHullStorage = true;
    g_sawPoisonedHullStorage = false;

    auto *manager = ::new (storage.data()) IVP_Hull_Manager_Base;

    EXPECT_TRUE(g_sawPoisonedHullStorage);
    g_expectPoisonedHullStorage = false;
    manager->~IVP_Hull_Manager_Base();
}

TEST(IvpObjectHierarchy,
     NestedClusterOwnsChildrenAndFastObjectUnlinksBeforeTeardown) {
    ResetObservations();

    auto *environment = reinterpret_cast<IVP_Environment *>(0x12345678u);
    {
        RootCluster root(environment);
        IVP_Template_Cluster clusterConfiguration;
        IVP_Template_Object objectConfiguration;

        EXPECT_EQ(root.get_environment(), environment);
        EXPECT_EQ(root.get_type(), IVP_CLUSTER);
        EXPECT_EQ(root.get_first_object_of_cluster(), nullptr);

        auto *child = new TrackedCluster(&root, &clusterConfiguration);
        auto *firstLeaf = new LeafObject(child, &objectConfiguration);
        auto *secondLeaf = new LeafObject(child, &objectConfiguration);

        EXPECT_EQ(root.get_first_object_of_cluster(), child);
        EXPECT_EQ(child->get_first_object_of_cluster(), secondLeaf);
        EXPECT_EQ(child->get_next_object_in_cluster(secondLeaf), firstLeaf);
        EXPECT_EQ(firstLeaf->father_cluster, child);
        EXPECT_EQ(secondLeaf->father_cluster, child);

        {
            IVP_Real_Object_Fast fastObject(&root, &objectConfiguration);
            EXPECT_EQ(root.get_first_object_of_cluster(), &fastObject);
            EXPECT_EQ(fastObject.father_cluster, &root);
            EXPECT_EQ(g_hullConstructCount, 1);
            // The exact Hull Manager constructor owns its embedded min-list.
            // A second compiler-generated min-list construction would leak it.
            EXPECT_EQ(g_minListConstructCount, 0);
        }

        EXPECT_EQ(root.get_first_object_of_cluster(), child);
        EXPECT_EQ(g_hullDestructCount, 1);
        EXPECT_EQ(g_minListDestructCount, 0);
        EXPECT_EQ(g_objectDestructCount, 1);
    }

    EXPECT_EQ(g_rootConstructCount, 1);
    EXPECT_EQ(g_objectConstructCount, 4);
    EXPECT_EQ(g_leafDestructCount, 2);
    EXPECT_EQ(g_childClusterDestructCount, 1);
    EXPECT_EQ(g_objectDestructCount, 5);
    EXPECT_EQ(g_hullConstructCount, 1);
    EXPECT_EQ(g_hullDestructCount, 1);
    EXPECT_EQ(g_minListConstructCount, 0);
    EXPECT_EQ(g_minListDestructCount, 0);
}

} // namespace
