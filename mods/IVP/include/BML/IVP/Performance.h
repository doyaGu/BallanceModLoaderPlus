#ifndef BML_IVP_PERFORMANCE_H
#define BML_IVP_PERFORMANCE_H

#include "BML/IVP/Types.h"

#include <cstddef>
#include <cstdio>
#include <cstdint>

class IVP_Environment;
enum IVP_BETTERSTATISTICSMANAGER_DATA_ENTITY_TYPE : std::int32_t {
    INT_VALUE = 1,
    DOUBLE_VALUE = 2,
    INT_ARRAY = 3,
    DOUBLE_ARRAY = 4,
    STRING = 5,
};

class IVP_BetterStatisticsmanager_Data_Int_Array {
public:
    int size;
    int *array;
    int max_value;
    int xpos;
    int ypos;
    int width;
    int height;
    int bg_color;
    int border_color;
    int graph_color;
};

class IVP_BetterStatisticsmanager_Data_Double_Array {
public:
    int size;
    IVP_DOUBLE *array;
    IVP_DOUBLE max_value;
    int xpos;
    int ypos;
    int width;
    int height;
    int bg_color;
    int border_color;
    int graph_color;
};

class IVP_BetterStatisticsmanager_Data_Entity {
private:
    IVP_BOOL enabled;

public:
    IVP_BETTERSTATISTICSMANAGER_DATA_ENTITY_TYPE type;
    union {
        int int_value;
        IVP_DOUBLE double_value;
        IVP_BetterStatisticsmanager_Data_Int_Array int_array;
        IVP_BetterStatisticsmanager_Data_Double_Array double_array;
    } data;
    char *text;
    int text_color;
    int xpos;
    int ypos;

    void enable() { enabled = IVP_TRUE; }
    void disable() { enabled = IVP_FALSE; }
    IVP_BOOL get_state() { return enabled; }

    void set_int_value(int value) { data.int_value = value; }
    void set_double_value(IVP_DOUBLE value) { data.double_value = value; }

    void set_array_size(int arraySize) {
        if (type == INT_ARRAY) {
            BML::IVP::ABI::Invoke<void>(
                BML::IVP::ABI::Address::Free, data.int_array.array);
            data.int_array.array =
                BML::IVP::ABI::Invoke<int *>(
                    BML::IVP::ABI::Address::AllocateZeroed,
                    arraySize + 1, static_cast<int>(sizeof(int)));
            data.int_array.size = arraySize;
        } else if (type == DOUBLE_ARRAY) {
            BML::IVP::ABI::Invoke<void>(
                BML::IVP::ABI::Address::Free, data.double_array.array);
            data.double_array.array =
                BML::IVP::ABI::Invoke<IVP_DOUBLE *>(
                    BML::IVP::ABI::Address::AllocateZeroed,
                    arraySize + 1, static_cast<int>(sizeof(IVP_DOUBLE)));
            data.double_array.size = arraySize;
        }
    }

    void set_int_array_latest_value(int value) {
        if (!data.int_array.array || data.int_array.size <= 0)
            return;
        for (int index = 0; index + 1 < data.int_array.size; ++index)
            data.int_array.array[index] = data.int_array.array[index + 1];
        data.int_array.array[data.int_array.size - 1] = value;
    }

    void set_double_array_latest_value(IVP_DOUBLE value) {
        if (!data.double_array.array || data.double_array.size <= 0)
            return;
        for (int index = 0; index + 1 < data.double_array.size; ++index)
            data.double_array.array[index] = data.double_array.array[index + 1];
        data.double_array.array[data.double_array.size - 1] = value;
    }

    void set_text(const char *newText) {
        char *replacement = BML::IVP::ABI::Invoke<char *>(
            BML::IVP::ABI::Address::DuplicateString, newText);
        if (!replacement)
            return;
        BML::IVP::ABI::Invoke<void>(
            BML::IVP::ABI::Address::Free, text);
        text = replacement;
    }

    void set_position(int x, int y) {
        xpos = x;
        ypos = y;
    }

