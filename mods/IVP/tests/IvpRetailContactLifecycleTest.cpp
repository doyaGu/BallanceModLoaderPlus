#include "IvpRetailTestSupport.h"

namespace {

class PassiveRetailActuator final : public IVP_Actuator {
public:
    PassiveRetailActuator() : IVP_Actuator(nullptr) {}

    void do_simulation_controller(
        IVP_Event_Sim *, IVP_U_Vector<IVP_Core> *) override {}
};

class PointerIdentityVHash final : public IVP_VHash {
public:
    explicit PointerIdentityVHash(int initialSize) : IVP_VHash(initialSize) {}

protected:
    IVP_BOOL compare(void *left, void *right) const override {
        return left == right ? IVP_TRUE : IVP_FALSE;
    }
};

class MatrixSeparationSolver final : public IVP_3D_Solver {
public:
    IVP_Time refine_crossing(
        IVP_Time t0, IVP_Time t1, IVP_DOUBLE value,
        IVP_DOUBLE v0, IVP_DOUBLE v1,
        IVP_Real_Object *objectA, IVP_Real_Object *objectB) {
        return calc_nullstelle(t0, t1, value, v0, v1, objectA, objectB);
    }

protected:
    IVP_DOUBLE get_value(
        IVP_U_Matrix *matrixA, IVP_U_Matrix *matrixB) override {
        return matrixA->vv.k[0] - matrixB->vv.k[0];
    }
};

class RecordingIntSetListener final : public IVP_Listener_Set_Active<int> {
public:
    void element_added(IVP_U_Set_Active<int> *, int *element) override {
        lastAdded = element;
        ++added;
    }
    void element_removed(IVP_U_Set_Active<int> *, int *element) override {
        lastRemoved = element;
        ++removed;
    }
    void pset_is_going_to_be_deleted(IVP_U_Set_Active<int> *) override {
        ++deleted;
    }

    int *lastAdded = nullptr;
    int *lastRemoved = nullptr;
    int added = 0;
    int removed = 0;
    int deleted = 0;
};

class RecordingSpringListener final : public IVP_Listener_Spring {
public:
    RecordingSpringListener(char identity, std::vector<char> *events)
        : identity_(identity), events_(events) {}

    void event_spring_broken(IVP_Actuator_Spring *spring) override {
        last_spring = spring;
        events_->push_back(identity_);
    }

    IVP_Actuator_Spring *last_spring = nullptr;

private:
    char identity_;
    std::vector<char> *events_;
};

class SpringEventAccess : public IVP_Actuator_Spring {
public:
    static void Fire(IVP_Actuator_Spring *spring) {
        reinterpret_cast<SpringEventAccess *>(spring)
            ->fire_event_spring_broken();
    }
};

class StaticSpringListenerVector final
    : public IVP_U_Vector<IVP_Listener_Spring> {
public:
    StaticSpringListenerVector(void **storage, int capacity)
        : IVP_U_Vector<IVP_Listener_Spring>(storage, capacity) {}
};

IVP_Actuator *gExpectedAnchorActuator = nullptr;
IVP_Anchor *gExpectedDeletedAnchor = nullptr;
int gAnchorDeletionNotifications = 0;
alignas(4) std::array<std::byte, sizeof(IVP_U_Vector<IVP_Core>)>
    gEmptySpringCoreVector{};
int gSpringControlledCoreQueries = 0;

void __fastcall RecordAnchorDeletion(
    IVP_Actuator *actuator, void *, IVP_Anchor *anchor) {
    EXPECT_EQ(actuator, gExpectedAnchorActuator);
    EXPECT_EQ(anchor, gExpectedDeletedAnchor);
    ++gAnchorDeletionNotifications;
}

IVP_U_Vector<IVP_Core> *__fastcall ReturnNoSpringControlledCores(
    IVP_Actuator_Spring *, void *) {
    ++gSpringControlledCoreQueries;
    return reinterpret_cast<IVP_U_Vector<IVP_Core> *>(
        gEmptySpringCoreVector.data());
}
class RecordingCollisionListener final : public IVP_Listener_Collision {
public:
    RecordingCollisionListener(
        char identity, std::vector<std::string> *events,
        IVP_Event_Collision *expectedCollision,
        IVP_Event_Friction *expectedFriction,
        int callbacks = IVP_LISTENER_COLLISION_CALLBACK_POST_COLLISION |
                        IVP_LISTENER_COLLISION_CALLBACK_FRICTION)
        : IVP_Listener_Collision(callbacks), identity_(identity),
          events_(events), expected_collision_(expectedCollision),
          expected_friction_(expectedFriction) {}

    void event_post_collision(IVP_Event_Collision *event) override {
        EXPECT_EQ(event, expected_collision_);
        record('P');
    }

    void event_friction_created(IVP_Event_Friction *event) override {
        EXPECT_EQ(event, expected_friction_);
        record('C');
    }

    void event_friction_deleted(IVP_Event_Friction *event) override {
        EXPECT_EQ(event, expected_friction_);
        record('D');
    }

private:
    void record(char kind) {
        events_->emplace_back(std::string{kind, identity_});
    }

    char identity_;
    std::vector<std::string> *events_;
    IVP_Event_Collision *expected_collision_;
    IVP_Event_Friction *expected_friction_;
};

template <typename Function>
Function RetailFunction(HMODULE module, std::uint32_t rva) {
    return reinterpret_cast<Function>(
        reinterpret_cast<std::byte *>(module) + rva);
}

} // namespace

TEST(IvpRetailContactLifecycle,
     OriginalRegistrationDispatchAndRemovalPreserveLifecycleOrder) {
    static_assert(sizeof(void *) == 4,
                  "The Ballance retail physics ABI is 32-bit x86");

    RetailImage physics;
    if (!physics.configured()) {
        GTEST_SKIP() << "Set BML_TEST_RETAIL_PHYSICS_DLL to the exact "
                        "Ballance BuildingBlocks\\physics_RT.dll";
    }
    ASSERT_TRUE(physics.valid()) << physics.error();

    // A mindist owns two 0x1C-byte synapses and links each one to an object by
    // a signed 16-bit back-offset. Exercise the retained attachment primitive
    // without fabricating a complete collision object, and ensure it changes
    // only the backlink fields owned by init_synapse_real.
    {
        alignas(16) std::array<std::byte, 0x100> mindistStorage{};
        auto *synapse = reinterpret_cast<IVP_Synapse *>(
            mindistStorage.data() + 0x20);
        auto *mindist = reinterpret_cast<IVP_Mindist_Base *>(
            mindistStorage.data() + 0x80);
        auto *object = reinterpret_cast<IVP_Real_Object *>(0x13572468u);
        auto *next = reinterpret_cast<IVP_Synapse *>(0x11112222u);
        auto *prev = reinterpret_cast<IVP_Synapse *>(0x33334444u);
        auto *edge = reinterpret_cast<const IVP_Compact_Edge *>(0x55556660u);
        synapse->next = next;
        synapse->prev = prev;
        synapse->edge = edge;
        *reinterpret_cast<std::int16_t *>(
            mindistStorage.data() + 0x20 + 0x1A) = 0x1234;

        synapse->init_synapse_real(mindist, object);

        EXPECT_EQ(synapse->get_synapse_mindist(), mindist);
        EXPECT_EQ(synapse->get_object(), object);
        EXPECT_EQ(synapse->next, next);
        EXPECT_EQ(synapse->prev, prev);
        EXPECT_EQ(synapse->get_edge(), edge);
        EXPECT_EQ(*reinterpret_cast<const std::int16_t *>(
                      mindistStorage.data() + 0x20 + 0x18),
                  0x60);
        EXPECT_EQ(*reinterpret_cast<const std::int16_t *>(
                      mindistStorage.data() + 0x20 + 0x1A),
                  0x1234);
    }

    alignas(16) std::array<std::byte, 0x178> environmentStorage{};
    auto *environment = reinterpret_cast<IVP_Environment *>(
        environmentStorage.data());
    IVP_Contact_Situation situation{};
    std::array<std::byte, 0x88> contactStorage{};
    auto *contact = reinterpret_cast<IVP_Contact_Point *>(
        contactStorage.data());
    IVP_Event_Collision collisionEvent{0.0f, environment, &situation};
    IVP_Event_Friction frictionEvent{environment, &situation, contact};
    std::vector<std::string> events;

    RecordingCollisionListener first(
        '1', &events, &collisionEvent, &frictionEvent);
    RecordingCollisionListener second(
        '2', &events, &collisionEvent, &frictionEvent);
    RecordingCollisionListener disabled(
        'X', &events, &collisionEvent, &frictionEvent, 0);
    std::array<IVP_Listener_Collision *, 3> listeners{};

    *reinterpret_cast<std::uint16_t *>(
        environmentStorage.data() + 0xF4u) =
        static_cast<std::uint16_t>(listeners.size());
    *reinterpret_cast<std::uint16_t *>(
        environmentStorage.data() + 0xF6u) = 0;
    *reinterpret_cast<IVP_Listener_Collision ***>(
        environmentStorage.data() + 0xF8u) = listeners.data();

    using ListenerRegistration = void (__thiscall *)(
        IVP_Environment *, IVP_Listener_Collision *);
    using CollisionDispatcher = void (__thiscall *)(
        IVP_Environment *, IVP_Event_Collision *);
    using FrictionDispatcher = void (__thiscall *)(
        IVP_Environment *, IVP_Event_Friction *);
    const auto frictionCreated = RetailFunction<FrictionDispatcher>(
        physics.get(), kFrictionCreatedDispatcherRva);
    const auto postCollision = RetailFunction<CollisionDispatcher>(
        physics.get(), kPostCollisionDispatcherRva);
    const auto frictionDeleted = RetailFunction<FrictionDispatcher>(
        physics.get(), kFrictionDeletedDispatcherRva);
    const auto addListener = RetailFunction<ListenerRegistration>(
        physics.get(), kAddCollisionListenerRva);

    addListener(environment, &first);
    addListener(environment, &second);
    addListener(environment, &disabled);

    EXPECT_EQ(*reinterpret_cast<const std::uint16_t *>(
                  environmentStorage.data() + 0xF6u),
              3u);
    EXPECT_EQ(listeners[0], &first);
    EXPECT_EQ(listeners[1], &second);
    EXPECT_EQ(listeners[2], &disabled);

    frictionCreated(environment, &frictionEvent);
    postCollision(environment, &collisionEvent);
    frictionDeleted(environment, &frictionEvent);

    EXPECT_EQ(events, (std::vector<std::string>{
        "C2", "C1", "P2", "P1", "D2", "D1"}));

    events.clear();
    environment->remove_listener_collision_global(&second);
    EXPECT_EQ(*reinterpret_cast<const std::uint16_t *>(
                  environmentStorage.data() + 0xF6u),
              2u);
    EXPECT_EQ(listeners[0], &first);
    EXPECT_EQ(listeners[1], &disabled);

    frictionCreated(environment, &frictionEvent);
    postCollision(environment, &collisionEvent);
    frictionDeleted(environment, &frictionEvent);
    EXPECT_EQ(events, (std::vector<std::string>{"C1", "P1", "D1"}));

    events.clear();
    addListener(environment, &second);
    frictionCreated(environment, &frictionEvent);
    postCollision(environment, &collisionEvent);
    frictionDeleted(environment, &frictionEvent);
    EXPECT_EQ(events, (std::vector<std::string>{
        "C2", "C1", "P2", "P1", "D2", "D1"}));

    // The four global object-event bodies survive anonymously in the retail
    // image. Exercise the public API through those exact RVAs, including the
    // mutation case used by gameplay object revive/freeze lifecycles.
    std::array<IVP_Listener_Object *, 2> objectListeners{};
    *reinterpret_cast<std::uint16_t *>(
        environmentStorage.data() + 0x150u) =
        static_cast<std::uint16_t>(objectListeners.size());
    *reinterpret_cast<std::uint16_t *>(
        environmentStorage.data() + 0x152u) = 0;
    *reinterpret_cast<IVP_Listener_Object ***>(
        environmentStorage.data() + 0x154u) = objectListeners.data();

    IVP_Event_Object objectEvent{
        environment, reinterpret_cast<IVP_Real_Object *>(0x24681350u)};
    std::vector<std::string> objectEvents;
    RecordingGlobalObjectListener persistent(
        'A', environment, &objectEvents, &objectEvent);
    RecordingGlobalObjectListener oneShot(
        'B', environment, &objectEvents, &objectEvent, true);

    environment->add_listener_object_global(&persistent);
    environment->install_listener_object_global(&persistent);
    environment->add_listener_object_global(&oneShot);
    ASSERT_EQ(environment->global_object_listeners.len(), 2);
    ASSERT_EQ(objectListeners[0], &persistent);
    ASSERT_EQ(objectListeners[1], &oneShot);

    environment->fire_event_object_created(&objectEvent);
    environment->fire_event_object_frozen(&objectEvent);
    environment->fire_event_object_revived(&objectEvent);
    EXPECT_EQ(environment->global_object_listeners.len(), 1);
    environment->fire_event_object_deleted(&objectEvent);

    EXPECT_EQ(objectEvents, (std::vector<std::string>{
        "CB", "CA", "FB", "FA", "RB", "RA", "DA"}));
    environment->remove_listener_object_global(&persistent);
    EXPECT_EQ(environment->global_object_listeners.len(), 0);

    gRetailModuleBase = 0;
}


