#include "IvpRetailTestSupport.h"

namespace {

class StackRetailSynapse final : public IVP_Synapse {
public:
    StackRetailSynapse() = default;
};

template <typename Object>
void ExpectRetailVptr(const Object *object) {
    const uintptr_t vptr = *reinterpret_cast<const uintptr_t *>(object);
    EXPECT_GE(vptr, gRetailModuleBase);
    EXPECT_LT(vptr, gRetailModuleBase + kExpectedImageSize);
}

__declspec(noinline) void DeleteMaterialThroughRetailVtable(
    IVP_Material *material) {
    delete material;
}

} // namespace

TEST(IvpRetailCrossDllOwnership,
     RetailVptrObjectsAllocateAndDeleteOnTheRetailHeap) {
    RetailImage physics;
    if (!physics.configured()) {
        GTEST_SKIP() << "Set BML_TEST_RETAIL_PHYSICS_DLL to the exact "
                        "Ballance BuildingBlocks\\physics_RT.dll";
    }
    ASSERT_TRUE(physics.valid()) << physics.error();

    // The exact constructors below all replace the wrapper's initial vptr
    // with a table in physics_RT. The corresponding environment callbacks or
    // deleting destructor then release through MSVCRT's operator delete. A
    // caller-side UCRT allocation here would be a real cross-heap mismatch.
    auto *rootMindist = new IVP_Collision_Delegator_Root_Mindist();
    ASSERT_NE(rootMindist, nullptr);
    ExpectRetailVptr(rootMindist);
    rootMindist->environment_is_going_to_be_deleted_event(nullptr);

    IVP_Material *material = new IVP_Material_Simple(0.4, 0.2);
    ASSERT_NE(material, nullptr);
    ExpectRetailVptr(material);
    DeleteMaterialThroughRetailVtable(material);

    auto *materialManager = new IVP_Material_Manager(IVP_TRUE);
    ASSERT_NE(materialManager, nullptr);
    ExpectRetailVptr(materialManager);
    materialManager->environment_will_be_deleted(nullptr);

    auto *limits = new IVP_Anomaly_Limits(IVP_TRUE);
    ASSERT_NE(limits, nullptr);
    ExpectRetailVptr(limits);
    limits->environment_will_be_deleted(nullptr);

    auto *anomalyManager = new IVP_Anomaly_Manager(IVP_TRUE);
    ASSERT_NE(anomalyManager, nullptr);
    ExpectRetailVptr(anomalyManager);
    anomalyManager->environment_will_be_deleted(nullptr);

    auto *rangeManager = new IVP_Range_Manager(nullptr, IVP_TRUE);
    ASSERT_NE(rangeManager, nullptr);
    ExpectRetailVptr(rangeManager);
    rangeManager->environment_will_be_deleted(nullptr);

    // A moving Ballance object keeps its broadphase connector on the object's
    // hull manager. Exercise the retained connector constructor, collision
    // backlink maintenance, first insert/update scheduling paths, and the
    // retail vtable callback that owns deletion at hull-manager teardown.
    alignas(IVP_Environment)
        std::array<std::byte, sizeof(IVP_Environment)> environmentStorage{};
    alignas(IVP_OV_Tree_Manager)
        std::array<std::byte, sizeof(IVP_OV_Tree_Manager)> ovTreeStorage{};
    alignas(IVP_Real_Object)
        std::array<std::byte, sizeof(IVP_Real_Object)> objectStorage{};
    auto *environment = reinterpret_cast<IVP_Environment *>(
        environmentStorage.data());
    auto *ovTree = reinterpret_cast<IVP_OV_Tree_Manager *>(
        ovTreeStorage.data());
    auto *object = reinterpret_cast<IVP_Real_Object *>(
        objectStorage.data());
    environment->ov_tree_manager = ovTree;
    environment->current_time = IVP_Time(7.25);
    object->environment = environment;

    IVP_Hull_Manager hullManager;
    auto *ovElement = new IVP_OV_Element(object);
    ASSERT_NE(ovElement, nullptr);
    ExpectRetailVptr(ovElement);
    EXPECT_EQ(*reinterpret_cast<const uintptr_t *>(ovElement),
              gRetailModuleBase + 0x00063A00u);
    EXPECT_EQ(ovElement->node, nullptr);
    EXPECT_EQ(ovElement->hull_manager, nullptr);
    EXPECT_FLOAT_EQ(ovElement->center.k[0], 0.0f);
    EXPECT_FLOAT_EQ(ovElement->center.k[1], 0.0f);
    EXPECT_FLOAT_EQ(ovElement->center.k[2], 0.0f);
    EXPECT_FLOAT_EQ(ovElement->radius, -1.0f);
    EXPECT_EQ(ovElement->real_object, object);
    EXPECT_EQ(ovElement->collision_fvector.memsize, 16u);
    EXPECT_EQ(ovElement->collision_fvector.len(), 0);
    EXPECT_NE(ovElement->collision_fvector.elems, nullptr);

    IVP_Listener_Hull *hullListener = ovElement;
    EXPECT_EQ(hullListener->get_type(), IVP_HULL_ELEM_OO_CONNECTOR);

    TestCollision earlierCollision;
    TestCollision laterCollision;
    ovElement->add_oo_collision(&earlierCollision);
    ovElement->add_oo_collision(&laterCollision);
    ASSERT_EQ(ovElement->collision_fvector.len(), 2);
    EXPECT_EQ(earlierCollision.get_fvector_index(0), 0);
    EXPECT_EQ(laterCollision.get_fvector_index(0), 1);

    // Removal uses the retail unordered-compaction rule and must repair the
    // surviving collision's reverse index.
    ovElement->remove_oo_collision(&earlierCollision);
    ASSERT_EQ(ovElement->collision_fvector.len(), 1);
    EXPECT_EQ(ovElement->collision_fvector.element_at(0), &laterCollision);
    EXPECT_EQ(earlierCollision.get_fvector_index(0), -1);
    EXPECT_EQ(laterCollision.get_fvector_index(0), 0);
    ovElement->remove_oo_collision(&laterCollision);
    EXPECT_EQ(ovElement->collision_fvector.len(), 0);
    EXPECT_EQ(laterCollision.get_fvector_index(0), -1);

    ovElement->add_to_hull_manager(&hullManager, 2.5);
    ASSERT_EQ(ovElement->hull_manager, &hullManager);
    ASSERT_EQ(hullManager.get_sorted_synapses()->find_min_elem(), ovElement);
    EXPECT_FLOAT_EQ(hullManager.get_sorted_synapses()->find_min_value(),
                    2.5f);
    ovElement->add_to_hull_manager(&hullManager, 4.0);
    EXPECT_EQ(hullManager.get_sorted_synapses()->counter, 1u);
    ASSERT_EQ(hullManager.get_sorted_synapses()->find_min_elem(), ovElement);
    EXPECT_FLOAT_EQ(hullManager.get_sorted_synapses()->find_min_value(),
                    4.0f);

    hullListener->hull_manager_is_going_to_be_deleted_event(&hullManager);
    EXPECT_FALSE(hullManager.get_sorted_synapses()->has_elements());
    EXPECT_EQ(hullManager.get_sorted_synapses()->find_min_elem(), nullptr);

    auto *counter = new IVP_PerformanceCounter_Simple();
    ASSERT_NE(counter, nullptr);
    ExpectRetailVptr(counter);
    counter->environment_is_going_to_be_deleted(nullptr);

    auto *activeManager = new IVP_U_Active_Value_Manager(IVP_TRUE);
    ASSERT_NE(activeManager, nullptr);
    ExpectRetailVptr(activeManager);
    activeManager->environment_will_be_deleted(nullptr);

    auto *filter = new IVP_Collision_Filter_Coll_Group_Ident(IVP_TRUE);
    ASSERT_NE(filter, nullptr);
    ExpectRetailVptr(filter);
    filter->environment_will_be_deleted(nullptr);

    auto *debugManager = new IVP_BetterDebugmanager();
    ASSERT_NE(debugManager, nullptr);
    ExpectRetailVptr(debugManager);
    delete debugManager;

    IVP_U_Float_Hesse liquidPlane;
    liquidPlane.set(0.0f, 1.0f, 0.0f);
    liquidPlane.hesse_val = 0.0f;
    IVP_U_Float_Point current;
    current.set_to_zero();
    auto *liquid = new IVP_Liquid_Surface_Descriptor_Simple(
        &liquidPlane, &current);
    ASSERT_NE(liquid, nullptr);
    ExpectRetailVptr(liquid);

    IVP_Template_Buoyancy buoyancyDefinition;
    IVP_U_Set_Active<IVP_Core> emptyCores(16);
    auto *buoyancyAttacher = new IVP_Attacher_To_Cores_Buoyancy(
        buoyancyDefinition, &emptyCores, liquid);
    ASSERT_NE(buoyancyAttacher, nullptr);
    ExpectRetailVptr(buoyancyAttacher);
    delete buoyancyAttacher;
    delete liquid;

    // Exercise the caller-generated complete destructors as well as retail
    // deleting-destructor dispatch. Each derived layer enters its exact
    // resource-owning or resource-free retail base body exactly once.
    {
        IVP_BetterDebugmanager debug;
        std::vector<char> delegatorEvents;
        RecordingCollisionDelegatorRoot delegator(
            'D', &delegatorEvents, nullptr);
        RecordingCollisionFilter localFilter(IVP_TRUE);
        IVP_Material_Simple localMaterial(0.55, 0.15);
        IVP_PerformanceCounter_Simple localCounter;
        IVP_SurfaceManager_Polygon localSurface(nullptr);
        StackRetailSynapse localSynapse;
        IVP_U_Active_Value localValue("Ballance stack lifetime");
        EXPECT_STREQ(localValue.get_name(), "Ballance stack lifetime");
    }
}