    explicit IVP_BetterStatisticsmanager_Data_Entity(
        IVP_BETTERSTATISTICSMANAGER_DATA_ENTITY_TYPE dataType)
        : enabled(IVP_TRUE), type(dataType), text(nullptr), text_color(0),
          xpos(0), ypos(0) {
        text = BML::IVP::ABI::Invoke<char *>(
            BML::IVP::ABI::Address::DuplicateString, "Value: ");
        switch (type) {
        case INT_VALUE:
            data.int_value = 0;
            break;
        case DOUBLE_VALUE:
            data.double_value = 0.0;
            break;
        case INT_ARRAY:
            data.int_array = {10, nullptr, 10, 0, 0, 10, 10, 0, 1, 2};
            data.int_array.array = BML::IVP::ABI::Invoke<int *>(
                BML::IVP::ABI::Address::AllocateZeroed, 11,
                static_cast<int>(sizeof(int)));
            break;
        case DOUBLE_ARRAY:
            data.double_array.size = 10;
            data.double_array.array = BML::IVP::ABI::Invoke<IVP_DOUBLE *>(
                BML::IVP::ABI::Address::AllocateZeroed, 11,
                static_cast<int>(sizeof(IVP_DOUBLE)));
            data.double_array.max_value = 10.0;
            data.double_array.width = 10;
            data.double_array.height = 10;
            data.double_array.xpos = 0;
            data.double_array.ypos = 0;
            // Preserve the neighboring revision's actual union-member writes:
            // these land one int earlier than the Double Array color fields.
            data.int_array.bg_color = 0;
            data.int_array.border_color = 1;
            data.int_array.graph_color = 2;
            break;
        case STRING:
            break;
        }
    }

    ~IVP_BetterStatisticsmanager_Data_Entity() {
        BML::IVP::ABI::Invoke<void>(BML::IVP::ABI::Address::Free, text);
    }
};

class IVP_BetterStatisticsmanager_Callback_Interface {
public:
    virtual void output_request(
        IVP_BetterStatisticsmanager_Data_Entity *entity) = 0;
    virtual void enable() = 0;
    virtual void disable() = 0;
};

class IVP_Statisticsmanager_Console_Callback
    : public IVP_BetterStatisticsmanager_Callback_Interface {
private:
    void output_request(
        IVP_BetterStatisticsmanager_Data_Entity *entity) override {
        switch (entity->type) {
        case INT_VALUE:
            std::printf("%s%d\n", entity->text, entity->data.int_value);
            break;
        case DOUBLE_VALUE:
            std::printf("%s%f\n", entity->text, entity->data.double_value);
            break;
        case INT_ARRAY:
            std::printf("%s\n", entity->text);
            for (int index = 0; index < entity->data.int_array.size; ++index)
                std::printf("%d\n", entity->data.int_array.array[index]);
            break;
        case DOUBLE_ARRAY:
            std::printf("%s\n", entity->text);
            for (int index = 0; index < entity->data.double_array.size; ++index)
                std::printf("%f\n", entity->data.double_array.array[index]);
            break;
        case STRING:
            std::printf("%s\n", entity->text);
            break;
        }
    }

    void enable() override {}
    void disable() override {}

public:
    IVP_Statisticsmanager_Console_Callback() = default;
};

class IVP_BetterStatisticsmanager {
    friend struct BML_IvpBetterStatisticsmanagerLayoutCheck;

private:
    IVP_BOOL enabled;
    union {
        IVP_U_Vector<IVP_BetterStatisticsmanager_Callback_Interface>
            output_callbacks;
    };
    union {
        IVP_U_Vector<IVP_BetterStatisticsmanager_Data_Entity> data_entities;
    };
    IVP_DOUBLE simulation_time;

public:
    IVP_BOOL update_delayed;
    IVP_DOUBLE update_interval;