TEST(IvpRetailIncrementalLu,
     OriginalDeletionPreservesRemainingCoupledContactMatrix) {
    static_assert(sizeof(void *) == 4,
                  "The Ballance retail physics ABI is 32-bit x86");

    RetailImage physics;
    if (!physics.configured()) {
        GTEST_SKIP() << "Set BML_TEST_RETAIL_PHYSICS_DLL to the exact "
                        "Ballance BuildingBlocks\\physics_RT.dll";
    }
    ASSERT_TRUE(physics.valid()) << physics.error();

    constexpr int stride = 3;
    // The retail row kernels align the first row address down before their
    // vector-FPU clear/copy loops.  Ballance's solver allocator guarantees
    // this alignment; the standalone fixture must preserve that precondition.
    alignas(16) std::array<IVP_DOUBLE, 9> lower{};
    alignas(16) std::array<IVP_DOUBLE, 9> upper{
        4.0, 1.0, 0.5,
        1.0, 3.0, 0.25,
        0.5, 0.25, 2.0,
    };
    alignas(16) std::array<IVP_DOUBLE, 3> input{};
    alignas(16) std::array<IVP_DOUBLE, 3> output{};
    alignas(16) std::array<IVP_DOUBLE, 3> temporary{};
    alignas(16) std::array<IVP_DOUBLE, 3> multiplied{};
    IVP_Incr_L_U_Matrix factors{
        1.0e-9, lower.data(), upper.data(), nullptr, nullptr,
        input.data(), output.data(), temporary.data(), multiplied.data(),
        stride, 3};

    using Decompose = IVP_RETURN_TYPE (__thiscall *)(
        IVP_Incr_L_U_Matrix *);
    using Decrement = IVP_RETURN_TYPE (__thiscall *)(
        IVP_Incr_L_U_Matrix *, int);
    const auto decompose = RetailFunction<Decompose>(
        physics.get(), kIncrementalLuDecomposeRva);
    const auto decrement = RetailFunction<Decrement>(
        physics.get(), kIncrementalLuDecrementRva);

    // Ballance enters the physics step with a clean x87 register stack. The
    // retail LU implementation is x87-only and assumes that host invariant;
    // a standalone test process may inherit occupied x87 stack registers from
    // DLL initialization or test-runtime code.
    _fpreset();
    const IVP_RETURN_TYPE decompositionResult = decompose(&factors);
    ASSERT_EQ(decompositionResult, IVP_OK)
        << "U=" << upper[0] << ',' << upper[1] << ',' << upper[2] << ';'
        << upper[3] << ',' << upper[4] << ',' << upper[5] << ';'
        << upper[6] << ',' << upper[7] << ',' << upper[8]
        << " L=" << lower[0] << ',' << lower[1] << ',' << lower[2] << ';'
        << lower[3] << ',' << lower[4] << ',' << lower[5] << ';'
        << lower[6] << ',' << lower[7] << ',' << lower[8];
    ASSERT_EQ(decrement(&factors, 1), IVP_OK);
    ASSERT_EQ(factors.n_sub, 2);

    // A = inverse(L) * U for the normalized retail factor representation.
    const IVP_DOUBLE l00 = lower[0];
    const IVP_DOUBLE l01 = lower[1];
    const IVP_DOUBLE l10 = lower[stride];
    const IVP_DOUBLE l11 = lower[stride + 1];
    const IVP_DOUBLE determinant = l00 * l11 - l01 * l10;
    ASSERT_GT(std::fabs(determinant), 1.0e-12);
    const std::array<IVP_DOUBLE, 4> inverseLower{
        l11 / determinant, -l01 / determinant,
        -l10 / determinant, l00 / determinant,
    };
    std::array<IVP_DOUBLE, 4> reconstructed{};
    for (int row = 0; row < 2; ++row) {
        for (int column = 0; column < 2; ++column) {
            for (int inner = 0; inner < 2; ++inner) {
                reconstructed[row * 2 + column] +=
                    inverseLower[row * 2 + inner] *
                    upper[inner * stride + column];
            }
        }
    }

    // The retail row kernels use the original solver's float-scale numeric
    // tolerances even though matrix storage is IVP_DOUBLE.
    EXPECT_NEAR(reconstructed[0], 4.0, 1.0e-6);
    EXPECT_NEAR(reconstructed[1], 0.5, 1.0e-6);
    EXPECT_NEAR(reconstructed[2], 0.5, 1.0e-6);
    EXPECT_NEAR(reconstructed[3], 2.0, 1.0e-6);
}