TEST(IvpRetailBroadphase,
     OverlappingMovingObjectsShareABoxAndCleanupTheTree) {
    RetailImage physics;
    if (!physics.configured()) {
        GTEST_SKIP() << "Set BML_TEST_RETAIL_PHYSICS_DLL to the exact "
                        "Ballance BuildingBlocks\\physics_RT.dll";
    }
    ASSERT_TRUE(physics.valid()) << physics.error();

    alignas(IVP_OV_Node)
        std::array<std::byte, sizeof(IVP_OV_Node)> standaloneNodeStorage;
    std::fill(standaloneNodeStorage.begin(), standaloneNodeStorage.end(),
              std::byte{0xA5});
    auto *standaloneNode =
        ::new (standaloneNodeStorage.data()) IVP_OV_Node();
    const auto *standaloneBytes =
        reinterpret_cast<const std::byte *>(standaloneNode);
    EXPECT_TRUE(std::all_of(
        standaloneBytes, standaloneBytes + sizeof(IVP_OV_Node_Data),
        [](std::byte value) { return value == std::byte{0xA5}; }));
    EXPECT_EQ(standaloneNode->parent, nullptr);
    EXPECT_EQ(standaloneNode->children.len(), 0);
    EXPECT_EQ(standaloneNode->elements.len(), 0);
    EXPECT_EQ(standaloneNode->children.elems, nullptr);
    EXPECT_EQ(standaloneNode->elements.elems, nullptr);
    BML::IVP::ABI::InvokeThis<void>(
        BML::IVP::ABI::Address::OVNodeDestruct, standaloneNode);

    auto *tree = new IVP_OV_Tree_Manager();
    ASSERT_NE(tree, nullptr);
    EXPECT_DOUBLE_EQ(RawOVTreeManager(tree).powerlist[39], 0.5);
    EXPECT_DOUBLE_EQ(RawOVTreeManager(tree).powerlist[40], 1.0);
    EXPECT_DOUBLE_EQ(RawOVTreeManager(tree).powerlist[44], 16.0);
    EXPECT_EQ(RawOVTreeManager(tree).root, nullptr);

    alignas(IVP_Environment)
        std::array<std::byte, sizeof(IVP_Environment)> environmentStorage{};
    alignas(IVP_Real_Object)
        std::array<std::byte, sizeof(IVP_Real_Object)> firstObjectStorage{};
    alignas(IVP_Real_Object)
        std::array<std::byte, sizeof(IVP_Real_Object)> secondObjectStorage{};
    auto *environment = reinterpret_cast<IVP_Environment *>(
        environmentStorage.data());
    auto *firstObject = reinterpret_cast<IVP_Real_Object *>(
        firstObjectStorage.data());
    auto *secondObject = reinterpret_cast<IVP_Real_Object *>(
        secondObjectStorage.data());
    environment->ov_tree_manager = tree;
    firstObject->environment = environment;
    secondObject->environment = environment;

    auto *first = new IVP_OV_Element(firstObject);
    auto *second = new IVP_OV_Element(secondObject);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    first->center.set(12.0f, 3.0f, -8.0f);
    second->center.set(&first->center);

    IVP_U_Vector<IVP_OV_Element> collisionCandidates(4);
    EXPECT_DOUBLE_EQ(
        tree->insert_ov_element(first, 1.0, 1.0,
                                &collisionCandidates),
        1.0);
    IVP_OV_Node *sharedNode = RawOVTreeManager(tree).root;
    ASSERT_NE(sharedNode, nullptr);
    EXPECT_EQ(first->node, sharedNode);
    EXPECT_EQ(sharedNode->elements.len(), 1);
    EXPECT_EQ(collisionCandidates.len(), 0);

    EXPECT_DOUBLE_EQ(
        tree->insert_ov_element(second, 1.0, 1.0,
                                &collisionCandidates),
        1.0);
    EXPECT_EQ(second->node, sharedNode);
    EXPECT_EQ(sharedNode->elements.len(), 2);
    ASSERT_EQ(collisionCandidates.len(), 2);
    EXPECT_NE(collisionCandidates.index_of(first), -1);
    EXPECT_NE(collisionCandidates.index_of(second), -1);

    tree->remove_ov_element(second);
    EXPECT_EQ(second->node, nullptr);
    EXPECT_EQ(RawOVTreeManager(tree).root, sharedNode);
    EXPECT_EQ(sharedNode->elements.len(), 1);

    tree->remove_ov_element(first);
    EXPECT_EQ(first->node, nullptr);
    EXPECT_EQ(RawOVTreeManager(tree).root, nullptr);

    // The element destructor repeats the removal request. The retail tree's
    // null-node fast path makes that lifecycle idempotent after Mindist has
    // explicitly removed the broadphase entries.
    collisionCandidates.remove_all();
    delete second;
    delete first;
    delete tree;
}

