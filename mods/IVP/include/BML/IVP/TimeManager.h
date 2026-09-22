#ifndef BML_IVP_TIME_MANAGER_H
#define BML_IVP_TIME_MANAGER_H

#include "BML/IVP/MinList.h"

#include <cstddef>

class IVP_Environment;
class IVP_Time_Manager;

// The protected time-event surface is needed because the public environment
// returns its time manager. These layouts and the two executable entry points
// below were checked against the Ballance retail image.
class IVP_Time_Event {
public:
    IVP_Time_Event() = default;
    virtual void simulate_time_event(IVP_Environment *) = 0;

    int index;
};

class IVP_Time_Event_PSI : public IVP_Time_Event {
public:
    void simulate_time_event(IVP_Environment *) override;
};

class IVP_Event_Manager {
public:
    virtual void simulate_time_events(
        IVP_Time_Manager *, IVP_Environment *, IVP_Time) = 0;

    virtual void simulate_variable_time_step(
        IVP_Time_Manager *timeManager, IVP_Environment *environment,
        IVP_Time_Event_PSI *psiEvent, IVP_FLOAT deltaTime) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::EventManagerSimulateVariable,
            this, timeManager, environment, psiEvent, deltaTime);
    }

    int mode;
};

class IVP_Event_Manager_Standard : public IVP_Event_Manager {
private:
    void simulate_time_events(
        IVP_Time_Manager *timeManager, IVP_Environment *environment,
        IVP_Time untilTime) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::EventManagerStandardSimulate,
            this, timeManager, environment, untilTime);
    }
};

// Alternate one-event stepping policy present in the public IVP revision but
// link-stripped from Ballance. It adds no state and overrides the same retail
// slot as IVP_Event_Manager_Standard, so the Ballance base layout supports a
// selective local reconstruction.
class IVP_Event_Manager_D : public IVP_Event_Manager {
private:
    void simulate_time_events(
        IVP_Time_Manager *timeManager, IVP_Environment *environment,
        IVP_Time untilTime) override;
};

class IVP_Time_Manager {
public:
    IVP_Time_Manager() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::TimeManagerConstruct, this);
    }
    ~IVP_Time_Manager() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::TimeManagerDestruct, this);
    }

    IVP_Time_Manager(const IVP_Time_Manager &) = delete;
    IVP_Time_Manager &operator=(const IVP_Time_Manager &) = delete;

    void insert_event(IVP_Time_Event *event, IVP_Time time) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::TimeManagerInsertEvent,
            this, event, time);
    }
    void remove_event(IVP_Time_Event *event) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::TimeManagerRemoveEvent,
            this, event);
    }
    void update_event(IVP_Time_Event *event, IVP_Time time) {
        const IVP_FLOAT relativeTime = static_cast<IVP_FLOAT>(time - base_time);
        min_hash->remove_minlist_elem(event->index);
        event->index = min_hash->add(event, relativeTime);
    }
    int get_event_count() const {
        return min_hash ? min_hash->counter : 0;
    }

    void event_loop(IVP_Environment *environment, IVP_Time untilTime) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::TimeManagerEventLoop,
            this, environment, untilTime);
    }

    void simulate_variable_time_step(
        IVP_Environment *environment, IVP_FLOAT deltaTime) {
        constexpr IVP_FLOAT MinDeltaPsi = 1.0f / 200.0f;
        constexpr IVP_FLOAT MaxDeltaPsi = 1.0f / 10.0f;
        if (deltaTime < MinDeltaPsi)
            deltaTime = MinDeltaPsi;
        if (deltaTime > MaxDeltaPsi)
            deltaTime = MaxDeltaPsi;

#if defined(_MSC_VER) && defined(_M_IX86)
        unsigned short oldControlWord = 0;
        unsigned short doublePrecisionControlWord = 0;
        __asm fstcw oldControlWord
        doublePrecisionControlWord =
            static_cast<unsigned short>(oldControlWord | 0x0300u);
        __asm fldcw doublePrecisionControlWord
#endif

        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::EventManagerSimulateVariable,
            event_manager, this, environment, psi_event, deltaTime);

#if defined(_MSC_VER) && defined(_M_IX86)
        __asm fldcw oldControlWord
#endif
    }

    void env_set_current_time(
        IVP_Environment *environment, IVP_Time time) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::TimeManagerSetEnvironmentTime,
            this, environment, time);
    }

    void reset_time(IVP_Time offset) {
        const IVP_FLOAT relativeOffset =
            static_cast<IVP_FLOAT>(offset - base_time);
        if (min_hash) {
            IVP_U_Min_List_Enumerator enumerator(min_hash);
            while (IVP_U_Min_List_Element *element =
                       enumerator.get_next_element_header()) {
                element->value -= relativeOffset;
            }
            min_hash->min_value -= relativeOffset;
        }
        base_time = IVP_Time(0.0);
        last_time = 0.0;
    }

private:
    int n_events;

public:
    IVP_Event_Manager *event_manager;
    IVP_U_Min_List *min_hash;
    IVP_Time_Event_PSI *psi_event;
    IVP_DOUBLE last_time;
    IVP_Time base_time;
};

inline void IVP_Event_Manager_D::simulate_time_events(
    IVP_Time_Manager *timeManager, IVP_Environment *environment,
    IVP_Time untilTime) {
    const IVP_FLOAT eventTime = timeManager->min_hash->find_min_value();
    if (untilTime - timeManager->base_time) {
        auto *event = static_cast<IVP_Time_Event *>(
            timeManager->min_hash->find_min_elem());
        timeManager->min_hash->remove_minlist_elem(event->index);
        event->index = IVP_U_MINLIST_UNUSED;
        timeManager->last_time = eventTime;
        timeManager->env_set_current_time(
            environment, timeManager->base_time + eventTime);
        event->simulate_time_event(environment);
    }
    timeManager->env_set_current_time(environment, untilTime);
}

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_Time_Event) == 0x08);
static_assert(offsetof(IVP_Time_Event, index) == 0x04);
static_assert(sizeof(IVP_Time_Event_PSI) == 0x08);
static_assert(sizeof(IVP_Event_Manager) == 0x08);
static_assert(sizeof(IVP_Event_Manager_Standard) == 0x08);
static_assert(sizeof(IVP_Event_Manager_D) == 0x08);
static_assert(offsetof(IVP_Event_Manager, mode) == 0x04);
static_assert(sizeof(IVP_Time_Manager) == 0x20);
static_assert(offsetof(IVP_Time_Manager, event_manager) == 0x04);
static_assert(offsetof(IVP_Time_Manager, min_hash) == 0x08);
static_assert(offsetof(IVP_Time_Manager, psi_event) == 0x0C);
static_assert(offsetof(IVP_Time_Manager, last_time) == 0x10);
static_assert(offsetof(IVP_Time_Manager, base_time) == 0x18);
#endif

#endif // BML_IVP_TIME_MANAGER_H