    void print() {
        if (!enabled || update_delayed)
            return;
        for (int callbackIndex = 0;
             callbackIndex < output_callbacks.len(); ++callbackIndex) {
            auto *callback = output_callbacks.element_at(callbackIndex);
            for (int entityIndex = 0;
                 entityIndex < data_entities.len(); ++entityIndex) {
                auto *entity = data_entities.element_at(entityIndex);
                if (entity->get_state())
                    callback->output_request(entity);
            }
        }
    }

    void set_simulation_time(IVP_DOUBLE time) {
        simulation_time = time;
        static IVP_DOUBLE timeOfLastUpdate = 0.0;
        if (simulation_time - timeOfLastUpdate < update_interval) {
            update_delayed = IVP_TRUE;
            return;
        }
        timeOfLastUpdate = simulation_time;
        update_delayed = IVP_FALSE;
    }

    void install_data_entity(
        IVP_BetterStatisticsmanager_Data_Entity *entity) {
        data_entities.add(entity);
    }
    void remove_data_entity(
        IVP_BetterStatisticsmanager_Data_Entity *entity) {
        const int index = data_entities.index_of(entity);
        if (index >= 0)
            data_entities.remove_at(index);
    }
    void install_output_callback(
        IVP_BetterStatisticsmanager_Callback_Interface *callback) {
        output_callbacks.add(callback);
    }
    void remove_output_callback(
        IVP_BetterStatisticsmanager_Callback_Interface *callback) {
        const int index = output_callbacks.index_of(callback);
        if (index >= 0)
            output_callbacks.remove_at(index);
    }

    void enable() { enabled = IVP_TRUE; }
    void disable() { enabled = IVP_FALSE; }
    IVP_BOOL get_state() { return enabled; }

    IVP_BetterStatisticsmanager() {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::BetterStatisticsmanagerCtor, this,
            [this]() {
                ::new (static_cast<void *>(&output_callbacks))
                    IVP_U_Vector<
                        IVP_BetterStatisticsmanager_Callback_Interface>();
                ::new (static_cast<void *>(&data_entities))
                    IVP_U_Vector<
                        IVP_BetterStatisticsmanager_Data_Entity>();
                enabled = IVP_TRUE;
                update_interval = 1.0;
                update_delayed = IVP_TRUE;
            });
    }
    ~IVP_BetterStatisticsmanager() {
        data_entities.~IVP_U_Vector<
            IVP_BetterStatisticsmanager_Data_Entity>();
        output_callbacks.~IVP_U_Vector<
            IVP_BetterStatisticsmanager_Callback_Interface>();
    }
};

struct BML_IvpBetterStatisticsmanagerLayoutCheck {
    static constexpr std::size_t enabled =
        offsetof(IVP_BetterStatisticsmanager, enabled);
    static constexpr std::size_t callbacks =
        offsetof(IVP_BetterStatisticsmanager, output_callbacks);
    static constexpr std::size_t entities =
        offsetof(IVP_BetterStatisticsmanager, data_entities);
    static constexpr std::size_t simulation_time =
        offsetof(IVP_BetterStatisticsmanager, simulation_time);
};

enum IVP_PERFORMANCE_ELEMENT : std::int32_t {
    IVP_PE_PSI_START = 0,
    IVP_PE_PSI_UNIVERSE,
    IVP_PE_PSI_CONTROLLERS,
    IVP_PE_PSI_INTEGRATORS,
    IVP_PE_PSI_HULL,
    IVP_PE_PSI_SHORT_MINDISTS,
    IVP_PE_PSI_CRITICAL_MINDISTS,
    IVP_PE_PSI_END,
    IVP_PE_AT_INIT,
    IVP_PE_AT_COLLISION,
    IVP_PE_AT_INTEGRATORS,
    IVP_PE_AT_HULL,
    IVP_PE_AT_SHORT_MINDISTS,
    IVP_PE_AT_CRITICAL_MINDISTS,
    IVP_PE_AT_END,
    IVP_PE_USR1,
    IVP_PE_USR2,
    IVP_PE_MAX,
};

class IVP_PerformanceCounter {
public:
    BML_IVP_RETAIL_ALLOCATED_OBJECT;

