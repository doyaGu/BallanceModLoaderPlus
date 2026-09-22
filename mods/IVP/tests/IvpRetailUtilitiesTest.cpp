#include "IvpRetailTestSupport.h"

namespace {

class RecordingTimeEvent final : public IVP_Time_Event {
public:
    void simulate_time_event(IVP_Environment *environment) override {
        lastEnvironment = environment;
        ++calls;
    }

    IVP_Environment *lastEnvironment = nullptr;
    int calls = 0;
};

class RecordingActiveFloatListener final
    : public IVP_U_Active_Float_Listener {
public:
    void active_float_changed(IVP_U_Active_Float *value) override {
        last_value = value;
        ++calls;
    }

    IVP_U_Active_Float *last_value = nullptr;
    int calls = 0;
};

class RecordingActiveIntListener final : public IVP_U_Active_Int_Listener {
public:
    void active_int_changed(IVP_U_Active_Int *value) override {
        last_value = value;
        ++calls;
    }

    IVP_U_Active_Int *last_value = nullptr;
    int calls = 0;
};

class RecordingPhantomVolumeListener final : public IVP_Listener_Phantom {
public:
    void mindist_entered_volume(
        IVP_Controller_Phantom *controller,
        IVP_Mindist_Base *mindist) override {
        last_controller = controller;
        last_mindist = mindist;
        ++mindist_entered;
    }
    void mindist_left_volume(
        IVP_Controller_Phantom *controller,
        IVP_Mindist_Base *mindist) override {
        last_controller = controller;
        last_mindist = mindist;
        ++mindist_left;
    }
    void core_entered_volume(IVP_Controller_Phantom *, IVP_Core *) override {}
    void core_left_volume(IVP_Controller_Phantom *, IVP_Core *) override {}
    void phantom_is_going_to_be_deleted_event(
        IVP_Controller_Phantom *) override {}

    IVP_Controller_Phantom *last_controller = nullptr;
    IVP_Mindist_Base *last_mindist = nullptr;
    int mindist_entered = 0;
    int mindist_left = 0;
};

class PhantomVolumeAccess : public IVP_Controller_Phantom {
public:
    static void Enter(
        IVP_Controller_Phantom *controller, IVP_Mindist *mindist) {
        reinterpret_cast<PhantomVolumeAccess *>(controller)
            ->mindist_entered_volume(mindist);
    }
    static void Leave(
        IVP_Controller_Phantom *controller, IVP_Mindist *mindist) {
        reinterpret_cast<PhantomVolumeAccess *>(controller)
            ->mindist_left_volume(mindist);
    }
};

struct RawPhantomController {
    IVP_Real_Object *object = nullptr;
    IVP_FLOAT exit_policy_extra_radius = 0.5f;
    IVP_U_Vector<IVP_Listener_Phantom> listeners;
    IVP_U_Set_Active<IVP_Mindist_Base> set_of_mindists{4};
    IVP_U_Set_Active<IVP_Real_Object> *set_of_objects = nullptr;
    IVP_VHash_Store *mindist_object_counter = nullptr;
    IVP_VHash_Store *mindist_core_counter = nullptr;
    IVP_U_Set_Active<IVP_Core> *set_of_cores = nullptr;
    IVP_Time time_of_last_set_transformation{};
};

static_assert(offsetof(RawPhantomController, listeners) == 0x08);
static_assert(offsetof(RawPhantomController, set_of_mindists) == 0x10);
static_assert(offsetof(RawPhantomController, set_of_objects) == 0x28);
static_assert(sizeof(RawPhantomController) == 0x40);
class RecordingDebugManager final : public IVP_BetterDebugmanager {
public:
    void output_function(
        IVP_DEBUG_CLASS classId, const char *message) override {
        lastClass = classId;
        lastMessage = message ? message : "";
        ++outputCalls;
    }

    IVP_DEBUG_CLASS lastClass = IVP_DM_DUMMY;
    std::string lastMessage;
    int outputCalls = 0;
};

class RecordingStatisticsCallback final
    : public IVP_BetterStatisticsmanager_Callback_Interface {
public:
    void output_request(
        IVP_BetterStatisticsmanager_Data_Entity *entity) override {
        outputs.push_back(entity);
    }
    void enable() override { enabled = true; }
    void disable() override { enabled = false; }

    bool enabled = false;
    std::vector<IVP_BetterStatisticsmanager_Data_Entity *> outputs;
};

} // namespace

TEST(IvpRetailPublicUtilities,
     AllocationsStringsAndAlignmentStayOnTheRetailHeap) {
    RetailImage physics;
    if (!physics.configured()) {
        GTEST_SKIP() << "Set BML_TEST_RETAIL_PHYSICS_DLL to the exact "
                        "Ballance BuildingBlocks\\physics_RT.dll";
    }
    ASSERT_TRUE(physics.valid()) << physics.error();

    // Public utility allocations are consumed by IVP vectors, compact
    // surfaces, active-value names and diagnostics. Exercise the exact retail
    // allocator/string bodies together with their compatible release path so
    // a UCRT/MSVCRT mismatch fails here instead of in a later engine delete.
    auto *plain = static_cast<std::uint8_t *>(p_malloc(96));
    ASSERT_NE(plain, nullptr);
    std::memset(plain, 0xA5, 96);
    EXPECT_EQ(plain[0], 0xA5);
    EXPECT_EQ(plain[95], 0xA5);
    p_free(plain);

    char *zeroed = p_calloc(24, 4);
    ASSERT_NE(zeroed, nullptr);
    EXPECT_TRUE(std::all_of(
        zeroed, zeroed + 96, [](char value) { return value == 0; }));
    p_free(zeroed);

    char *copy = p_strdup("Ballance IVP public utility ownership");
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(p_strlen(copy), 37);
    EXPECT_EQ(p_strcmp(copy, "Ballance IVP public utility ownership"), 0);
    EXPECT_LT(p_strcmp("Ballance", "IVP"), 0);
    EXPECT_EQ(p_strlen(nullptr), 0);
    EXPECT_EQ(p_strcmp(nullptr, nullptr), 0);
    EXPECT_GT(p_strcmp(nullptr, "value"), 0);
    EXPECT_LT(p_strcmp("value", nullptr), 0);
    p_free(copy);

    void *aligned = ivp_malloc_aligned(128, 16);
    ASSERT_NE(aligned, nullptr);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(aligned) & 0x0Fu, 0u);
    std::memset(aligned, 0x5A, 128);
    ivp_free_aligned(aligned);

    auto *alignedZeroed = static_cast<std::uint8_t *>(
        ivp_calloc_aligned(128, 16));
    ASSERT_NE(alignedZeroed, nullptr);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(alignedZeroed) & 0x0Fu, 0u);
    EXPECT_TRUE(std::all_of(
        alignedZeroed, alignedZeroed + 128,
        [](std::uint8_t value) { return value == 0; }));
    ivp_free_aligned(alignedZeroed);
}