TEST(IvpRetailContactGeometry,
     OriginalVectorBodiesBuildAndProjectAContactFrame) {
    RetailImage physics;
    if (!physics.configured()) {
        GTEST_SKIP() << "Set BML_TEST_RETAIL_PHYSICS_DLL to the exact "
                        "Ballance BuildingBlocks\\physics_RT.dll";
    }
    ASSERT_TRUE(physics.valid()) << physics.error();

    // An embedded local anchor has a staged lifetime: its own retained
    // constructor initializes only rot, then IVP_Constraint_Local assigns the
    // object binding after both 0x88-byte anchor layers exist. Preserve a
    // sentinel binding here so a source reconstruction cannot silently clear
    // a field the parent constructor owns.
    {
        alignas(16) std::array<std::byte,
            sizeof(IVP_Constraint_Local_Anchor)> anchorStorage;
        std::fill(anchorStorage.begin(), anchorStorage.end(), std::byte{0xA5});
        auto *sentinel = reinterpret_cast<IVP_Real_Object *>(0x13572468u);
        auto *oldRotation = reinterpret_cast<IVP_U_Matrix3 *>(0x24681357u);
        std::memcpy(anchorStorage.data() + 0x80, &sentinel, sizeof(sentinel));
        std::memcpy(anchorStorage.data() + 0x84, &oldRotation,
                    sizeof(oldRotation));
        auto *anchor = ::new (anchorStorage.data())
            IVP_Constraint_Local_Anchor();
        EXPECT_EQ(anchor->object, sentinel);
        EXPECT_EQ(anchor->rot, nullptr);
        anchor->~IVP_Constraint_Local_Anchor();
    }

    // Broadphase and ray queries accumulate compact ledges in BigVectors.
    // Force the retained growth body to leave inline/small storage, preserve
    // the existing entry, and expose the retail 2*n+1 capacity rule.
    {
        IVP_U_BigVector<int> nearbyLedges(1);
        int ledges[3]{};
        EXPECT_EQ(nearbyLedges.add(&ledges[0]), 0);
        EXPECT_EQ(nearbyLedges.add(&ledges[1]), 1);
        EXPECT_EQ(nearbyLedges.add(&ledges[2]), 2);
        EXPECT_EQ(nearbyLedges.memsize, 3);
        EXPECT_EQ(nearbyLedges.len(), 3);
        EXPECT_EQ(nearbyLedges.element_at(0), &ledges[0]);
        EXPECT_EQ(nearbyLedges.element_at(1), &ledges[1]);
        EXPECT_EQ(nearbyLedges.element_at(2), &ledges[2]);
    }

    // Core/controller/listener collections use the compact 16-bit IVP vector
    // header.  Grow a one-slot object neighborhood through the retained body
    // and require the retail allocator transition to preserve all identities.
    {
        IVP_U_Vector<int> nearbyObjects(1);
        int objects[3]{};
        EXPECT_EQ(nearbyObjects.add(&objects[0]), 0);
        EXPECT_EQ(nearbyObjects.add(&objects[1]), 1);
        EXPECT_EQ(nearbyObjects.add(&objects[2]), 2);
        EXPECT_EQ(nearbyObjects.memsize, 3);
        EXPECT_EQ(nearbyObjects.len(), 3);
        EXPECT_EQ(nearbyObjects.element_at(0), &objects[0]);
        EXPECT_EQ(nearbyObjects.element_at(1), &objects[1]);
        EXPECT_EQ(nearbyObjects.element_at(2), &objects[2]);
    }

    // The environment owns one Mindist Manager. Build that exact retail
    // submanager, populate its wheel look-ahead collision queue far enough to
    // exercise retained vector growth, and let its retained complete
    // destructor release the queue across the same DLL heap boundary.
    {
        auto *environment =
            reinterpret_cast<IVP_Environment *>(0x13572468u);
        auto *manager = new IVP_Mindist_Manager(environment);
        ASSERT_NE(manager, nullptr);
        EXPECT_EQ(manager->environment, environment);
        EXPECT_EQ(manager->exact_mindists, nullptr);
        EXPECT_EQ(manager->invalid_mindists, nullptr);
        EXPECT_EQ(manager->wheel_look_ahead_mindists.len(), 0);

        auto *first = reinterpret_cast<IVP_Mindist *>(0x24681000u);
        auto *second = reinterpret_cast<IVP_Mindist *>(0x24682000u);
        auto *third = reinterpret_cast<IVP_Mindist *>(0x24683000u);
        EXPECT_EQ(manager->wheel_look_ahead_mindists.add(first), 0);
        EXPECT_EQ(manager->wheel_look_ahead_mindists.add(second), 1);
        EXPECT_EQ(manager->wheel_look_ahead_mindists.add(third), 2);
        EXPECT_EQ(manager->wheel_look_ahead_mindists.memsize, 3);
        EXPECT_EQ(manager->wheel_look_ahead_mindists.element_at(0), first);
        EXPECT_EQ(manager->wheel_look_ahead_mindists.element_at(1), second);
        EXPECT_EQ(manager->wheel_look_ahead_mindists.element_at(2), third);
        delete manager;
    }

    // The contact and constraint solvers allocate one-frame state from the
    // environment's transaction arena. Exercise the retained allocator with
    // realistic small solver records, force a second block with a large
    // matrix workspace, then require transaction rollback to reuse the first
    // block. Construction, expansion, rollback and destruction all execute in
    // the exact retail DLL and therefore cover both behavior and heap origin.
    {
        auto *memory = new IVP_U_Memory();
        ASSERT_NE(memory, nullptr);
        memory->init_mem_transaction_usage();
        memory->start_memory_transaction();

        auto *contactState = static_cast<std::byte *>(
            memory->get_mem_transaction(96));
        ASSERT_NE(contactState, nullptr);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(contactState) & 0x1Fu,
                  0u);
        std::fill(contactState, contactState + 96, std::byte{0x7B});

        auto *zeroedConstraintState = static_cast<std::byte *>(
            memory->get_memc(64));
        ASSERT_NE(zeroedConstraintState, nullptr);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(
                      zeroedConstraintState) & 0x1Fu,
                  0u);
        EXPECT_TRUE(std::all_of(
            zeroedConstraintState, zeroedConstraintState + 64,
            [](std::byte value) { return value == std::byte{0}; }));

        void *largeMatrixWorkspace =
            memory->get_mem_transaction(0x9000u);
        ASSERT_NE(largeMatrixWorkspace, nullptr);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(largeMatrixWorkspace) &
                      0x1Fu,
                  0u);
        EXPECT_NE(largeMatrixWorkspace, contactState);

        memory->end_memory_transaction();
        memory->start_memory_transaction();
        EXPECT_EQ(memory->get_mem_transaction(96), contactState);
        memory->end_memory_transaction();
        delete memory;

        // The same arena API also accepts caller-owned scratch storage. The
        // retained destructor must leave that first block alone while still
        // resetting transaction state and serving aligned solver records.
        alignas(32) std::array<std::byte, 4096> externalScratch;
        std::fill(externalScratch.begin(), externalScratch.end(),
                  std::byte{0xA5});
        {
            IVP_U_Memory externalMemory;
            externalMemory.init_mem_transaction_usage(
                reinterpret_cast<char *>(externalScratch.data()),
                static_cast<int>(externalScratch.size()));
            externalMemory.start_memory_transaction();
            auto *externalState = static_cast<std::byte *>(
                externalMemory.get_memc(128));
            ASSERT_NE(externalState, nullptr);
            EXPECT_EQ(reinterpret_cast<std::uintptr_t>(externalState) &
                          0x1Fu,
                      0u);
            EXPECT_GE(externalState, externalScratch.data());
            EXPECT_LE(externalState + 128, externalScratch.data() +
                                              externalScratch.size());
            EXPECT_TRUE(std::all_of(
                externalState, externalState + 128,
                [](std::byte value) { return value == std::byte{0}; }));
            externalMemory.end_memory_transaction();
        }
    }

    // Ballance emits the allocated compact-vector constructor as the shared
    // private body at RVA 0x24C20.  Build a collision-neighborhood-sized list
    // through that exact body, use the public operations, and release its
    // allocation through the same retail heap boundary.
    {
        alignas(IVP_U_Vector<int>)
            std::array<std::byte, sizeof(IVP_U_Vector<int>)> vectorStorage{};
        auto *retailVector = reinterpret_cast<IVP_U_Vector<int> *>(
            vectorStorage.data());
        using RetailVectorConstructor =
            void (__thiscall *)(IVP_U_Vector_Base *, int);
        const auto retailVectorConstructor =
            RetailFunction<RetailVectorConstructor>(
                physics.get(), kVectorAllocatedConstructorInlineRva);
        ASSERT_NE(retailVectorConstructor, nullptr);
        retailVectorConstructor(retailVector, 2);

        int collisionCandidates[2]{};
        EXPECT_EQ(retailVector->memsize, 2);
        EXPECT_EQ(retailVector->len(), 0);
        ASSERT_NE(retailVector->elems, nullptr);
        EXPECT_EQ(retailVector->add(&collisionCandidates[0]), 0);
        EXPECT_EQ(retailVector->add(&collisionCandidates[1]), 1);
        EXPECT_EQ(retailVector->element_at(0), &collisionCandidates[0]);
        EXPECT_EQ(retailVector->element_at(1), &collisionCandidates[1]);
        retailVector->~IVP_U_Vector<int>();
    }

    // Fixed-capacity controller vectors keep their this+1 storage across a
    // public clear().  Losing that pointer/capacity would make the next Core
    // registration allocate needlessly and would break the retail small-
    // vector ownership test used by increment_mem().
    {
        IVP_Vector_of_Cores_2 controlledCores;
        void **inlineStorage = controlledCores.elems;
        auto *firstCore = reinterpret_cast<IVP_Core *>(0x13571000u);
        auto *secondCore = reinterpret_cast<IVP_Core *>(0x24682000u);
        EXPECT_EQ(controlledCores.add(firstCore), 0);
        EXPECT_EQ(controlledCores.add(secondCore), 1);
        controlledCores.clear();
        EXPECT_EQ(controlledCores.len(), 0);
        EXPECT_EQ(controlledCores.memsize, 2);
        EXPECT_EQ(controlledCores.elems, inlineStorage);
        EXPECT_EQ(controlledCores.add(secondCore), 0);
        EXPECT_EQ(controlledCores.element_at(0), secondCore);
    }

    // Collision events can belong to two fast vectors simultaneously.  Test
    // both reverse-index slots, then compact one list and reorder the other;
    // stale indices here would corrupt friction/mindist deletion in Ballance.
    {
        IVP_U_FVector<TestCollision> activeMindists(1);
        IVP_U_FVector<TestCollision> delegatedMindists(1);
        TestCollision entries[3];

        EXPECT_EQ(activeMindists.add(&entries[0]), 0);
        EXPECT_EQ(activeMindists.add(&entries[1]), 1);
        EXPECT_EQ(activeMindists.add(&entries[2]), 2);
        EXPECT_EQ(delegatedMindists.add(&entries[1]), 0);
        EXPECT_EQ(delegatedMindists.add(&entries[0]), 1);
        EXPECT_EQ(activeMindists.memsize, 3);
        EXPECT_EQ(activeMindists.len(), 3);
        EXPECT_EQ(activeMindists.index_of(&entries[1]), 1);
        EXPECT_EQ(delegatedMindists.index_of(&entries[1]), 0);

        activeMindists.remove_allow_resort(&entries[1]);
        EXPECT_EQ(activeMindists.len(), 2);
        EXPECT_EQ(activeMindists.element_at(1), &entries[2]);
        EXPECT_EQ(activeMindists.index_of(&entries[2]), 1);
        EXPECT_EQ(activeMindists.index_of(&entries[1]), -1);
        EXPECT_EQ(delegatedMindists.index_of(&entries[1]), 0);

        delegatedMindists.swap_elems(0, 1);
        EXPECT_EQ(delegatedMindists.element_at(0), &entries[0]);
        EXPECT_EQ(delegatedMindists.element_at(1), &entries[1]);
        EXPECT_EQ(delegatedMindists.index_of(&entries[0]), 0);
        EXPECT_EQ(delegatedMindists.index_of(&entries[1]), 1);
    }

    // Collision-side registries use IVP_VHash with an owner-defined equality
    // predicate. Put three distinct objects in one hash chain so the retail
    // add path must grow 4 -> 8, then remove the middle identity and require
    // the displaced probe chain to remain searchable.
    {
        PointerIdentityVHash objectRegistry(4);
        int objects[3]{};
        const auto collisionHash = static_cast<unsigned int>(
            IVP_VHash::fast_hash_index(0x42));
        objectRegistry.add_elem(&objects[0], collisionHash);
        objectRegistry.add_elem(&objects[1], collisionHash);
        objectRegistry.add_elem(&objects[2], collisionHash);
        EXPECT_EQ(objectRegistry.len(), 8);
        EXPECT_EQ(objectRegistry.n_elems(), 3);
        EXPECT_EQ(objectRegistry.find_elem(&objects[0], collisionHash),
                  &objects[0]);
        EXPECT_EQ(objectRegistry.find_elem(&objects[1], collisionHash),
                  &objects[1]);
        EXPECT_EQ(objectRegistry.find_elem(&objects[2], collisionHash),
                  &objects[2]);
        EXPECT_EQ(objectRegistry.remove_elem(&objects[1], collisionHash),
                  &objects[1]);
        EXPECT_EQ(objectRegistry.n_elems(), 2);
        EXPECT_EQ(objectRegistry.find_elem(&objects[0], collisionHash),
                  &objects[0]);
        EXPECT_EQ(objectRegistry.find_elem(&objects[1], collisionHash),
                  nullptr);
        EXPECT_EQ(objectRegistry.find_elem(&objects[2], collisionHash),
                  &objects[2]);
    }

    // Compact builders and the Mindist minimizer use fixed-width binary keys,
    // not C strings. Exercise exact retail construction/add/find with keys
    // containing zero bytes, then require the selectively reconstructed remove
    // path to compute the same CRC bucket and preserve the collision chain.
    {
        int missing = -1;
        int firstValue = 101;
        int secondValue = 202;
        int thirdValue = 303;
        const std::array<unsigned char, 4> firstKey{0x31, 0x00, 0x42, 0x7F};
        const std::array<unsigned char, 4> secondKey{0x31, 0x00, 0x43, 0x7F};
        const std::array<unsigned char, 4> thirdKey{0x31, 0x00, 0x44, 0x7F};
        IVP_Hash binaryRegistry(1, 4, &missing);
        binaryRegistry.add(
            reinterpret_cast<const char *>(firstKey.data()), &firstValue);
        binaryRegistry.add(
            reinterpret_cast<const char *>(secondKey.data()), &secondValue);
        binaryRegistry.add(
            reinterpret_cast<const char *>(thirdKey.data()), &thirdValue);
        EXPECT_EQ(binaryRegistry.find(
                      reinterpret_cast<const char *>(firstKey.data())),
                  &firstValue);
        EXPECT_EQ(binaryRegistry.find(
                      reinterpret_cast<const char *>(secondKey.data())),
                  &secondValue);
        EXPECT_EQ(binaryRegistry.find(
                      reinterpret_cast<const char *>(thirdKey.data())),
                  &thirdValue);

        binaryRegistry.remove(
            reinterpret_cast<const char *>(secondKey.data()));
        EXPECT_EQ(binaryRegistry.find(
                      reinterpret_cast<const char *>(secondKey.data())),
                  &missing);
        EXPECT_EQ(binaryRegistry.find(
                      reinterpret_cast<const char *>(firstKey.data())),
                  &firstValue);
        EXPECT_EQ(binaryRegistry.find(
                      reinterpret_cast<const char *>(thirdKey.data())),
                  &thirdValue);
    }

    // Phantom and forcefield membership is built on the same retail VHash,
    // first as an identity set and then as an active set with ordered listener
    // notifications. Duplicate installation must remain silent.
    {
        int cores[2]{};
        IVP_U_Set<int> coreSet(4);
        coreSet.add_element(&cores[0]);
        coreSet.install_element(&cores[0]);
        coreSet.install_element(&cores[1]);
        EXPECT_EQ(coreSet.n_elems(), 2);
        EXPECT_EQ(coreSet.find_element(&cores[0]), &cores[0]);
        EXPECT_EQ(coreSet.find_element(&cores[1]), &cores[1]);
        coreSet.remove_element(&cores[0]);
        EXPECT_EQ(coreSet.n_elems(), 1);
        EXPECT_EQ(coreSet.find_element(&cores[0]), nullptr);
        EXPECT_EQ(coreSet.find_element(&cores[1]), &cores[1]);
    }
    {
        int cores[2]{};
        RecordingIntSetListener listener;
        {
            IVP_U_Set_Active<int> activeCores(4);
            activeCores.add_listener_set_active(&listener);
            activeCores.add_element(&cores[0]);
            activeCores.install_element(&cores[0]);
            activeCores.install_element(&cores[1]);
            EXPECT_EQ(listener.added, 2);
            EXPECT_EQ(listener.lastAdded, &cores[1]);
            activeCores.remove_element(&cores[0]);
            EXPECT_EQ(listener.removed, 1);
            EXPECT_EQ(listener.lastRemoved, &cores[0]);
            EXPECT_EQ(activeCores.find_element(&cores[0]), nullptr);
            EXPECT_EQ(activeCores.find_element(&cores[1]), &cores[1]);
        }
        EXPECT_EQ(listener.deleted, 1);
    }

    IVP_U_Float_Point triangle0;
    IVP_U_Float_Point triangle1;
    IVP_U_Float_Point triangle2;
    triangle0.set(0.0f, 0.0f, 0.0f);
    triangle1.set(2.0f, 0.0f, 0.0f);
    triangle2.set(0.0f, 0.0f, 2.0f);

    IVP_U_Point contactNormal;
    contactNormal.inline_set_vert_to_area_defined_by_three_points(
        &triangle0, &triangle1, &triangle2);
    EXPECT_NEAR(contactNormal.real_length_plus_normize(), 4.0, 1.0e-12);
    EXPECT_NEAR(contactNormal.k[0], 0.0, 1.0e-12);
    EXPECT_NEAR(contactNormal.k[1], 1.0, 1.0e-12);
    EXPECT_NEAR(contactNormal.k[2], 0.0, 1.0e-12);

    IVP_U_Float_Hesse supportPlane;
    supportPlane.set(0.0f, 1.0f, 0.0f);
    IVP_U_Float_Point supportPoint;
    supportPoint.set(0.0f, 3.0f, 0.0f);
    supportPlane.calc_hesse_val(&supportPoint);

    IVP_U_Float_Point candidate;
    candidate.set(2.0f, 7.0f, -1.0f);
    IVP_U_Float_Point projected;
    supportPlane.proj_on_plane(&candidate, &projected);
    EXPECT_NEAR(projected.k[0], 2.0f, 1.0e-6f);
    EXPECT_NEAR(projected.k[1], 3.0f, 1.0e-6f);
    EXPECT_NEAR(projected.k[2], -1.0f, 1.0e-6f);
    EXPECT_NEAR(supportPlane.get_dist(&projected), 0.0, 1.0e-6);

    // Build the double-precision support plane used by contact and impact
    // solvers.  Scaling all four coefficients must not change the plane;
    // the retained normalizer restores a unit normal before projection.
    const IVP_U_Point planePoint0(0.0, 3.0, 0.0);
    const IVP_U_Point planePoint1(0.0, 3.0, 2.0);
    const IVP_U_Point planePoint2(2.0, 3.0, 0.0);
    IVP_U_Hesse contactPlane;
    _fpreset();
    contactPlane.calc_hesse(&planePoint0, &planePoint1, &planePoint2);
    EXPECT_NEAR(contactPlane.k[0], 0.0, 1.0e-12);
    EXPECT_NEAR(contactPlane.k[1], -4.0, 1.0e-12);
    EXPECT_NEAR(contactPlane.k[2], 0.0, 1.0e-12);
    EXPECT_NEAR(contactPlane.hesse_val, 12.0, 1.0e-12);
    contactPlane.mult_hesse(5.0);
    _fpreset();
    contactPlane.normize();
    EXPECT_NEAR(contactPlane.k[1], -1.0, 1.0e-12);
    EXPECT_NEAR(contactPlane.hesse_val, 3.0, 1.0e-12);
    IVP_U_Point doubleCandidate(2.0, 7.0, -1.0);
    IVP_U_Point doubleProjected;
    contactPlane.proj_on_plane(&doubleCandidate, &doubleProjected);
    EXPECT_NEAR(doubleProjected.k[0], 2.0, 1.0e-12);
    EXPECT_NEAR(doubleProjected.k[1], 3.0, 1.0e-12);
    EXPECT_NEAR(doubleProjected.k[2], -1.0, 1.0e-12);
    EXPECT_NEAR(contactPlane.get_dist(&doubleProjected), 0.0, 1.0e-12);

    IVP_U_Point previous(1.0, 3.0, -2.0);
    IVP_U_Point next(5.0, 3.0, 2.0);
    IVP_U_Point midpoint;
    midpoint.set_interpolate(&previous, &next, 0.5);
    EXPECT_NEAR(midpoint.k[0], 3.0, 1.0e-12);
    EXPECT_NEAR(midpoint.k[1], 3.0, 1.0e-12);
    EXPECT_NEAR(midpoint.k[2], 0.0, 1.0e-12);

    IVP_U_Matrix3 contactBasis;
    contactBasis.init3();
    IVP_U_Point tangent(1.0, 0.0, 0.0);
    contactBasis.set_col(IVP_INDEX_X, &tangent);
    EXPECT_NEAR(contactBasis.rows[0].k[0], 1.0, 1.0e-12);
    EXPECT_NEAR(contactBasis.rows[1].k[0], 0.0, 1.0e-12);
    EXPECT_NEAR(contactBasis.rows[2].k[0], 0.0, 1.0e-12);

    // Compose the same two frames used when Ballance maps an object's local
    // geometry through its Core into world space.  The source-reconstructed
    // inline product must agree element-for-element with the retained retail
    // body, then the retail point transform and inverse must round-trip.
    IVP_U_Matrix worldFromObject;
    worldFromObject.init_rotated3(IVP_INDEX_Z, -0.22f);
    worldFromObject.vv.set(10.0, -4.0, 1.0);
    IVP_U_Matrix objectFromCore;
    objectFromCore.init_rotated3(IVP_INDEX_Y, 0.37f);
    objectFromCore.vv.set(2.0, 3.0, -1.0);

    IVP_U_Matrix reconstructedWorldFromCore;
    worldFromObject.inline_mmult4(
        &objectFromCore, &reconstructedWorldFromCore);
    IVP_U_Matrix retailWorldFromCore;
    _fpreset();
    worldFromObject.mmult4(&objectFromCore, &retailWorldFromCore);
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            EXPECT_NEAR(retailWorldFromCore.get_elem(row, column),
                        reconstructedWorldFromCore.get_elem(row, column),
                        1.0e-12);
        }
    }
    for (int component = 0; component < 3; ++component) {
        EXPECT_NEAR(retailWorldFromCore.vv.k[component],
                    reconstructedWorldFromCore.vv.k[component], 1.0e-12);
    }

    const IVP_U_Point corePoint(0.5, -0.25, 1.25);
    IVP_U_Point worldPoint;
    retailWorldFromCore.vmult4(&corePoint, &worldPoint);
    IVP_U_Point recoveredCorePoint;
    retailWorldFromCore.vimult4(&worldPoint, &recoveredCorePoint);
    for (int component = 0; component < 3; ++component) {
        EXPECT_NEAR(recoveredCorePoint.k[component], corePoint.k[component],
                    1.0e-7);
    }

    // Store a world-space attachment point through Ballance's retained
    // template-anchor body.  This is the configuration path used before a
    // spring, suspension, or two-point actuator turns the point into its
    // object/Core-space runtime anchor.  Check the object identity and the
    // complete aligned four-double point, including its Hesse lane.
    {
        IVP_Template_Anchor templateAnchor;
        IVP_Real_Object *anchoredObject =
            reinterpret_cast<IVP_Real_Object *>(0x13572468u);
        IVP_U_Point attachmentPoint(12.5, -3.25, 7.75);
        attachmentPoint.hesse_val = 0.625;
        templateAnchor.set_anchor_position_ws(
            anchoredObject, &attachmentPoint);

        EXPECT_EQ(templateAnchor.get_object(), anchoredObject);
        const IVP_U_Point *storedPoint =
            templateAnchor.get_anchor_position_ws();
        ASSERT_NE(storedPoint, nullptr);
        EXPECT_NE(storedPoint, &attachmentPoint);
        EXPECT_DOUBLE_EQ(storedPoint->k[0], 12.5);
        EXPECT_DOUBLE_EQ(storedPoint->k[1], -3.25);
        EXPECT_DOUBLE_EQ(storedPoint->k[2], 7.75);
        EXPECT_DOUBLE_EQ(storedPoint->hesse_val, 0.625);
    }

    // Assemble the template state used for a breakable two-point spring, such
    // as a suspended platform or constrained level mechanism.  Both retained
    // constructors are called from the mandatory DLL: the base establishes the
    // two attachment slots and the Spring body establishes Ballance's exact
    // 0x38 defaults, including its large no-break threshold.
    {
        IVP_Template_Two_Point twoPoint;
        EXPECT_EQ(twoPoint.client_data, nullptr);
        EXPECT_EQ(twoPoint.anchors[0], nullptr);
        EXPECT_EQ(twoPoint.anchors[1], nullptr);

        IVP_Template_Anchor anchor0;
        IVP_Template_Anchor anchor1;
        twoPoint.client_data = reinterpret_cast<void *>(0x24681357u);
        twoPoint.anchors[0] = &anchor0;
        twoPoint.anchors[1] = &anchor1;
        EXPECT_EQ(twoPoint.client_data,
                  reinterpret_cast<void *>(0x24681357u));
        EXPECT_EQ(twoPoint.anchors[0], &anchor0);
        EXPECT_EQ(twoPoint.anchors[1], &anchor1);

        IVP_Template_Spring spring;
        EXPECT_EQ(spring.client_data, nullptr);
        EXPECT_EQ(spring.anchors[0], nullptr);
        EXPECT_EQ(spring.anchors[1], nullptr);
        EXPECT_FLOAT_EQ(spring.spring_len, 0.0f);
        EXPECT_EQ(spring.spring_values_are_relative, IVP_FALSE);
        EXPECT_FLOAT_EQ(spring.spring_constant, 0.0f);
        EXPECT_FLOAT_EQ(spring.spring_damp, 0.0f);
        EXPECT_FLOAT_EQ(spring.rel_pos_damp, 0.0f);
        EXPECT_EQ(spring.max_len_exceed_type, IVP_SFE_NONE);
        EXPECT_FLOAT_EQ(spring.break_max_len, 1.0e20f);
        EXPECT_EQ(spring.active_float_spring_len, nullptr);
        EXPECT_EQ(spring.active_float_spring_constant, nullptr);
        EXPECT_EQ(spring.active_float_spring_damp, nullptr);
        EXPECT_EQ(spring.active_float_spring_rel_pos_damp, nullptr);

        spring.client_data = reinterpret_cast<void *>(0x13572468u);
        spring.anchors[0] = &anchor0;
        spring.anchors[1] = &anchor1;
        spring.spring_len = 2.25f;
        spring.spring_values_are_relative = IVP_TRUE;
        spring.spring_constant = 18.0f;
        spring.spring_damp = 0.35f;
        spring.rel_pos_damp = 0.10f;
        spring.max_len_exceed_type = IVP_SFE_BREAK;
        spring.break_max_len = 4.5f;

        EXPECT_EQ(spring.anchors[0], &anchor0);
        EXPECT_EQ(spring.anchors[1], &anchor1);
        EXPECT_FLOAT_EQ(spring.spring_len, 2.25f);
        EXPECT_EQ(spring.spring_values_are_relative, IVP_TRUE);
        EXPECT_FLOAT_EQ(spring.spring_constant, 18.0f);
        EXPECT_FLOAT_EQ(spring.spring_damp, 0.35f);
        EXPECT_FLOAT_EQ(spring.rel_pos_damp, 0.10f);
        EXPECT_EQ(spring.max_len_exceed_type, IVP_SFE_BREAK);
        EXPECT_FLOAT_EQ(spring.break_max_len, 4.5f);

        // Exercise the retained runtime tuning bodies without inventing a
        // Spring constructor fixture. Their wake path asks the actuator for
        // controlled cores; an empty but ABI-correct vector models a detached
        // level mechanism and makes the retail controller manager stop before
        // any Simulation Unit traversal.
        alignas(4) std::array<
            std::byte,
            sizeof(IVP_Actuator_Spring) + 2 * sizeof(void *)>
            actuatorStorage{};
        alignas(4) std::array<std::byte, 0x1C> objectStorage{};
        alignas(4) std::array<std::byte, 0xA4> environmentStorage{};
        std::array<std::uintptr_t, 3> actuatorVtable{};
        actuatorVtable[2] = reinterpret_cast<std::uintptr_t>(
            &ReturnNoSpringControlledCores);

        auto *runtimeSpring = reinterpret_cast<IVP_Actuator_Spring *>(
            actuatorStorage.data());
        *reinterpret_cast<std::uintptr_t **>(actuatorStorage.data()) =
            actuatorVtable.data();
        *reinterpret_cast<void **>(actuatorStorage.data() + 0x14) =
            objectStorage.data();
        *reinterpret_cast<void **>(objectStorage.data() + 0x18) =
            environmentStorage.data();
        *reinterpret_cast<void **>(environmentStorage.data() + 0xA0) =
            environmentStorage.data();
        *reinterpret_cast<IVP_FLOAT *>(actuatorStorage.data() + 0x78) = 1.5f;

        gSpringControlledCoreQueries = 0;
        runtimeSpring->set_len(2.75);
        runtimeSpring->set_constant(12.0);
        runtimeSpring->set_damp(0.4);
        runtimeSpring->set_rel_pos_damp(0.2);

        EXPECT_FLOAT_EQ(runtimeSpring->get_spring_length_zero_force(), 2.75f);
        EXPECT_FLOAT_EQ(runtimeSpring->get_constant(), 18.0f);
        EXPECT_FLOAT_EQ(runtimeSpring->get_damp_factor(), 0.6f);
        EXPECT_FLOAT_EQ(runtimeSpring->get_rel_pos_damp(), 0.3f);
        EXPECT_EQ(gSpringControlledCoreQueries, 4);
        EXPECT_EQ(runtimeSpring->get_only_stretch(), IVP_FALSE);

        // A broken platform spring notifies the most recently registered
        // mechanism first. Exercise the exact RVA 0x14220 body over the
        // confirmed +0x90 listener-vector layout rather than duplicating its
        // dispatch loop in the test.
        std::vector<char> springEvents;
        RecordingSpringListener firstSpringListener('A', &springEvents);
        RecordingSpringListener secondSpringListener('B', &springEvents);
        auto *springListeners = ::new (
            actuatorStorage.data() + 0x90)
            StaticSpringListenerVector(
                reinterpret_cast<void **>(
                    actuatorStorage.data() + sizeof(IVP_Actuator_Spring)),
                2);
        springListeners->add(&firstSpringListener);
        springListeners->add(&secondSpringListener);
        SpringEventAccess::Fire(runtimeSpring);
        EXPECT_EQ(springEvents, (std::vector<char>{'B', 'A'}));
        EXPECT_EQ(firstSpringListener.last_spring, runtimeSpring);
        EXPECT_EQ(secondSpringListener.last_spring, runtimeSpring);
        springListeners->~StaticSpringListenerVector();
    }

    // Configure a bounded translation/twist joint with the retained Ballance
    // constraint-template constructor and limit writers.  This is a useful
    // pre-creation state: X may travel inside a range, Y/Z stay fixed, twist
    // around Z is bounded, and the other rotations are fixed explicitly.
    {
        IVP_Template_Constraint joint;
        EXPECT_EQ(joint.objectR, nullptr);
        EXPECT_EQ(joint.objectA, nullptr);
        EXPECT_EQ(joint.m_Ros_f_Rfs, nullptr);
        EXPECT_EQ(joint.m_Ros_f_Rrs, nullptr);
        EXPECT_EQ(joint.m_Aos_f_Afs, nullptr);
        EXPECT_FLOAT_EQ(joint.force_factor, 1.0f);
        EXPECT_FLOAT_EQ(joint.damp_factor, 1.0f);
        EXPECT_FLOAT_EQ(joint.limited_axis_stiffness, 0.3f);

        for (int axis = 0; axis < 3; ++axis)
            EXPECT_EQ(joint.axis_type[axis], IVP_CONSTRAINT_AXIS_FIXED);
        for (int axis = 3; axis < 6; ++axis)
            EXPECT_EQ(joint.axis_type[axis], IVP_CONSTRAINT_AXIS_FREE);

        joint.limit_translation_axis(IVP_INDEX_X, -1.25f, 2.5f);
        joint.fix_rotation_axis(IVP_INDEX_X);
        joint.fix_rotation_axis(IVP_INDEX_Y);
        joint.limit_rotation_axis(IVP_INDEX_Z, -0.5f, 0.75f);
        joint.set_max_translation_impulse(IVP_CFE_BREAK, 50.0f);
        joint.set_max_rotation_impulse(IVP_CFE_CLIP, 10.0f);

        EXPECT_EQ(joint.axis_type[IVP_TR_INDEX_TX],
                  IVP_CONSTRAINT_AXIS_LIMITED);
        EXPECT_FLOAT_EQ(joint.borderleft_Rfs[IVP_TR_INDEX_TX], -1.25f);
        EXPECT_FLOAT_EQ(joint.borderright_Rfs[IVP_TR_INDEX_TX], 2.5f);
        EXPECT_EQ(joint.axis_type[IVP_TR_INDEX_TY],
                  IVP_CONSTRAINT_AXIS_FIXED);
        EXPECT_EQ(joint.axis_type[IVP_TR_INDEX_TZ],
                  IVP_CONSTRAINT_AXIS_FIXED);
        EXPECT_EQ(joint.axis_type[IVP_TR_INDEX_RX],
                  IVP_CONSTRAINT_AXIS_FIXED);
        EXPECT_EQ(joint.axis_type[IVP_TR_INDEX_RY],
                  IVP_CONSTRAINT_AXIS_FIXED);
        EXPECT_EQ(joint.axis_type[IVP_TR_INDEX_RZ],
                  IVP_CONSTRAINT_AXIS_LIMITED);
        EXPECT_FLOAT_EQ(joint.borderleft_Rfs[IVP_TR_INDEX_RZ], -0.5f);
        EXPECT_FLOAT_EQ(joint.borderright_Rfs[IVP_TR_INDEX_RZ], 0.75f);
        for (int axis = 0; axis < 3; ++axis) {
            EXPECT_EQ(joint.maximpulse_type[axis], IVP_CFE_BREAK);
            EXPECT_FLOAT_EQ(joint.maximpulse[axis], 50.0f);
        }
        for (int axis = 3; axis < 6; ++axis) {
            EXPECT_EQ(joint.maximpulse_type[axis], IVP_CFE_CLIP);
            EXPECT_FLOAT_EQ(joint.maximpulse[axis], 10.0f);
        }
    }

    // Exercise the retained Object cache transforms with the same Core-to-world
    // frame. These are the hot-path operations used by collision and friction
    // code after IVP_Cache_Object_Manager refreshes an object's cache.
    IVP_Cache_Object objectCache{};
    objectCache.m_world_f_object = retailWorldFromCore;
    IVP_U_Point cacheRecoveredPoint;
    objectCache.transform_position_to_object_coords(
        &worldPoint, &cacheRecoveredPoint);
    for (int component = 0; component < 3; ++component) {
        EXPECT_NEAR(cacheRecoveredPoint.k[component], corePoint.k[component],
                    1.0e-7);
    }

    IVP_U_Float_Point floatCorePoint(0.5f, -0.25f, 1.25f);
    IVP_U_Point cacheWorldPoint;
    objectCache.transform_position_to_world_coords(
        &floatCorePoint, &cacheWorldPoint);
    for (int component = 0; component < 3; ++component) {
        EXPECT_NEAR(cacheWorldPoint.k[component], worldPoint.k[component],
                    1.0e-7);
    }

    IVP_U_Point coreDirection(0.25, 0.5, -0.75);
    IVP_U_Point worldDirection;
    objectCache.transform_vector_to_world_coords(
        &coreDirection, &worldDirection);
    IVP_U_Point recoveredDirection;
    objectCache.transform_vector_to_object_coords(
        &worldDirection, &recoveredDirection);
    for (int component = 0; component < 3; ++component) {
        EXPECT_NEAR(recoveredDirection.k[component],
                    coreDirection.k[component], 3.0e-8);
    }

    IVP_U_Float_Point floatCoreDirection(0.25f, 0.5f, -0.75f);
    IVP_U_Float_Point floatWorldDirection;
    objectCache.transform_vector_to_world_coords(
        &floatCoreDirection, &floatWorldDirection);
    IVP_U_Float_Point recoveredFloatDirection;
    objectCache.transform_vector_to_object_coords(
        &floatWorldDirection, &recoveredFloatDirection);
    for (int component = 0; component < 3; ++component) {
        EXPECT_NEAR(recoveredFloatDirection.k[component],
                    floatCoreDirection.k[component], 1.0e-6f);
    }

    // Ballance stores Core/Object orientation as four consecutive doubles.
    // Convert a nontrivial contact frame to that representation, perturb its
    // length as integration does, normalize it in the DLL, and convert back.
    IVP_U_Matrix3 sourceOrientation;
    sourceOrientation.init_rotated3(IVP_INDEX_Y, 0.37f);
    IVP_U_Quat orientation;
    _fpreset();
    orientation.set_quaternion(&sourceOrientation);
    orientation.x *= 2.0;
    orientation.y *= 2.0;
    orientation.z *= 2.0;
    orientation.w *= 2.0;
    orientation.normize_quat();
    IVP_U_Matrix3 recoveredOrientation;
    orientation.set_matrix(&recoveredOrientation);
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            EXPECT_NEAR(recoveredOrientation.get_elem(row, column),
                        sourceOrientation.get_elem(row, column), 1.0e-8);
        }
    }

    IVP_U_Matrix retailInverse = retailWorldFromCore;
    ASSERT_EQ(retailInverse.real_invert(), IVP_OK);
    IVP_U_Matrix retailIdentity;
    retailWorldFromCore.mmult4(&retailInverse, &retailIdentity);
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            EXPECT_NEAR(retailIdentity.get_elem(row, column),
                        row == column ? 1.0 : 0.0, 1.0e-12);
        }
        EXPECT_NEAR(retailIdentity.vv.k[row], 0.0, 1.0e-12);
    }

    // Exercise the retained actuator base constructor, vector accessor/growth,
    // and complete destructor as one Ballance-owned lifetime. These are the
    // same controlled-core entries used by spring/force controllers.
    {
        PassiveRetailActuator actuator;
        auto *controlled = actuator.get_associated_controlled_cores();
        ASSERT_EQ(controlled, &actuator.actuator_controlled_cores);
        EXPECT_EQ(controlled->len(), 0);
        IVP_Core *first = reinterpret_cast<IVP_Core *>(0x11110000u);
        IVP_Core *second = reinterpret_cast<IVP_Core *>(0x22220000u);
        controlled->add(first);
        controlled->add(second);
        EXPECT_EQ(controlled->len(), 2);
        EXPECT_EQ(controlled->element_at(0), first);
        EXPECT_EQ(controlled->element_at(1), second);
        EXPECT_EQ(actuator.get_controller_priority(), IVP_CP_ACTUATOR);
    }

    // The same store backs Phantom's per-object/per-Core mindist counters.
    // Start small enough to force Ballance's retail rehash while three nearby
    // objects enter the phantom, then update and remove one counter.
    {
        IVP_VHash_Store mindistCounters(4);
        int objectKeys[3]{};
        void *firstCount = reinterpret_cast<void *>(1u);
        void *secondCount = reinterpret_cast<void *>(2u);
        void *thirdCount = reinterpret_cast<void *>(3u);
        mindistCounters.add_elem(&objectKeys[0], firstCount);
        mindistCounters.add_elem(&objectKeys[1], secondCount);
        mindistCounters.add_elem(&objectKeys[2], thirdCount);
        EXPECT_EQ(mindistCounters.len(), 8);
        EXPECT_EQ(mindistCounters.n_elems(), 3);
        EXPECT_EQ(mindistCounters.find_elem(&objectKeys[0]), firstCount);
        EXPECT_EQ(mindistCounters.find_elem(&objectKeys[1]), secondCount);
        EXPECT_EQ(mindistCounters.find_elem(&objectKeys[2]), thirdCount);

        void *updatedCount = reinterpret_cast<void *>(4u);
        mindistCounters.change_elem(&objectKeys[1], updatedCount);
        EXPECT_EQ(mindistCounters.find_elem(&objectKeys[1]), updatedCount);
        EXPECT_EQ(mindistCounters.remove_elem(&objectKeys[0]), firstCount);
        EXPECT_EQ(mindistCounters.find_elem(&objectKeys[0]), nullptr);
        EXPECT_EQ(mindistCounters.n_elems(), 2);
    }

    // Manager construction owns a contiguous cache ring. The retained
    // constructor and destructor must agree on the Ballance 0xD0 stride.
    {
        IVP_Cache_Object_Manager cacheManager(4);
        const RawCacheManager &managerState =
            RawCacheManagerState(&cacheManager);
        EXPECT_EQ(managerState.cacheObjectCount, 4);
        EXPECT_EQ(managerState.reuseLoopIndex, 0);
        ASSERT_NE(managerState.cacheObjectsBuffer, nullptr);
        for (int index = 0; index < 4; ++index) {
            IVP_Cache_Object *entry = cacheManager.cache_object_at(index);
            ASSERT_NE(entry, nullptr);
            EXPECT_EQ(RawCache(entry).object, nullptr);
            if (index != 0) {
                const auto stride = reinterpret_cast<std::uintptr_t>(entry) -
                    reinterpret_cast<std::uintptr_t>(
                        cacheManager.cache_object_at(index - 1));
                EXPECT_EQ(stride, 0xD0u);
            }
        }
    }

    // Continuous collision prediction builds a 21-sample transform cache from
    // the same retail Cache Object used by contact and ray queries. Static
    // objects share its current matrix in every slot; simulated objects keep
    // only slot zero and invalidate future samples when the cache time changes.
    {
        alignas(16) std::array<std::byte, sizeof(IVP_Real_Object)>
            objectStorage{};
        alignas(16) std::array<std::byte, sizeof(IVP_Cache_Object)>
            cacheStorage{};
        auto *object = reinterpret_cast<IVP_Real_Object *>(
            objectStorage.data());
        auto *cache = reinterpret_cast<IVP_Cache_Object *>(
            cacheStorage.data());
        auto *core = reinterpret_cast<IVP_Core *>(0x13572460u);

        object->physical_core = core;
        object->flags = 0;
        object->flags.object_movement_state = IVP_MT_STATIC;
        RawCache(cache).validUntilTimeCode = 41;
        RawCache(cache).object = object;

        IVP_U_Matrix_Cache transformCache(cache);
        EXPECT_EQ(transformCache.object, object);
        EXPECT_EQ(transformCache.core, core);
        EXPECT_EQ(transformCache.base_time_code, 41);
        for (int index = 0;
             index <= IVP_3D_SOLVER_MAX_STEPS_PER_PSI; ++index) {
            EXPECT_EQ(transformCache.m_world_f_object[index],
                      &cache->m_world_f_object);
        }
        EXPECT_EQ(transformCache.calc_matrix_at_now(IVP_Time(0.0), 0),
                  &cache->m_world_f_object);
        EXPECT_EQ(transformCache.calc_matrix_at(IVP_Time(0.0), 7),
                  &cache->m_world_f_object);

        object->flags.object_movement_state = IVP_MT_MOVING;
        RawCache(cache).validUntilTimeCode = 42;
        transformCache.calc_calc_matrix_cache(cache);
        EXPECT_EQ(transformCache.base_time_code, 42);
        EXPECT_EQ(transformCache.m_world_f_object[0],
                  &cache->m_world_f_object);
        for (int index = 1;
             index <= IVP_3D_SOLVER_MAX_STEPS_PER_PSI; ++index) {
            EXPECT_EQ(transformCache.m_world_f_object[index], nullptr);
        }

        // A movement-state change alone does not invalidate an already valid
        // cache; the exact initializer runs only when the retail time code does.
        object->flags.object_movement_state = IVP_MT_STATIC;
        transformCache.calc_calc_matrix_cache(cache);
        EXPECT_EQ(transformCache.m_world_f_object[1], nullptr);
        RawCache(cache).validUntilTimeCode = 43;
        transformCache.calc_calc_matrix_cache(cache);
        EXPECT_EQ(transformCache.base_time_code, 43);
        for (int index = 0;
             index <= IVP_3D_SOLVER_MAX_STEPS_PER_PSI; ++index) {
            EXPECT_EQ(transformCache.m_world_f_object[index],
                      &cache->m_world_f_object);
        }

        // Invoke Ballance's continuous-collision solver with Mod-owned vtable
        // slot zero. Precomputed transforms isolate the stepping policy from
        // object integration while still crossing the DLL boundary in both
        // directions for every geometric separation query.
        IVP_U_Matrix_Cache secondTransformCache(cache);
        for (int index = 0;
             index <= IVP_3D_SOLVER_MAX_STEPS_PER_PSI; ++index) {
            transformCache.matrizes[index].set_identity();
            secondTransformCache.matrizes[index].set_identity();
            transformCache.m_world_f_object[index] =
                &transformCache.matrizes[index];
            secondTransformCache.m_world_f_object[index] =
                &secondTransformCache.matrizes[index];
        }

        MatrixSeparationSolver solver;
        solver.type = IVP_3D_SOLVER_TYPE_MAX_DEV;
        solver.max_deviation2 = 0.0;
        solver.set_max_deviation(10.0);

        for (int index = 0;
             index <= IVP_3D_SOLVER_MAX_STEPS_PER_PSI; ++index) {
            transformCache.matrizes[index].vv.k[0] = 0.2;
        }
        IVP_DOUBLE startSeparation = 0.2;
        IVP_Time collisionTime(-1.0);
        EXPECT_EQ(solver.find_first_t_for_value_max_dev(
                      0.1, IVP_Time(0.0), IVP_Time(0.1), 0,
                      &transformCache, &secondTransformCache,
                      &startSeparation, &collisionTime),
                  IVP_FALSE);
        EXPECT_DOUBLE_EQ(collisionTime.get_seconds(), -1.0);
        EXPECT_EQ(solver.find_first_t_for_value_max_dev2(
                      0.1, IVP_Time(0.0), IVP_Time(0.1), 0,
                      &transformCache, &secondTransformCache,
                      &startSeparation, &collisionTime),
                  IVP_FALSE);

        transformCache.matrizes[0].vv.k[0] = 0.05;
        transformCache.matrizes[1].vv.k[0] = 0.04;
        collisionTime = IVP_Time(-1.0);
        EXPECT_EQ(solver.find_first_t_for_value_coll(
                      0.1, 0.0, IVP_Time(0.0), IVP_Time(0.1),
                      &transformCache, &secondTransformCache,
                      nullptr, &collisionTime),
                  IVP_TRUE);
        EXPECT_DOUBLE_EQ(collisionTime.get_seconds(), 0.0);

        for (int index = 0;
             index <= IVP_3D_SOLVER_MAX_STEPS_PER_PSI; ++index) {
            transformCache.matrizes[index].vv.k[0] =
                0.05 + 0.001 * index;
        }
        collisionTime = IVP_Time(-1.0);
        const IVP_Time ordinaryPsiEnd(1.0 / 66.0);
        EXPECT_EQ(solver.find_first_t_for_value_coll(
                      0.1, 0.0, IVP_Time(0.0), ordinaryPsiEnd,
                      &transformCache, &secondTransformCache,
                      nullptr, &collisionTime),
                  IVP_FALSE);
        EXPECT_DOUBLE_EQ(collisionTime.get_seconds(), -1.0);

        // The protected root-refinement body returns IVP_Time through an x86
        // hidden result pointer. Give it two real retail-layout Object/Core
        // interpolation paths: A closes linearly from x=0.2 to x=0.1 while B
        // stays at the origin, so the x-separation crosses 0.15 at t=0.05.
        alignas(16) std::array<std::byte, sizeof(IVP_Core)> coreAStorage{};
        alignas(16) std::array<std::byte, sizeof(IVP_Core)> coreBStorage{};
        alignas(16) std::array<std::byte, sizeof(IVP_Real_Object)>
            rootObjectAStorage{};
        alignas(16) std::array<std::byte, sizeof(IVP_Real_Object)>
            rootObjectBStorage{};
        auto *coreA = reinterpret_cast<IVP_Core *>(coreAStorage.data());
        auto *coreB = reinterpret_cast<IVP_Core *>(coreBStorage.data());
        auto *rootObjectA = reinterpret_cast<IVP_Real_Object *>(
            rootObjectAStorage.data());
        auto *rootObjectB = reinterpret_cast<IVP_Real_Object *>(
            rootObjectBStorage.data());
        coreA->time_of_last_psi = IVP_Time(0.0);
        coreB->time_of_last_psi = IVP_Time(0.0);
        coreA->i_delta_time = 10.0f;
        coreB->i_delta_time = 10.0f;
        coreA->q_world_f_core_last_psi.set_identity();
        coreA->q_world_f_core_next_psi.set_identity();
        coreB->q_world_f_core_last_psi.set_identity();
        coreB->q_world_f_core_next_psi.set_identity();
        coreA->pos_world_f_core_last_psi.set(0.2, 0.0, 0.0);
        coreB->pos_world_f_core_last_psi.set_to_zero();
        coreA->delta_world_f_core_psis.set(-1.0f, 0.0f, 0.0f);
        coreB->delta_world_f_core_psis.set_to_zero();
        rootObjectA->physical_core = coreA;
        rootObjectB->physical_core = coreB;
        rootObjectA->flags.shift_core_f_object_is_zero = 1;
        rootObjectB->flags.shift_core_f_object_is_zero = 1;

        IVP_U_Matrix interpolatedA;
        IVP_U_Matrix interpolatedB;
        rootObjectA->calc_at_matrix(IVP_Time(0.05), &interpolatedA);
        rootObjectB->calc_at_matrix(IVP_Time(0.05), &interpolatedB);
        EXPECT_NEAR(interpolatedA.vv.k[0], 0.15, 1.0e-12);
        EXPECT_NEAR(interpolatedB.vv.k[0], 0.0, 1.0e-12);
        const IVP_Time refined = solver.refine_crossing(
            IVP_Time(0.0), IVP_Time(0.1), 0.15, 0.2, 0.1,
            rootObjectA, rootObjectB);
        EXPECT_NEAR(refined.get_seconds(), 0.05, 1.0e-12);
    }

    // A cache ring must never evict an entry that is still referenced by a
    // collision/raycast caller. Fill a two-entry ring, pin the first entry,
    // force the allocator to skip it and evict the second, then explicitly
    // invalidate and release the pinned entry so it can be reused. Manager
    // destruction must clear every remaining Real Object backlink.
    {
        alignas(16) std::array<std::byte, sizeof(IVP_Real_Object)>
            firstObjectStorage{};
        alignas(16) std::array<std::byte, sizeof(IVP_Real_Object)>
            secondObjectStorage{};
        alignas(16) std::array<std::byte, sizeof(IVP_Real_Object)>
            thirdObjectStorage{};
        alignas(16) std::array<std::byte, sizeof(IVP_Real_Object)>
            fourthObjectStorage{};
        auto *firstObject = reinterpret_cast<IVP_Real_Object *>(
            firstObjectStorage.data());
        auto *secondObject = reinterpret_cast<IVP_Real_Object *>(
            secondObjectStorage.data());
        auto *thirdObject = reinterpret_cast<IVP_Real_Object *>(
            thirdObjectStorage.data());
        auto *fourthObject = reinterpret_cast<IVP_Real_Object *>(
            fourthObjectStorage.data());

        {
            IVP_Cache_Object_Manager cacheManager(2);
            IVP_Cache_Object *firstCache =
                cacheManager.get_cache_object(firstObject);
            ASSERT_EQ(firstObject->cache_object, firstCache);
            EXPECT_EQ(RawCache(firstCache).object, firstObject);
            firstObject->flags = 0;
            firstObject->flags.object_movement_state = IVP_MT_STATIC;

            // Ray queries use the no-lock form while contact and coordinate
            // users pin the same cache.  The reconstructed inline API must
            // preserve that distinction so the retail ring can evict only
            // entries which no caller still owns.
            EXPECT_EQ(firstObject->get_cache_object_no_lock(), firstCache);
            EXPECT_EQ(RawCache(firstCache).referenceCount, 0);
            EXPECT_EQ(firstObject->get_cache_object(), firstCache);
            EXPECT_EQ(RawCache(firstCache).referenceCount, 1);

            IVP_Cache_Object *secondCache =
                cacheManager.get_cache_object(secondObject);
            ASSERT_EQ(secondObject->cache_object, secondCache);
            EXPECT_NE(secondCache, firstCache);

            IVP_Cache_Object *thirdCache =
                cacheManager.get_cache_object(thirdObject);
            EXPECT_EQ(thirdCache, secondCache);
            EXPECT_EQ(secondObject->cache_object, nullptr);
            EXPECT_EQ(thirdObject->cache_object, thirdCache);
            EXPECT_EQ(firstObject->cache_object, firstCache);

            IVP_Cache_Object_Manager::invalid_cache_object(firstObject);
            EXPECT_EQ(firstObject->cache_object, nullptr);
            EXPECT_EQ(RawCache(firstCache).object, nullptr);
            EXPECT_EQ(RawCache(firstCache).referenceCount, 1);
            firstCache->remove_reference();
            EXPECT_EQ(RawCache(firstCache).referenceCount, 0);

            IVP_Cache_Object *fourthCache =
                cacheManager.get_cache_object(fourthObject);
            EXPECT_EQ(fourthCache, firstCache);
            EXPECT_EQ(fourthObject->cache_object, fourthCache);
            EXPECT_EQ(thirdObject->cache_object, thirdCache);

            // The compiler emitted Ballance's public inline locking accessor
            // as a shared private body at RVA 0x1A190.  Use it as the retail
            // oracle and confirm the same cache identity/reference contract.
            fourthObject->flags = 0;
            fourthObject->flags.object_movement_state = IVP_MT_STATIC;
            using RetailGetCacheObject =
                IVP_Cache_Object *(__thiscall *)(IVP_Real_Object *);
            const auto retailGetCacheObject =
                RetailFunction<RetailGetCacheObject>(
                    physics.get(), kRealObjectGetCacheObjectInlineRva);
            ASSERT_NE(retailGetCacheObject, nullptr);
            EXPECT_EQ(retailGetCacheObject(fourthObject), fourthCache);
            EXPECT_EQ(RawCache(fourthCache).referenceCount, 1);
            fourthCache->remove_reference();
        }

        EXPECT_EQ(thirdObject->cache_object, nullptr);
        EXPECT_EQ(fourthObject->cache_object, nullptr);
    }

    // Mindist scheduling uses this minimum hash to choose the next collision
    // event. Verify ordering changes, explicit removal, remove-min and final
    // cleanup with distinct event identities instead of an empty-container test.
    {
        IVP_U_Min_Hash pendingMindists(8);
        int eventKeys[3]{};
        pendingMindists.add(&eventKeys[0], 3.0);
        pendingMindists.add(&eventKeys[1], 1.0);
        pendingMindists.add(&eventKeys[2], 2.0);
        EXPECT_EQ(pendingMindists.counter, 3);
        EXPECT_EQ(pendingMindists.find_min_elem(), &eventKeys[1]);
        EXPECT_NEAR(pendingMindists.find_min_value(), 1.0, 1.0e-12);

        pendingMindists.change_value(&eventKeys[1], 4.0);
        EXPECT_EQ(pendingMindists.find_min_elem(), &eventKeys[2]);
        EXPECT_NEAR(pendingMindists.find_min_value(), 2.0, 1.0e-12);
        pendingMindists.remove_min();
        EXPECT_EQ(pendingMindists.counter, 2);
        EXPECT_EQ(pendingMindists.is_elem(&eventKeys[2]), 0);
        EXPECT_EQ(pendingMindists.find_min_elem(), &eventKeys[0]);

        pendingMindists.remove(&eventKeys[0]);
        EXPECT_EQ(pendingMindists.counter, 1);
        EXPECT_EQ(pendingMindists.find_min_elem(), &eventKeys[1]);
    }

    // Run Ballance's retained gravity controller over one ordinary Core and
    // one Core carrying the nearby revision's pinned bit. Unlike that nearby
    // source, the retail loop processes both: pending pushes are committed
    // before gravity is integrated.
    {
        IVP_Standard_Gravity_Controller gravityController;
        IVP_U_Point gravity(0.0, -9.81, 0.0);
        gravityController.set_standard_gravity(&gravity);
        EXPECT_NEAR(gravityController.grav_vec.k[0], 0.0f, 1.0e-6f);
        EXPECT_NEAR(gravityController.grav_vec.k[1], -9.81f, 1.0e-6f);
        EXPECT_NEAR(gravityController.grav_vec.k[2], 0.0f, 1.0e-6f);
        EXPECT_EQ(gravityController.get_controller_priority(), IVP_CP_GRAVITY);

        alignas(16) std::array<std::byte, sizeof(IVP_Core)> movableStorage{};
        alignas(16) std::array<std::byte, sizeof(IVP_Core)> pinnedStorage{};
        const auto writeFloat = [](auto &storage, std::size_t offset,
                                   IVP_FLOAT value) {
            *reinterpret_cast<IVP_FLOAT *>(storage.data() + offset) = value;
        };
        const auto readFloat = [](const auto &storage, std::size_t offset) {
            return *reinterpret_cast<const IVP_FLOAT *>(
                storage.data() + offset);
        };

        // Zero damping coefficients make the retail global damping factor 1.
        // speed_change is committed into speed before gravity is added.
        writeFloat(movableStorage, 0x84, 0.5f);
        writeFloat(movableStorage, 0x88, 0.0f);
        writeFloat(movableStorage, 0x8C, -0.25f);
        writeFloat(movableStorage, 0xA4, 1.0f);
        writeFloat(movableStorage, 0xA8, 2.0f);
        writeFloat(movableStorage, 0xAC, 3.0f);

        auto *pinnedCore = reinterpret_cast<IVP_Core *>(pinnedStorage.data());
        pinnedCore->flags = 0u;
        pinnedCore->pinned = IVP_TRUE;
        EXPECT_EQ(pinnedCore->flags, 0x100u);
        writeFloat(pinnedStorage, 0x84, 7.0f);
        writeFloat(pinnedStorage, 0xA4, 4.0f);
        writeFloat(pinnedStorage, 0xA8, 5.0f);
        writeFloat(pinnedStorage, 0xAC, 6.0f);

        IVP_U_Vector<IVP_Core> gravityCores(2);
        gravityCores.add(reinterpret_cast<IVP_Core *>(movableStorage.data()));
        gravityCores.add(pinnedCore);
        IVP_Event_Sim gravityStep(nullptr, 0.25);
        _fpreset();
        gravityController.do_simulation_controller(
            &gravityStep, &gravityCores);

        EXPECT_NEAR(readFloat(movableStorage, 0xA4), 1.5f, 1.0e-6f);
        EXPECT_NEAR(readFloat(movableStorage, 0xA8),
                    2.0f - 9.81f * 0.25f, 1.0e-6f);
        EXPECT_NEAR(readFloat(movableStorage, 0xAC), 2.75f, 1.0e-6f);
        EXPECT_NEAR(readFloat(movableStorage, 0x84), 0.0f, 1.0e-6f);
        EXPECT_NEAR(readFloat(movableStorage, 0x8C), 0.0f, 1.0e-6f);

        EXPECT_NEAR(readFloat(pinnedStorage, 0xA4), 11.0f, 1.0e-6f);
        EXPECT_NEAR(readFloat(pinnedStorage, 0xA8),
                    5.0f - 9.81f * 0.25f, 1.0e-6f);
        EXPECT_NEAR(readFloat(pinnedStorage, 0xAC), 6.0f, 1.0e-6f);
        EXPECT_NEAR(readFloat(pinnedStorage, 0x84), 0.0f, 1.0e-6f);
    }

    // When the object behind an anchor is deleted, Ballance's retained
    // callback forwards the anchor identity to slot 7 of its owning actuator.
    // This is the ownership notification used to prevent a dangling anchor.
    {
        std::array<std::uintptr_t, 8> actuatorVtable{};
        actuatorVtable[7] = reinterpret_cast<std::uintptr_t>(
            &RecordAnchorDeletion);
        alignas(4) std::array<std::byte, sizeof(void *)> actuatorStorage{};
        *reinterpret_cast<std::uintptr_t **>(actuatorStorage.data()) =
            actuatorVtable.data();

        alignas(16) std::array<std::byte, sizeof(IVP_Anchor)> anchorStorage{};
        auto *anchor = reinterpret_cast<IVP_Anchor *>(anchorStorage.data());
        auto *actuator = reinterpret_cast<IVP_Actuator *>(
            actuatorStorage.data());
        anchor->l_actuator = actuator;

        gExpectedAnchorActuator = actuator;
        gExpectedDeletedAnchor = anchor;
        gAnchorDeletionNotifications = 0;
        anchor->object_is_going_to_be_deleted_event(
            reinterpret_cast<IVP_Real_Object *>(0x12340000u));
        EXPECT_EQ(gAnchorDeletionNotifications, 1);
        EXPECT_EQ(anchor->l_actuator, actuator);
        gExpectedAnchorActuator = nullptr;
        gExpectedDeletedAnchor = nullptr;
    }
}