TEST(IvpRetailByValueAbi,
     TimeValuesDriveTransformInterpolationFreezeClassificationAndCounters) {
    RetailImage physics;
    if (!physics.configured()) {
        GTEST_SKIP() << "Set BML_TEST_RETAIL_PHYSICS_DLL to the exact "
                        "Ballance BuildingBlocks\\physics_RT.dll";
    }
    ASSERT_TRUE(physics.valid()) << physics.error();

    // A moving Core interpolates its Ballance-space transform from an
    // eight-byte IVP_Time passed on the stack. This exercises the public
    // wrapper and the original qword-consuming function, not a test double.
    alignas(IVP_Core) std::array<std::byte, sizeof(IVP_Core)> coreStorage{};
    auto *core = reinterpret_cast<IVP_Core *>(coreStorage.data());
    core->movement_state = IVP_MT_MOVING;
    core->inv_rot_inertia.set(0.25f, 0.5f, 0.75f);
    core->inv_rot_inertia.hesse_val = 0.2f;
    const IVP_U_Float_Point contactArm(1.0f, 2.0f, 3.0f);
    const IVP_DOUBLE worstCaseMass =
        core->calc_virt_mass_worst_case(&contactArm);
    EXPECT_NEAR(worstCaseMass, 1.0 / 5.2, 2.0e-8);
    core->time_of_last_psi = IVP_Time(10.0);
    core->i_delta_time = 0.5f;
    core->pos_world_f_core_last_psi.set(1.0, 2.0, 3.0);
    core->delta_world_f_core_psis.set(4.0f, 6.0f, 8.0f);
    core->q_world_f_core_last_psi.set_identity();
    core->q_world_f_core_next_psi.set_identity();

    IVP_U_Matrix coreMatrix;
    core->calc_at_matrix(IVP_Time(10.25), &coreMatrix);
    EXPECT_DOUBLE_EQ(coreMatrix.get_position()->k[0], 2.0);
    EXPECT_DOUBLE_EQ(coreMatrix.get_position()->k[1], 3.5);
    EXPECT_DOUBLE_EQ(coreMatrix.get_position()->k[2], 5.0);
    EXPECT_DOUBLE_EQ(coreMatrix.get_elem(0, 0), 1.0);
    EXPECT_DOUBLE_EQ(coreMatrix.get_elem(1, 1), 1.0);
    EXPECT_DOUBLE_EQ(coreMatrix.get_elem(2, 2), 1.0);

    // Real Object interpolation consumes the same value ABI and composes the
    // object's Core-space offset. This is the transform queried by gameplay
    // code between PSI ticks.
    alignas(IVP_Real_Object)
        std::array<std::byte, sizeof(IVP_Real_Object)> objectStorage{};
    auto *object = reinterpret_cast<IVP_Real_Object *>(objectStorage.data());
    object->physical_core = core;
    object->flags = 0;
    object->flags.object_movement_state = IVP_MT_MOVING;
    object->flags.collision_detection_enabled = 1u;
    EXPECT_EQ(object->flags.raw & 0x3FFu,
              static_cast<std::uint32_t>(IVP_MT_MOVING) | 0x100u);
    object->q_core_f_object = nullptr;
    object->shift_core_f_object.set(0.5f, 1.0f, -0.5f);

    IVP_U_Matrix objectMatrix;
    object->calc_at_matrix(IVP_Time(10.25), &objectMatrix);
    EXPECT_DOUBLE_EQ(objectMatrix.get_position()->k[0], 2.5);
    EXPECT_DOUBLE_EQ(objectMatrix.get_position()->k[1], 4.5);
    EXPECT_DOUBLE_EQ(objectMatrix.get_position()->k[2], 4.5);

    // The retained calm-state classifier reads the same by-value time and
    // returns the 32-bit public movement enum. Both sides of that boundary
    // are checked with a realistic stationary Core and freeze interval.
    alignas(IVP_Environment)
        std::array<std::byte, sizeof(IVP_Environment)> environmentStorage{};
    auto *environment = reinterpret_cast<IVP_Environment *>(
        environmentStorage.data());
    environment->freeze_manager.freeze_check_dtime = 2.0f;
    core->environment = environment;
    core->upper_limit_radius = 1.0f;
    core->pos_world_f_core_last_psi.set_to_zero();
    core->position_world_f_core_calm_reference[0].set_to_zero();
    core->q_world_f_core_next_psi.set_identity();
    core->q_world_f_core_calm_reference[0] = {0.0f, 0.0f, 0.0f, 1.0f};
    core->time_of_calm_reference[0] = IVP_Time(5.0);
    EXPECT_EQ(core->calc_movement_state(IVP_Time(6.0)), IVP_MT_SLOW);
    EXPECT_EQ(core->calc_movement_state(IVP_Time(8.0)), IVP_MT_CALM);

    // The concrete retail performance counter consumes its time value and
    // commits it as the next reporting-window origin. Non-zero PSI count
    // deliberately takes the real report/reset path instead of its no-op arm.
    alignas(IVP_PerformanceCounter_Simple)
        std::array<std::byte, sizeof(IVP_PerformanceCounter_Simple)>
            counterStorage{};
    auto *counter = reinterpret_cast<IVP_PerformanceCounter_Simple *>(
        counterStorage.data());
    counter->count_PSIs = 1;
    counter->IVP_PerformanceCounter_Simple::
        reset_and_print_performance_counters(IVP_Time(42.125));
    EXPECT_DOUBLE_EQ(counter->time_of_last_reset.get_time(), 42.125);
    EXPECT_EQ(counter->count_PSIs, 0);

    class TimeResetController final : public IVP_Controller {
    public:
        IVP_U_Vector<IVP_Core> *get_associated_controlled_cores() override {
            return nullptr;
        }
        void do_simulation_controller(
            IVP_Event_Sim *, IVP_U_Vector<IVP_Core> *) override {}
        IVP_CONTROLLER_PRIORITY get_controller_priority() override {
            return IVP_CP_NONE;
        }
    } controller;

#if defined(_MSC_VER) && defined(_M_IX86)
    std::uintptr_t stackBefore = 0;
    std::uintptr_t stackAfter = 0;
    __asm mov stackBefore, esp
    controller.IVP_Controller::reset_time(IVP_Time(1234.5));
    __asm mov stackAfter, esp
    EXPECT_EQ(stackAfter, stackBefore);
#else
    controller.IVP_Controller::reset_time(IVP_Time(1234.5));
#endif
}