TEST(IvpRetailControlValues,
     OriginalMaterialAndLiquidBodiesPreserveRuntimeState) {
    RetailImage physics;
    if (!physics.configured()) {
        GTEST_SKIP() << "Set BML_TEST_RETAIL_PHYSICS_DLL to the exact "
                        "Ballance BuildingBlocks\\physics_RT.dll";
    }
    ASSERT_TRUE(physics.valid()) << physics.error();

    // Ballance movement checks and Mod-side callers must advance one shared
    // process-global IVP random sequence. The DLL retains ivp_rand(), while
    // the two public seed accessors were linked away and are reconstructed
    // against the seed address read by that exact body.
    {
        const int previousSeed = ivp_srand_read();
        ivp_srand(12345);
        EXPECT_EQ(ivp_srand_read(), 12345);
        const int advancedSeed = 12345 * 75;
        EXPECT_FLOAT_EQ(
            ivp_rand(),
            static_cast<IVP_FLOAT>(advancedSeed & 0xFFFF) / 65536.0f);
        EXPECT_EQ(ivp_srand_read(), advancedSeed);

        ivp_srand(0);
        EXPECT_EQ(ivp_srand_read(), 1);
        EXPECT_FLOAT_EQ(ivp_rand(), 75.0f / 65536.0f);
        EXPECT_EQ(ivp_srand_read(), 75);
        ivp_srand(previousSeed);
    }

    // A Ballance diagnostics overlay needs rolling counters to update only at
    // the configured simulation interval and must omit disabled channels.
    // Exercise the complete source-visible manager/entity/callback protocol.
    {
        IVP_BetterStatisticsmanager_Data_Entity integerValue(INT_VALUE);
        IVP_BetterStatisticsmanager_Data_Entity doubleValue(DOUBLE_VALUE);
        IVP_BetterStatisticsmanager_Data_Entity integerHistory(INT_ARRAY);
        IVP_BetterStatisticsmanager_Data_Entity doubleHistory(DOUBLE_ARRAY);
        IVP_BetterStatisticsmanager_Data_Entity label(STRING);
        RecordingStatisticsCallback callback;
        IVP_BetterStatisticsmanager statistics;

        EXPECT_EQ(statistics.get_state(), IVP_TRUE);
        EXPECT_EQ(statistics.update_delayed, IVP_TRUE);
        EXPECT_DOUBLE_EQ(statistics.update_interval, 1.0);

        integerValue.set_int_value(42);
        doubleValue.set_double_value(3.5);
        integerHistory.set_array_size(3);
        doubleHistory.set_array_size(3);
        for (int value = 1; value <= 4; ++value) {
            integerHistory.set_int_array_latest_value(value);
            doubleHistory.set_double_array_latest_value(value * 0.25);
        }
        label.set_text("Ball velocity");
        label.set_position(24, 40);

        EXPECT_EQ(integerValue.data.int_value, 42);
        EXPECT_DOUBLE_EQ(doubleValue.data.double_value, 3.5);
        EXPECT_EQ(integerHistory.data.int_array.array[0], 2);
        EXPECT_EQ(integerHistory.data.int_array.array[1], 3);
        EXPECT_EQ(integerHistory.data.int_array.array[2], 4);
        EXPECT_DOUBLE_EQ(doubleHistory.data.double_array.array[0], 0.5);
        EXPECT_DOUBLE_EQ(doubleHistory.data.double_array.array[1], 0.75);
        EXPECT_DOUBLE_EQ(doubleHistory.data.double_array.array[2], 1.0);
        EXPECT_EQ(doubleHistory.data.double_array.height, 0);
        EXPECT_EQ(doubleHistory.data.double_array.bg_color, 1);
        EXPECT_EQ(doubleHistory.data.double_array.border_color, 2);
        EXPECT_STREQ(label.text, "Ball velocity");
        EXPECT_EQ(label.xpos, 24);
        EXPECT_EQ(label.ypos, 40);

        callback.enable();
        EXPECT_TRUE(callback.enabled);
        callback.disable();
        EXPECT_FALSE(callback.enabled);
        callback.enable();

        statistics.install_output_callback(&callback);
        statistics.install_data_entity(&integerValue);
        statistics.install_data_entity(&doubleValue);
        statistics.install_data_entity(&integerHistory);
        statistics.install_data_entity(&doubleHistory);
        statistics.install_data_entity(&label);

        statistics.print();
        EXPECT_TRUE(callback.outputs.empty());
        statistics.set_simulation_time(0.5);
        statistics.print();
        EXPECT_TRUE(callback.outputs.empty());

        doubleValue.disable();
        EXPECT_EQ(doubleValue.get_state(), IVP_FALSE);
        statistics.set_simulation_time(1.25);
        statistics.print();
        EXPECT_EQ(callback.outputs,
                  (std::vector<IVP_BetterStatisticsmanager_Data_Entity *>{
                      &integerValue, &integerHistory, &doubleHistory, &label}));
        doubleValue.enable();
        EXPECT_EQ(doubleValue.get_state(), IVP_TRUE);

        statistics.disable();
        EXPECT_EQ(statistics.get_state(), IVP_FALSE);
        statistics.print();
        EXPECT_EQ(callback.outputs.size(), 4u);
        statistics.enable();
        EXPECT_EQ(statistics.get_state(), IVP_TRUE);

        statistics.remove_data_entity(&label);
        statistics.remove_data_entity(&label);
        statistics.remove_output_callback(&callback);
        statistics.remove_output_callback(&callback);
        statistics.print();
        EXPECT_EQ(callback.outputs.size(), 4u);

        // The public console callback is the stock Ballance diagnostics sink.
        // Route one concrete gameplay counter through the manager rather than
        // merely checking that the vptr can be constructed.
        IVP_Statisticsmanager_Console_Callback consoleCallback;
        IVP_BetterStatisticsmanager consoleStatistics;
        integerValue.set_text("Ball contacts: ");
        consoleStatistics.install_data_entity(&integerValue);
        consoleStatistics.install_output_callback(&consoleCallback);
        consoleStatistics.set_simulation_time(100.0);
        testing::internal::CaptureStdout();
        consoleStatistics.print();
        const std::string consoleOutput =
            testing::internal::GetCapturedStdout();
        EXPECT_EQ(consoleOutput, "Ball contacts: 42\n");
        consoleStatistics.remove_output_callback(&consoleCallback);
        consoleStatistics.remove_data_entity(&integerValue);

        // The historical entity destructor owns text only; array storage is
        // explicitly released here, matching the original ownership rule.
        BML::IVP::ABI::Invoke<void>(
            BML::IVP::ABI::Address::Free,
            integerHistory.data.int_array.array);
        integerHistory.data.int_array.array = nullptr;
        BML::IVP::ABI::Invoke<void>(
            BML::IVP::ABI::Address::Free,
            doubleHistory.data.double_array.array);
        doubleHistory.data.double_array.array = nullptr;
    }

    // Ballance brackets each PSI with this six-slot interface. The retained
    // simple implementation clears its complete counter window, increments
    // the PSI count at start, and advances the active timing bucket at pcount.
    // Wall-clock counter values are intentionally not asserted.
    {
        IVP_PerformanceCounter_Simple counter;
        EXPECT_EQ(counter.ref_counter64, 0);
        EXPECT_EQ(counter.count_PSIs, 0);
        EXPECT_EQ(counter.counting, IVP_PE_PSI_START);
        for (const auto &bucket : counter.counter) {
            EXPECT_EQ(bucket[0], 0);
            EXPECT_EQ(bucket[1], 0);
        }

        counter.start_pcount();
        EXPECT_EQ(counter.count_PSIs, 0);
        EXPECT_EQ(counter.counting, IVP_PE_PSI_START);
        counter.pcount(IVP_PE_PSI_UNIVERSE);
        EXPECT_EQ(counter.count_PSIs, 1);
        EXPECT_EQ(counter.counting, IVP_PE_PSI_UNIVERSE);
        counter.pcount(IVP_PE_PSI_CONTROLLERS);
        EXPECT_EQ(counter.counting, IVP_PE_PSI_CONTROLLERS);
        counter.stop_pcount();
        EXPECT_EQ(counter.counting, IVP_PE_PSI_CONTROLLERS);
    }

    // Collision filters are installed before gameplay objects enter the
    // broadphase. Validate exact group matching, composite all-filter dispatch,
    // environment teardown and the self-owning Exclusive Pair callback.
    {
        alignas(16) std::array<std::byte, sizeof(IVP_Real_Object)>
            firstStorage{};
        alignas(16) std::array<std::byte, sizeof(IVP_Real_Object)>
            secondStorage{};
        auto *first = reinterpret_cast<IVP_Real_Object *>(firstStorage.data());
        auto *second = reinterpret_cast<IVP_Real_Object *>(secondStorage.data());
        std::memcpy(first->nocoll_group_ident, "BALL", 5);
        std::memcpy(second->nocoll_group_ident, "BALL", 5);

        IVP_Collision_Filter_Coll_Group_Ident groupFilter(IVP_FALSE);
        EXPECT_EQ(groupFilter.check_objects_for_collision_detection(
                      first, second), IVP_FALSE);
        std::memcpy(second->nocoll_group_ident, "RAIL", 5);
        EXPECT_EQ(groupFilter.check_objects_for_collision_detection(
                      first, second), IVP_TRUE);
        second->nocoll_group_ident[0] = '\0';
        EXPECT_EQ(groupFilter.check_objects_for_collision_detection(
                      first, second), IVP_TRUE);
        groupFilter.environment_will_be_deleted(nullptr);

        RecordingCollisionFilter accepts(IVP_TRUE);
        RecordingCollisionFilter rejects(IVP_FALSE);
        IVP_Meta_Collision_Filter composite(IVP_FALSE);
        composite.add_collision_filter(&accepts);
        composite.add_collision_filter(&rejects);
        EXPECT_EQ(composite.check_objects_for_collision_detection(
                      first, second), IVP_FALSE);
        EXPECT_EQ(accepts.checkCalls, 1);
        EXPECT_EQ(rejects.checkCalls, 1);

        auto *environment = reinterpret_cast<IVP_Environment *>(0x12340000u);
        composite.environment_will_be_deleted(environment);
        EXPECT_EQ(accepts.environmentDeleteCalls, 1);
        EXPECT_EQ(rejects.environmentDeleteCalls, 1);
        EXPECT_EQ(accepts.lastEnvironment, environment);
        EXPECT_EQ(rejects.lastEnvironment, environment);
        EXPECT_EQ(composite.check_objects_for_collision_detection(
                      first, second), IVP_TRUE);

        void *pairStorage = BML::IVP::ABI::Invoke<void *>(
            BML::IVP::ABI::Address::OperatorNew,
            static_cast<unsigned int>(
                sizeof(IVP_Collision_Filter_Exclusive_Pair)));
        ASSERT_NE(pairStorage, nullptr);
        std::memset(pairStorage, 0xA5,
                    sizeof(IVP_Collision_Filter_Exclusive_Pair));
        auto *pairFilter =
            ::new (pairStorage) IVP_Collision_Filter_Exclusive_Pair();
        EXPECT_EQ(pairFilter->check_objects_for_collision_detection(
                      first, second), IVP_TRUE);
        pairFilter->disable_collision_between_objects(first, second);
        pairFilter->disable_collision_between_objects(first, second);
        EXPECT_EQ(pairFilter->check_objects_for_collision_detection(
                      first, second), IVP_FALSE);
        pairFilter->enable_collision_between_objects(first, second);
        pairFilter->enable_collision_between_objects(first, second);
        EXPECT_EQ(pairFilter->check_objects_for_collision_detection(
                      first, second), IVP_TRUE);
        pairFilter->environment_will_be_deleted(environment);
    }

    // IVP diagnostics are useful when a reconstructed surface builder rejects
    // Ballance geometry. Verify channel control and formatted redirection,
    // including the retail variadic member's stack calling convention.
    {
        static_assert(sizeof(IVP_BetterDebugmanager) == 0x2008);
        RecordingDebugManager diagnostics;
        EXPECT_EQ(diagnostics.is_debug_enabled(
                      IVP_DM_SURBUILD_POINTSOUP), IVP_FALSE);
        diagnostics.enable_debug_output(IVP_DM_SURBUILD_POINTSOUP);
        EXPECT_EQ(diagnostics.is_debug_enabled(
                      IVP_DM_SURBUILD_POINTSOUP), IVP_TRUE);
        EXPECT_EQ(diagnostics.is_debug_enabled(
                      IVP_DEBUG_MAX_N_CLASSES), IVP_FALSE);

        diagnostics.dprint(
            IVP_DM_SURBUILD_POINTSOUP,
            "surface %d: %.1f ledges", 17, 6.5);
        EXPECT_EQ(diagnostics.outputCalls, 1);
        EXPECT_EQ(diagnostics.lastClass, IVP_DM_SURBUILD_POINTSOUP);
        EXPECT_EQ(diagnostics.lastMessage, "surface 17: 6.5 ledges");

        diagnostics.disable_debug_output(IVP_DM_SURBUILD_POINTSOUP);
        EXPECT_EQ(diagnostics.is_debug_enabled(
                      IVP_DM_SURBUILD_POINTSOUP), IVP_FALSE);

        EXPECT_EQ(
            IVP_Get_Debugmanager(),
            reinterpret_cast<IVP_BetterDebugmanager *>(
                gRetailModuleBase +
                BML::IVP::ABI::BetterDebugmanagerGlobalRva));
    }

    // The retained getter returns the process-wide manager constructed by
    // physics_RT's static initializer. Its constructor clears the optimization
    // flag and the complete IVP_U_Vector at +0x04, proving the 0x0C boundary.
    {
        static_assert(sizeof(IVP_Environment_Manager) == 0x0C);
        IVP_Environment_Manager *manager =
            IVP_Environment_Manager::get_environment_manager();
        ASSERT_EQ(
            manager,
            reinterpret_cast<IVP_Environment_Manager *>(
                gRetailModuleBase + 0x00075DA0u));
        EXPECT_EQ(manager->ivp_willamette_optimization, 0);
        EXPECT_EQ(manager->environments.len(), 0);
        EXPECT_EQ(manager->environments.memsize, 0);
        EXPECT_EQ(manager->environments.elems, nullptr);
    }

    // IVP_Time crosses the DLL boundary by value as one 8-byte scalar.  Use
    // the retained environment setter to verify both stack halves, the
    // destination offset and the cache-code increment in one state change.
    {
        alignas(16) std::array<std::byte, sizeof(IVP_Environment)>
            environmentStorage{};
        auto *environment = reinterpret_cast<IVP_Environment *>(
            environmentStorage.data());
        environment->current_time = IVP_Time(-17.0);
        environment->current_time_code = 41;

        const IVP_Time contactTime(123.456789);
        environment->set_current_time(contactTime);
        EXPECT_DOUBLE_EQ(environment->get_current_time().get_time(),
                         123.456789);
        EXPECT_EQ(environment->current_time_code, 42);
    }

    // Application setup defaults to a 256-entry object-cache pool and leaves
    // all optional user managers unset. The Environment setters then maintain
    // both sides of their derived state: PSI duration plus reciprocal, and the
    // double gravity vector plus scalar magnitude and controller float copy.
    {
        alignas(IVP_Application_Environment)
            std::array<std::byte, sizeof(IVP_Application_Environment)>
                applicationStorage;
        std::fill(applicationStorage.begin(), applicationStorage.end(),
                  std::byte{0xA5});
        auto &application = *::new (applicationStorage.data())
            IVP_Application_Environment();
        EXPECT_EQ(application.n_cache_object, 256);
        EXPECT_EQ(application.scratchpad_addr, nullptr);
        EXPECT_EQ(application.scratchpad_size, 0);
        EXPECT_EQ(application.material_manager, nullptr);
        EXPECT_EQ(application.collision_filter, nullptr);
        EXPECT_EQ(application.universe_manager, nullptr);
        EXPECT_EQ(application.performancecounter, nullptr);
        EXPECT_EQ(application.anomaly_manager, nullptr);
        EXPECT_EQ(application.anomaly_limits, nullptr);
        EXPECT_EQ(application.default_collision_delegator_root, nullptr);
        EXPECT_EQ(application.env_active_float_manager, nullptr);
        EXPECT_EQ(application.range_manager, nullptr);

        // Build and tear down an actual Ballance Environment, not synthetic
        // storage. This exercises the constructor writes, manager backlink,
        // owned customer-name allocation and destructor unlink/free path that
        // distinguish the retail layout from the neighboring source revision.
        IVP_Environment_Manager *manager =
            IVP_Environment_Manager::get_environment_manager();
        ASSERT_NE(manager, nullptr);
        ASSERT_EQ(manager->environments.len(), 0);
        constexpr char customerName[] = "BML retail environment ABI";
        constexpr unsigned int authorizationCode = 0x51A7C0DEu;
        IVP_Environment *ownedEnvironment = manager->create_environment(
            &application, customerName, authorizationCode);
        ASSERT_NE(ownedEnvironment, nullptr);
        ASSERT_EQ(manager->environments.len(), 1);
        EXPECT_EQ(manager->environments.element_at(0), ownedEnvironment);
        EXPECT_STREQ(ownedEnvironment->auth_costumer_name, customerName);
        EXPECT_NE(ownedEnvironment->auth_costumer_name, customerName);
        EXPECT_EQ(ownedEnvironment->auth_costumer_code, authorizationCode);
        EXPECT_EQ(ownedEnvironment->pw_count, 10);
        EXPECT_EQ(ownedEnvironment->environment_manager, manager);

        // The add helper is stripped, but Ballance retains the complete
        // teardown at RVA 0x13940. Exercise it on a real Environment with
        // retail-allocated nodes and copied labels so both destructor and
        // allocator boundaries are covered by the original DLL.
        const IVP_U_Point gravityStart(0.0, 1.5, 0.0);
        const IVP_U_Float_Point gravityDirection(0.0f, -9.81f, 0.0f);
        const IVP_U_Point contactStart(2.0, 0.25, -1.0);
        const IVP_U_Float_Point contactDirection(0.0f, 1.0f, 0.0f);
        ownedEnvironment->add_draw_vector(
            &gravityStart, &gravityDirection, "gravity", 0x3366CC);
        ownedEnvironment->add_draw_vector(
            &contactStart, &contactDirection, "contact normal", 0xCC6633);
        ASSERT_NE(ownedEnvironment->draw_vectors, nullptr);
        EXPECT_STREQ(
            ownedEnvironment->draw_vectors->debug_text, "contact normal");
        ASSERT_NE(ownedEnvironment->draw_vectors->next, nullptr);
        EXPECT_STREQ(
            ownedEnvironment->draw_vectors->next->debug_text, "gravity");
        ownedEnvironment->delete_draw_vector_debug();
        EXPECT_EQ(ownedEnvironment->draw_vectors, nullptr);

        // Also enter the public node destructor wrapper directly. Both the
        // node and its label originate in physics_RT's CRT, so a successful
        // exact destructor + operator-delete round trip checks the standalone
        // ownership contract rather than relying only on the list helper.
        auto *standaloneDrawVector = new IVP_Draw_Vector_Debug();
        constexpr char standaloneLabel[] = "standalone contact normal";
        standaloneDrawVector->debug_text = static_cast<char *>(
            BML::IVP::ABI::Invoke<void *>(
                BML::IVP::ABI::Address::Allocate,
                static_cast<unsigned int>(sizeof(standaloneLabel))));
        ASSERT_NE(standaloneDrawVector->debug_text, nullptr);
        std::memcpy(standaloneDrawVector->debug_text, standaloneLabel,
                    sizeof(standaloneLabel));
        delete standaloneDrawVector;

        ownedEnvironment->destroy();
        EXPECT_EQ(manager->environments.len(), 0);

        alignas(16) std::array<std::byte, sizeof(IVP_Environment)>
            environmentStorage{};
        auto *environment = reinterpret_cast<IVP_Environment *>(
            environmentStorage.data());
        IVP_Standard_Gravity_Controller gravityController;
        environment->standard_gravity_controller = &gravityController;

        const IVP_DOUBLE fixedStep = 1.0 / 120.0;
        environment->set_delta_PSI_time(fixedStep);
        EXPECT_DOUBLE_EQ(environment->delta_PSI_time, fixedStep);
        EXPECT_NEAR(environment->inv_delta_PSI_time, 120.0, 1.0e-12);
        EXPECT_FLOAT_EQ(environment->get_delta_PSI_time(),
                        static_cast<IVP_FLOAT>(fixedStep));
        EXPECT_FLOAT_EQ(environment->get_inv_delta_PSI_time(), 120.0f);

        IVP_U_Point gravity(1.25, -9.81, 0.5);
        environment->set_gravity(&gravity);
        EXPECT_EQ(environment->get_gravity(), &environment->gravity);
        EXPECT_EQ(environment->get_gravity_controller(), &gravityController);
        for (int component = 0; component < 3; ++component) {
            EXPECT_DOUBLE_EQ(environment->gravity.k[component],
                             gravity.k[component]);
            EXPECT_FLOAT_EQ(gravityController.grav_vec.k[component],
                            static_cast<IVP_FLOAT>(gravity.k[component]));
        }
        EXPECT_FLOAT_EQ(environment->gravity_scalar,
                        static_cast<IVP_FLOAT>(std::sqrt(
                            gravity.quad_length())));

        // Ballance keeps the public two-argument Environment signature, but
        // the retained Mindist Settings body consumes only tolerance. Change
        // gravityLength independently, then restore this process-global state.
        const IVP_FLOAT previousTolerance =
            IVP_Environment::get_global_collision_tolerance();
        IVP_Environment::set_global_collision_tolerance(0.0375, 1.0);
        EXPECT_FLOAT_EQ(IVP_Environment::get_global_collision_tolerance(),
                        0.0375f);
        IVP_Environment::set_global_collision_tolerance(0.0375, 250.0);
        EXPECT_FLOAT_EQ(IVP_Environment::get_global_collision_tolerance(),
                        0.0375f);
        IVP_Environment::set_global_collision_tolerance(previousTolerance);
        application.~IVP_Application_Environment();
    }

    // Broadphase look-ahead must scale with both translational and surface
    // rotational speed. This uses the retained Ballance Range Manager bodies
    // on two differently sized moving objects, not a construction-only probe.
    {
        alignas(16) std::array<std::byte, sizeof(IVP_Environment)>
            environmentStorage{};
        auto *environment = reinterpret_cast<IVP_Environment *>(
            environmentStorage.data());
        environment->delta_PSI_time = 0.1;

        alignas(16) std::array<std::byte, sizeof(IVP_Core)> firstCoreStorage{};
        alignas(16) std::array<std::byte, sizeof(IVP_Core)> secondCoreStorage{};
        auto *firstCore = reinterpret_cast<IVP_Core *>(firstCoreStorage.data());
        auto *secondCore = reinterpret_cast<IVP_Core *>(secondCoreStorage.data());
        firstCore->current_speed = 4.0f;
        firstCore->max_surface_rot_speed = 1.0f;
        firstCore->upper_limit_radius = 2.0f;
        secondCore->current_speed = 1.0f;
        secondCore->max_surface_rot_speed = 1.0f;
        secondCore->upper_limit_radius = 4.0f;

        alignas(16) std::array<std::byte, sizeof(IVP_Real_Object)>
            firstObjectStorage{};
        alignas(16) std::array<std::byte, sizeof(IVP_Real_Object)>
            secondObjectStorage{};
        auto *firstObject = reinterpret_cast<IVP_Real_Object *>(
            firstObjectStorage.data());
        auto *secondObject = reinterpret_cast<IVP_Real_Object *>(
            secondObjectStorage.data());
        firstObject->physical_core = firstCore;
        secondObject->physical_core = secondCore;

        IVP_Range_Manager ranges(environment, IVP_FALSE);
        EXPECT_DOUBLE_EQ(ranges.look_ahead_time_world, 1.0);
        EXPECT_NEAR(ranges.look_ahead_max_radius_intra, 0.9, 1.0e-6);
        EXPECT_NEAR(ranges.look_ahead_min_distance_intra, 0.8, 1.0e-6);

        EXPECT_NEAR(ranges.get_coll_range_in_world(firstObject), 4.5, 1.0e-9);

        IVP_DOUBLE firstRange = 0.0;
        IVP_DOUBLE secondRange = 0.0;
        ranges.get_coll_range_intra_objects(
            firstObject, secondObject, &firstRange, &secondRange);
        EXPECT_NEAR(firstRange + secondRange, 1.1, 1.0e-6);
        EXPECT_GT(firstRange, secondRange);
        EXPECT_NEAR(firstRange / secondRange, 5.4 / 2.972, 1.0e-6);

        // The non-owning manager must survive the environment callback.
        ranges.environment_will_be_deleted(environment);
        EXPECT_EQ(ranges.environment, environment);
    }

    // Polygon surface mass properties are a gameplay-facing use of the
    // concrete Surface Manager: object-space center, inertia and broad radius
    // feed Core construction and collision bounds.
    {
        IVP_Compact_Surface surface{};
        const IVP_FLOAT massCenterValues[3] = {1.0f, 2.0f, 3.0f};
        const IVP_FLOAT inertiaValues[3] = {4.0f, 5.0f, 6.0f};
        surface.mass_center.set(massCenterValues);
        surface.rotation_inertia.set(inertiaValues);
        surface.upper_limit_radius = 10.0f;
        surface.factor_and_size = 5u;

        IVP_SurfaceManager_Polygon manager(&surface);
        EXPECT_EQ(manager.get_compact_surface(), &surface);
        EXPECT_EQ(manager.get_type(), IVP_SURMAN_POLYGON);

        IVP_U_Float_Point massCenter;
        IVP_U_Float_Point inertia;
        manager.get_mass_center(&massCenter);
        manager.get_rotation_inertia(&inertia);
        EXPECT_FLOAT_EQ(massCenter.k[0], 1.0f);
        EXPECT_FLOAT_EQ(massCenter.k[1], 2.0f);
        EXPECT_FLOAT_EQ(massCenter.k[2], 3.0f);
        EXPECT_FLOAT_EQ(inertia.k[0], 4.0f);
        EXPECT_FLOAT_EQ(inertia.k[1], 5.0f);
        EXPECT_FLOAT_EQ(inertia.k[2], 6.0f);

        const IVP_U_Float_Point shiftedCenter(4.0f, 6.0f, 3.0f);
        IVP_FLOAT radius = 0.0f;
        IVP_FLOAT deviation = 0.0f;
        manager.get_radius_and_radius_dev_to_given_center(
            &shiftedCenter, &radius, &deviation);
        EXPECT_NEAR(radius, 15.0f, 1.0e-6f);
        EXPECT_NEAR(deviation, 5.2f, 1.0e-6f);

        manager.add_reference_to_ledge(nullptr);
        manager.remove_reference_to_ledge(nullptr);
    }

    // The resolver-free fallback mirrors the retained newest-first object
    // dispatchers. A listener may remove itself during revived dispatch
    // without suppressing the older listener in that same event.
    {
        alignas(16) std::array<std::byte, sizeof(IVP_Environment)>
            environmentStorage{};
        auto *environment = reinterpret_cast<IVP_Environment *>(
            environmentStorage.data());
        IVP_Event_Object event{environment, nullptr};
        std::vector<std::string> events;
        RecordingGlobalObjectListener persistent(
            'A', environment, &events, &event);
        RecordingGlobalObjectListener oneShot(
            'B', environment, &events, &event, true);

        environment->add_listener_object_global(&persistent);
        environment->install_listener_object_global(&persistent);
        environment->add_listener_object_global(&oneShot);
        ASSERT_EQ(environment->global_object_listeners.len(), 2);

        environment->fire_event_object_created(&event);
        environment->fire_event_object_frozen(&event);
        environment->fire_event_object_revived(&event);
        EXPECT_EQ(environment->global_object_listeners.len(), 1);
        environment->fire_event_object_deleted(&event);

        EXPECT_EQ(events, (std::vector<std::string>{
            "CB", "CA", "FB", "FA", "RB", "RA", "DA"}));
        environment->remove_listener_object_global(&persistent);
        EXPECT_EQ(environment->global_object_listeners.len(), 0);
        environment->global_object_listeners
            .~IVP_U_Vector<IVP_Listener_Object>();
    }

    // RVA 0x13650 addresses collision_delegator_roots, not the adjacent
    // global object-listener vector. Its slot-2 call also proves that the two
    // later spawned-mindist hooks are absent from Ballance's base vtable.
    {
        alignas(16) std::array<std::byte, sizeof(IVP_Environment)>
            environmentStorage{};
        alignas(16) std::array<std::byte, sizeof(IVP_Real_Object)>
            objectStorage{};
        auto *environment = reinterpret_cast<IVP_Environment *>(
            environmentStorage.data());
        auto *object = reinterpret_cast<IVP_Real_Object *>(
            objectStorage.data());
        std::vector<char> events;
        RecordingCollisionDelegatorRoot first('A', &events, object);
        RecordingCollisionDelegatorRoot second('B', &events, object);

        environment->collision_delegator_roots.add(&first);
        environment->collision_delegator_roots.add(&second);
        environment->fire_object_is_removed_from_collision_detection(object);
        EXPECT_EQ(events, (std::vector<char>{'B', 'A'}));

        environment->collision_delegator_roots
            .~IVP_U_Vector<IVP_Collision_Delegator_Root>();
    }

    // Ballance keeps one manager-owned PSI event in every time manager. Add
    // two gameplay events around it and exercise the retained Min List path:
    // insertion stores the 16-bit retail handle in IVP_Time_Event::index,
    // update_event repairs that handle after a remove/add, and chronological
    // priority is preserved across both operations. Remove stack-owned events
    // before manager destruction because the retail manager owns everything
    // still present in its queue.
    {
        IVP_Time_Manager manager;
        RecordingTimeEvent lateEvent;
        RecordingTimeEvent earlyEvent;

        ASSERT_NE(manager.min_hash, nullptr);
        ASSERT_NE(manager.psi_event, nullptr);
        EXPECT_EQ(manager.get_event_count(), 1);
        EXPECT_EQ(manager.min_hash->find_min_elem(), manager.psi_event);
        EXPECT_FLOAT_EQ(manager.min_hash->find_min_value(), 0.0f);

        manager.insert_event(&lateEvent, IVP_Time(12.5));
        manager.insert_event(&earlyEvent, IVP_Time(-3.25));
        EXPECT_EQ(manager.get_event_count(), 3);
        EXPECT_NE(lateEvent.index, IVP_U_MINLIST_UNUSED);
        EXPECT_NE(earlyEvent.index, IVP_U_MINLIST_UNUSED);
        EXPECT_NE(lateEvent.index, earlyEvent.index);
        EXPECT_EQ(manager.min_hash->find_min_elem(), &earlyEvent);
        EXPECT_FLOAT_EQ(manager.min_hash->find_min_value(), -3.25f);

        manager.update_event(&lateEvent, IVP_Time(-7.5));
        EXPECT_EQ(manager.get_event_count(), 3);
        EXPECT_EQ(manager.min_hash->find_min_elem(), &lateEvent);
        EXPECT_FLOAT_EQ(manager.min_hash->find_min_value(), -7.5f);

        const IVP_U_MINLIST_INDEX lateIndex = lateEvent.index;
        manager.remove_event(&lateEvent);
        EXPECT_EQ(manager.get_event_count(), 2);
        EXPECT_EQ(lateEvent.index, lateIndex);
        EXPECT_EQ(manager.min_hash->find_min_elem(), &earlyEvent);

        const IVP_U_MINLIST_INDEX earlyIndex = earlyEvent.index;
        manager.remove_event(&earlyEvent);
        EXPECT_EQ(manager.get_event_count(), 1);
        EXPECT_EQ(earlyEvent.index, earlyIndex);
        EXPECT_EQ(manager.min_hash->find_min_elem(), manager.psi_event);
        EXPECT_EQ(lateEvent.calls, 0);
        EXPECT_EQ(earlyEvent.calls, 0);
    }

    // The anomaly callbacks are Ballance's last-resort protection against a
    // core acquiring an unstable speed during a PSI. Verify the retail
    // defaults and both distinct limits: linear speed is capped to 99% of the
    // configured absolute limit, while angular speed is capped to 90% of the
    // per-PSI limit converted through the environment's inverse PSI duration.
    {
        IVP_Anomaly_Limits limits(IVP_FALSE);
        IVP_Anomaly_Manager manager(IVP_FALSE);
        EXPECT_FLOAT_EQ(limits.max_velocity, 2000.0f);
        EXPECT_EQ(limits.max_collisions_per_psi, 70000);
        EXPECT_FLOAT_EQ(limits.max_angular_velocity_per_psi,
                        1.5707963705062866f);

        limits.max_velocity = 2.0f;
        IVP_U_Float_Point linearVelocity(3.0f, 4.0f, 0.0f);
        manager.max_velocity_exceeded(
            &limits, nullptr, &linearVelocity);
        EXPECT_NEAR(linearVelocity.k[0], 1.188f, 1.0e-6f);
        EXPECT_NEAR(linearVelocity.k[1], 1.584f, 1.0e-6f);
        EXPECT_FLOAT_EQ(linearVelocity.k[2], 0.0f);

        alignas(16) std::array<std::byte, sizeof(IVP_Environment)>
            environmentStorage{};
        alignas(16) std::array<std::byte, sizeof(IVP_Core)> coreStorage{};
        auto *environment = reinterpret_cast<IVP_Environment *>(
            environmentStorage.data());
        auto *core = reinterpret_cast<IVP_Core *>(coreStorage.data());
        environment->inv_delta_PSI_time = 100.0;
        core->environment = environment;
        limits.max_angular_velocity_per_psi = 0.5f;

        IVP_U_Float_Point angularVelocity(0.0f, 0.0f, 60.0f);
        manager.max_angular_velocity_exceeded(
            &limits, core, &angularVelocity);
        EXPECT_FLOAT_EQ(angularVelocity.k[0], 0.0f);
        EXPECT_FLOAT_EQ(angularVelocity.k[1], 0.0f);
        EXPECT_NEAR(angularVelocity.k[2], 45.0f, 1.0e-5f);
        EXPECT_EQ(manager.max_collisions_exceeded_check_freezing(
                      &limits, core),
                  IVP_TRUE);
    }

    // Collision normals and friction directions repeatedly normalize vectors
    // through these retained fast inverse-square-root bodies. Exercise a
    // 3-4-5 direction and a non-perfect square so the test checks the retail
    // IEEE exponent seed plus Newton refinement, not only an exact power of 2.
    {
        const IVP_FLOAT inverseLengthFloat =
            IVP_Inline_Math::isqrt_float(25.0f);
        EXPECT_NEAR(inverseLengthFloat, 0.2f, 2.0e-7f);
        IVP_U_Float_Point normalizedFloat(
            3.0f * inverseLengthFloat,
            4.0f * inverseLengthFloat, 0.0f);
        EXPECT_NEAR(normalizedFloat.quad_length(), 1.0f, 2.0e-6f);

        const IVP_DOUBLE inverseLengthDouble =
            IVP_Inline_Math::isqrt_double(7.25);
        const IVP_DOUBLE retailInverseLengthDouble = static_cast<double>(
            1.0f / std::sqrt(7.25f));
        EXPECT_DOUBLE_EQ(inverseLengthDouble,
                         retailInverseLengthDouble);
        EXPECT_NEAR(7.25 * inverseLengthDouble * inverseLengthDouble,
                    1.0, 4.0e-8);
    }

    // These two managers are embedded in every Ballance environment.  The
    // retained constructors establish the freeze cadence and clear the full
    // statistics window before the environment stores its backlink.
    {
        IVP_Freeze_Manager freezeManager;
        EXPECT_FLOAT_EQ(freezeManager.freeze_check_dtime, 0.3f);
        freezeManager.freeze_check_dtime = 9.0f;
        freezeManager.init_freeze_manager();
        EXPECT_FLOAT_EQ(freezeManager.freeze_check_dtime, 0.3f);

        IVP_Statistic_Manager statistics;
        const auto *statisticsBytes = reinterpret_cast<const std::byte *>(
            &statistics);
        for (std::size_t index = 0; index < sizeof(statistics); ++index) {
            EXPECT_EQ(statisticsBytes[index], std::byte{0}) << index;
        }

        // clear_statistic starts a new reporting interval but intentionally
        // preserves the lifetime energy/mindist totals and global counter.
        statistics.max_rescue_speed = 12.0f;
        statistics.impact_counter = 7;
        statistics.impact_coll_checks = 19;
        statistics.sum_energy_destr = 4.5;
        statistics.sum_of_mindists = 23;
        statistics.global_fmd_counter = 29;
        statistics.clear_statistic();
        EXPECT_FLOAT_EQ(statistics.max_rescue_speed, 0.0f);
        EXPECT_EQ(statistics.impact_counter, 0);
        EXPECT_EQ(statistics.impact_coll_checks, 0);
        EXPECT_DOUBLE_EQ(statistics.sum_energy_destr, 4.5);
        EXPECT_EQ(statistics.sum_of_mindists, 23);
        EXPECT_EQ(statistics.global_fmd_counter, 29);
    }

    // update_float/update_int are retained secondary-base implementations:
    // their ECX values must point at the Delayed subobjects rather than the
    // complete terminal objects. Exercise direct public calls and require the
    // original DLL to propagate both values to real listener vtables.
    {
        IVP_U_Active_Value activeValue("controller_parameter");
        IVP_U_Active_Terminal_Double activeDouble("gravity_scale", 1.0);
        IVP_U_Active_Terminal_Int activeInt("gravity_enabled", 1);
        RecordingActiveFloatListener floatListener;
        RecordingActiveIntListener intListener;
        activeDouble.add_reference();
        activeInt.add_reference();
        activeDouble.add_dependency(&floatListener);
        activeInt.add_dependency(&intListener);

        EXPECT_STREQ(activeValue.get_name(), "controller_parameter");
        EXPECT_STREQ(activeDouble.get_name(), "gravity_scale");
        EXPECT_STREQ(activeInt.get_name(), "gravity_enabled");
        EXPECT_DOUBLE_EQ(activeDouble.give_double_value(), 1.0);
        EXPECT_EQ(activeInt.give_int_value(), 1);

        activeDouble.double_value = 0.625;
        activeDouble.update_float();
        EXPECT_EQ(floatListener.calls, 1);
        EXPECT_EQ(floatListener.last_value, &activeDouble);
        EXPECT_DOUBLE_EQ(activeDouble.give_double_value(), 0.625);

        activeInt.int_value = 0;
        activeInt.update_int();
        EXPECT_EQ(intListener.calls, 1);
        EXPECT_EQ(intListener.last_value, &activeInt);
        EXPECT_EQ(activeInt.give_int_value(), 0);

        // An unchanged value must not emit a second dependency notification.
        activeDouble.update_float();
        activeInt.update_int();
        EXPECT_EQ(floatListener.calls, 1);
        EXPECT_EQ(intListener.calls, 1);

        activeDouble.remove_dependency(&floatListener);
        activeInt.remove_dependency(&intListener);

        // The retained Spring Active constructor/destructor call the anonymous
        // shared bodies emitted for these public Float dependency operations.
        // Use those bodies as an oracle for the reference and callback graph,
        // while the first pair above exercises the reconstructed API itself.
        using ActiveFloatDependencyBody = void(__thiscall *)(
            IVP_U_Active_Float *, IVP_U_Active_Float_Listener *);
        const auto exactAddDependency =
            reinterpret_cast<ActiveFloatDependencyBody>(
                gRetailModuleBase + kActiveFloatAddDependencyInlineRva);
        const auto exactRemoveDependency =
            reinterpret_cast<ActiveFloatDependencyBody>(
                gRetailModuleBase + kActiveFloatRemoveDependencyInlineRva);
        IVP_U_Active_Terminal_Double retailDependency(
            "spring_force", 2.0);
        RecordingActiveFloatListener retailListener;
        retailDependency.add_reference();
        const auto referenceCount = [](const IVP_U_Active_Value &value) {
            std::int32_t count = 0;
            std::memcpy(
                &count,
                reinterpret_cast<const std::byte *>(&value) + 0x08,
                sizeof(count));
            return count;
        };
        EXPECT_EQ(referenceCount(retailDependency), 1);
        exactAddDependency(&retailDependency, &retailListener);
        EXPECT_EQ(referenceCount(retailDependency), 2);
        retailDependency.double_value = 3.5;
        retailDependency.update_float();
        EXPECT_EQ(retailListener.calls, 1);
        EXPECT_EQ(retailListener.last_value, &retailDependency);
        exactRemoveDependency(&retailDependency, &retailListener);
        EXPECT_EQ(referenceCount(retailDependency), 1);
        retailDependency.double_value = 4.0;
        retailDependency.update_float();
        EXPECT_EQ(retailListener.calls, 1);
    }

    // The exact constructor intentionally leaves material_type and the second
    // friction value untouched. Verify that the public wrapper does not
    // pre-initialize storage before dispatching RVA 0xBF00.
    alignas(IVP_Material_Simple)
        std::array<std::byte, sizeof(IVP_Material_Simple)> materialStorage;
    std::fill(materialStorage.begin(), materialStorage.end(), std::byte{0xA5});
    auto *poisonedMaterial = ::new (materialStorage.data())
        IVP_Material_Simple(0.72, 0.18);
    for (std::size_t offset = 0x04; offset < 0x08; ++offset)
        EXPECT_EQ(materialStorage[offset], std::byte{0xA5});
    for (std::size_t offset = 0x18; offset < 0x20; ++offset)
        EXPECT_EQ(materialStorage[offset], std::byte{0xA5});
    EXPECT_EQ(poisonedMaterial->second_friction_x_enabled, IVP_FALSE);
    EXPECT_NEAR(poisonedMaterial->get_friction_factor(), 0.72, 1.0e-12);
    EXPECT_NEAR(poisonedMaterial->get_elasticity(), 0.18, 1.0e-12);
    EXPECT_NEAR(poisonedMaterial->get_adhesion(), 0.0, 1.0e-12);
    poisonedMaterial->~IVP_Material_Simple();

    IVP_Material_Simple material(0.72, 0.18);
    material.second_friction_x = 0.0;
    EXPECT_NEAR(material.get_friction_factor(), 0.72, 1.0e-12);
    EXPECT_NEAR(material.get_second_friction_factor(), 0.0, 1.0e-12);
    EXPECT_NEAR(material.get_elasticity(), 0.18, 1.0e-12);
    EXPECT_NEAR(material.get_adhesion(), 0.0, 1.0e-12);
    EXPECT_STREQ(material.get_name(), "Simple material");

    // Contact resolution asks the material manager to combine both surfaces,
    // so exercise the actual IVP_Contact_Situation offsets and both material
    // vtables. Ballance multiplies friction and elasticity but adds adhesion.
    // The default indexed material is a process-owned singleton independent
    // of compact-triangle index and query position.
    {
        IVP_Material_Manager materialManager(IVP_FALSE);
        IVP_Material_Simple ballMaterial(0.72, 0.18);
        IVP_Material_Simple floorMaterial(0.5, 0.4);
        ballMaterial.adhesion = 0.125;
        floorMaterial.adhesion = 0.375;
        EXPECT_NEAR(ballMaterial.get_friction_factor(), 0.72, 1.0e-12);
        EXPECT_NEAR(ballMaterial.get_elasticity(), 0.18, 1.0e-12);
        EXPECT_NEAR(floorMaterial.get_friction_factor(), 0.5, 1.0e-12);
        EXPECT_NEAR(floorMaterial.get_elasticity(), 0.4, 1.0e-12);

        IVP_Contact_Situation contact{};
        contact.materials[0] = &ballMaterial;
        contact.materials[1] = &floorMaterial;
        const double retailFriction = static_cast<double>(0.36f);
        const double retailElasticity = static_cast<double>(0.072f);
        EXPECT_DOUBLE_EQ(materialManager.get_friction_factor(&contact),
                         retailFriction);
        EXPECT_DOUBLE_EQ(materialManager.get_elasticity(&contact),
                         retailElasticity);
        EXPECT_NEAR(materialManager.get_adhesion(&contact),
                    0.5, 1.0e-12);

        const IVP_U_Point worldPosition(17.0, -4.0, 2.5);
        IVP_Material *firstDefault =
            materialManager.get_material_by_index(&worldPosition, 0);
        IVP_Material *otherIndex =
            materialManager.get_material_by_index(nullptr, 173);
        ASSERT_NE(firstDefault, nullptr);
        EXPECT_EQ(otherIndex, firstDefault);
        EXPECT_NEAR(firstDefault->get_friction_factor(), 0.5, 1.0e-12);
        EXPECT_NEAR(firstDefault->get_elasticity(), 0.5, 1.0e-12);
        EXPECT_NEAR(firstDefault->get_adhesion(), 0.0, 1.0e-12);

        // A non-owning manager must survive the environment callback and
        // remain usable; testing the owning branch would deliberately delete
        // the receiver and is not safe for a stack-owned fixture.
        materialManager.environment_will_be_deleted(nullptr);
        EXPECT_DOUBLE_EQ(materialManager.get_friction_factor(&contact),
                         retailFriction);
    }

    // Template names cross the retail allocator boundary: set_name owns a
    // duplicated string, replacement releases the previous duplicate, and the
    // destructor releases the final one. Mutating the caller buffer must not
    // alter the name retained for subsequent object creation.
    {
        IVP_Template_Object namedTemplate;
        EXPECT_EQ(namedTemplate.get_name(), nullptr);
        std::array<char, 16> firstName{
            'B', 'a', 'l', 'l', '_', 'W', 'o', 'o', 'd', '\0'};
        namedTemplate.set_name(firstName.data());
        ASSERT_NE(namedTemplate.get_name(), nullptr);
        EXPECT_STREQ(namedTemplate.get_name(), "Ball_Wood");
        firstName[0] = 'X';
        EXPECT_STREQ(namedTemplate.get_name(), "Ball_Wood");
        namedTemplate.set_name("P_Extra_Life");
        EXPECT_STREQ(namedTemplate.get_name(), "P_Extra_Life");
    }

    // Default trigger volume policy used when a real object is converted to
    // a phantom. The retail constructor keeps membership tracking disabled
    // until requested and initializes symmetric exit hysteresis.
    {
        alignas(IVP_Template_Phantom)
            std::array<std::byte, sizeof(IVP_Template_Phantom)>
                phantomTemplateStorage;
        std::fill(phantomTemplateStorage.begin(),
                  phantomTemplateStorage.end(), std::byte{0xA5});
        auto &phantomTemplate = *::new (phantomTemplateStorage.data())
            IVP_Template_Phantom();
        EXPECT_EQ(phantomTemplate.manage_intruding_objects, IVP_FALSE);
        EXPECT_EQ(phantomTemplate.manage_intruding_cores, IVP_FALSE);
        EXPECT_EQ(phantomTemplate.dont_check_for_unmoveables, IVP_FALSE);
        EXPECT_FLOAT_EQ(phantomTemplate.exit_policy_extra_radius, 0.5f);
        EXPECT_FLOAT_EQ(phantomTemplate.exit_policy_extra_time, 0.5f);
        phantomTemplate.manage_intruding_objects = IVP_TRUE;
        phantomTemplate.exit_policy_extra_radius = 0.75f;
        EXPECT_EQ(phantomTemplate.manage_intruding_objects, IVP_TRUE);
        EXPECT_FLOAT_EQ(phantomTemplate.exit_policy_extra_radius, 0.75f);
        phantomTemplate.~IVP_Template_Phantom();
    }

    // A Ballance trigger volume records each penetrating mindist and notifies
    // Phantom listeners on both transitions. Keep optional object/Core
    // aggregation disabled so this isolates the retained membership contract
    // without fabricating Real Object or Mindist internals.
    {
        RawPhantomController rawController;
        auto *controller = reinterpret_cast<IVP_Controller_Phantom *>(
            &rawController);
        RecordingPhantomVolumeListener listener;
        controller->add_listener_phantom(&listener);

        std::uint32_t mindistIdentity = 0x4D494E44u;
        auto *mindist = reinterpret_cast<IVP_Mindist *>(
            &mindistIdentity);
        auto *mindistBase = reinterpret_cast<IVP_Mindist_Base *>(mindist);

        EXPECT_EQ(rawController.set_of_mindists.n_elems(), 0);
        PhantomVolumeAccess::Enter(controller, mindist);
        EXPECT_EQ(rawController.set_of_mindists.n_elems(), 1);
        EXPECT_EQ(rawController.set_of_mindists.find_element(mindistBase),
                  mindistBase);
        EXPECT_EQ(listener.mindist_entered, 1);
        EXPECT_EQ(listener.last_controller, controller);
        EXPECT_EQ(listener.last_mindist, mindistBase);

        PhantomVolumeAccess::Leave(controller, mindist);
        EXPECT_EQ(rawController.set_of_mindists.n_elems(), 0);
        EXPECT_EQ(rawController.set_of_mindists.find_element(mindistBase),
                  nullptr);
        EXPECT_EQ(listener.mindist_left, 1);
        EXPECT_EQ(listener.last_controller, controller);
        EXPECT_EQ(listener.last_mindist, mindistBase);
        controller->remove_listener_phantom(&listener);
    }

    // Build the object half of a Ballance ball/polygon creation request with
    // the retained template constructor. The no-collision identifier is what
    // gameplay uses to suppress contacts inside a related object group.
    {
        alignas(IVP_Template_Real_Object)
            std::array<std::byte, sizeof(IVP_Template_Real_Object)>
                objectTemplateStorage;
        std::fill(objectTemplateStorage.begin(), objectTemplateStorage.end(),
                  std::byte{0xA5});
        auto &objectTemplate = *::new (objectTemplateStorage.data())
            IVP_Template_Real_Object();
        EXPECT_EQ(objectTemplate.get_name(), nullptr);
        EXPECT_STREQ(objectTemplate.get_nocoll_group_ident(), "");
        EXPECT_EQ(objectTemplate.physical_unmoveable, IVP_FALSE);
        EXPECT_EQ(objectTemplate.enable_piling_optimization, IVP_FALSE);
        EXPECT_EQ(objectTemplate.material, nullptr);
        EXPECT_DOUBLE_EQ(objectTemplate.mass, 1.0);
        EXPECT_EQ(objectTemplate.rot_inertia_is_factor, IVP_TRUE);
        EXPECT_FLOAT_EQ(objectTemplate.rot_inertia.k[0], 1.0f);
        EXPECT_FLOAT_EQ(objectTemplate.rot_inertia.k[1], 1.0f);
        EXPECT_FLOAT_EQ(objectTemplate.rot_inertia.k[2], 1.0f);
        EXPECT_FLOAT_EQ(objectTemplate.auto_check_rot_inertia, 0.03f);
        constexpr IVP_DOUBLE retailDampingDefault =
            static_cast<IVP_DOUBLE>(0.01f);
        EXPECT_DOUBLE_EQ(objectTemplate.speed_damp_factor,
                         retailDampingDefault);
        EXPECT_DOUBLE_EQ(objectTemplate.rot_speed_damp_factor.k[0],
                         retailDampingDefault);
        EXPECT_DOUBLE_EQ(objectTemplate.rot_speed_damp_factor.k[1],
                         retailDampingDefault);
        EXPECT_DOUBLE_EQ(objectTemplate.rot_speed_damp_factor.k[2],
                         retailDampingDefault);
        EXPECT_FLOAT_EQ(objectTemplate.extra_radius, 0.0f);
        EXPECT_EQ(objectTemplate.mass_center_override, nullptr);
        EXPECT_EQ(objectTemplate.client_data, nullptr);

        objectTemplate.mass = 3.5;
        objectTemplate.material = &material;
        objectTemplate.set_nocoll_group_ident("BMLTEST");
        EXPECT_DOUBLE_EQ(objectTemplate.mass, 3.5);
        EXPECT_EQ(objectTemplate.material, &material);
        EXPECT_STREQ(objectTemplate.get_nocoll_group_ident(), "BMLTEST");
        objectTemplate.set_nocoll_group_ident(nullptr);
        EXPECT_STREQ(objectTemplate.get_nocoll_group_ident(), "");
        objectTemplate.~IVP_Template_Real_Object();
    }

    // Ballance resolves named active values and surface-side objects through
    // this exact retail hash. Exercise collision-prone gameplay names, the
    // not-found sentinel and the reconstructed removal/destruction ownership
    // path over a table allocated by physics_RT itself.
    {
        int missingSurface = 0;
        int ballWoodSurface = 1;
        int ballPaperSurface = 2;
        alignas(IVP_U_String_Hash)
            std::array<std::byte, sizeof(IVP_U_String_Hash)> hashStorage;
        std::fill(hashStorage.begin(), hashStorage.end(), std::byte{0xA5});
        auto &surfaceNames = *::new (hashStorage.data())
            IVP_U_String_Hash(16, &missingSurface);

        EXPECT_EQ(surfaceNames.size, 16);
        EXPECT_EQ(surfaceNames.not_found_value, &missingSurface);
        ASSERT_NE(surfaceNames.elems, nullptr);
        EXPECT_EQ(surfaceNames.find("Ball_Wood"), &missingSurface);
        surfaceNames.add("Ball_Wood", &ballWoodSurface);
        surfaceNames.add("Ball_Paper", &ballPaperSurface);
        EXPECT_EQ(surfaceNames.find("Ball_Wood"), &ballWoodSurface);
        EXPECT_EQ(surfaceNames.find("Ball_Paper"), &ballPaperSurface);
        surfaceNames.remove("Ball_Wood");
        EXPECT_EQ(surfaceNames.find("Ball_Wood"), &missingSurface);
        EXPECT_EQ(surfaceNames.find("Ball_Paper"), &ballPaperSurface);
        surfaceNames.~IVP_U_String_Hash();
    }

    IVP_U_Float_Hesse sourcePlane(0.0, 1.0, 0.0, -2.5);
    IVP_U_Float_Point sourceCurrent(0.25, 0.0, -0.5);
    IVP_Liquid_Surface_Descriptor_Simple liquid(
        &sourcePlane, &sourceCurrent);
    IVP_U_Float_Hesse outputPlane;
    IVP_U_Float_Point outputCurrent;
    liquid.calc_liquid_surface(
        nullptr, nullptr, &outputPlane, &outputCurrent);
    EXPECT_NEAR(outputPlane.k[0], 0.0f, 1.0e-6f);
    EXPECT_NEAR(outputPlane.k[1], 1.0f, 1.0e-6f);
    EXPECT_NEAR(outputPlane.k[2], 0.0f, 1.0e-6f);
    EXPECT_NEAR(outputPlane.hesse_val, -2.5f, 1.0e-6f);
    EXPECT_NEAR(outputCurrent.k[0], 0.25f, 1.0e-6f);
    EXPECT_NEAR(outputCurrent.k[1], 0.0f, 1.0e-6f);
    EXPECT_NEAR(outputCurrent.k[2], -0.5f, 1.0e-6f);

}