    IVP_PerformanceCounter() = default;
    virtual void start_pcount() = 0;
    virtual void pcount(IVP_PERFORMANCE_ELEMENT) = 0;
    virtual void stop_pcount() = 0;
    virtual void environment_is_going_to_be_deleted(IVP_Environment *) = 0;
    virtual void reset_and_print_performance_counters(IVP_Time) = 0;
    virtual ~IVP_PerformanceCounter() {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::PerformanceCounterDestruct,
            this, [] {});
    }
};

class IVP_PerformanceCounter_Simple : public IVP_PerformanceCounter {
public:
    IVP_PerformanceCounter_Simple() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::PerformanceCounterSimpleConstruct, this);
    }
    ~IVP_PerformanceCounter_Simple() override = default;

    void start_pcount() override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::PerformanceCounterSimpleStart, this);
    }
    void pcount(IVP_PERFORMANCE_ELEMENT element) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::PerformanceCounterSimpleCount,
            this, element);
    }
    void stop_pcount() override {}
    void environment_is_going_to_be_deleted(IVP_Environment *environment)
        override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::PerformanceCounterSimpleEnvironmentDelete,
            this, environment);
    }
    void reset_and_print_performance_counters(IVP_Time time) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::PerformanceCounterSimpleReset,
            this, time);
    }

    union alignas(8) {
        int ref_counter[2];
        std::int64_t ref_counter64;
    };
    IVP_PERFORMANCE_ELEMENT counting;
    int count_PSIs;
    int counter[IVP_PE_MAX][2];
    IVP_Time time_of_last_reset;
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_BetterStatisticsmanager) == 0x30);
static_assert(BML_IvpBetterStatisticsmanagerLayoutCheck::enabled == 0x00);
static_assert(BML_IvpBetterStatisticsmanagerLayoutCheck::callbacks == 0x04);
static_assert(BML_IvpBetterStatisticsmanagerLayoutCheck::entities == 0x0C);
static_assert(BML_IvpBetterStatisticsmanagerLayoutCheck::simulation_time ==
              0x18);
static_assert(offsetof(IVP_BetterStatisticsmanager, update_delayed) == 0x20);
static_assert(offsetof(IVP_BetterStatisticsmanager, update_interval) == 0x28);
static_assert(sizeof(IVP_BetterStatisticsmanager_Data_Int_Array) == 0x28);
static_assert(sizeof(IVP_BetterStatisticsmanager_Data_Double_Array) == 0x30);
static_assert(sizeof(IVP_BetterStatisticsmanager_Data_Entity) == 0x48);
static_assert(offsetof(IVP_BetterStatisticsmanager_Data_Entity, type) == 0x04);
static_assert(offsetof(IVP_BetterStatisticsmanager_Data_Entity, data) == 0x08);
static_assert(offsetof(IVP_BetterStatisticsmanager_Data_Entity, text) == 0x38);
static_assert(
    offsetof(IVP_BetterStatisticsmanager_Data_Entity, text_color) == 0x3C);
static_assert(offsetof(IVP_BetterStatisticsmanager_Data_Entity, xpos) == 0x40);
static_assert(offsetof(IVP_BetterStatisticsmanager_Data_Entity, ypos) == 0x44);
static_assert(sizeof(IVP_BetterStatisticsmanager_Callback_Interface) == 0x04);
static_assert(sizeof(IVP_Statisticsmanager_Console_Callback) == 0x04);
static_assert(sizeof(IVP_PerformanceCounter) == 0x04);
static_assert(sizeof(IVP_PerformanceCounter_Simple) == 0xA8);
static_assert(offsetof(IVP_PerformanceCounter_Simple, counting) == 0x10);
static_assert(offsetof(IVP_PerformanceCounter_Simple, counter) == 0x18);
static_assert(offsetof(IVP_PerformanceCounter_Simple, time_of_last_reset) ==
              0xA0);
#endif

#endif // BML_IVP_PERFORMANCE_H
