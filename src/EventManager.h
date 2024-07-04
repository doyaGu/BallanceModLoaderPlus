#ifndef BML_EVENTMANAGER_H
#define BML_EVENTMANAGER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>

#include "BML/IEventManager.h"

namespace BML {
    class EventManager final : public IEventManager {
    public:
        EventManager();

        EventManager(const EventManager &rhs) = delete;
        EventManager(EventManager &&rhs) noexcept = delete;

        ~EventManager();

        EventManager &operator=(const EventManager &rhs) = delete;
        EventManager &operator=(EventManager &&rhs) noexcept = delete;

        void Reset();

        EventType AddEventType(const char *name) override;

        EventType GetEventType(const char *name) const override;
        const char *GetEventName(EventType type) const override;
        size_t GetEventCount() const override;

        bool RenameEvent(EventType type, const char *name) override;
        bool RenameEvent(const char *oldName, const char *newName) override;

        bool SendEvent(Event *event, IEventListener *listener) override;
        bool SendEvent(EventType type, uint32_t value, uintptr_t param1, uintptr_t param2, IEventListener *listener) override;
        bool SendEvent(const char *name, uint32_t value, uintptr_t param1, uintptr_t param2, IEventListener *listener) override;

        bool AddListener(EventType eventType, IEventListener *listener) override;
        bool AddListener(const char *eventName, IEventListener *listener) override;

        bool RemoveListener(EventType eventType, IEventListener *listener) override;
        bool RemoveListener(const char *eventName, IEventListener *listener) override;

        bool RemoveListeners(EventType eventType) override;
        bool RemoveListeners(const char *eventName) override;

        IEventListener *GetListener(EventType eventType, const char *name) override;
        IEventListener *GetListener(const char *eventName, const char *name) override;

        IEventListener *GetListener(EventType eventType, std::size_t index) override;
        IEventListener *GetListener(const char *eventName, std::size_t index) override;

        std::size_t GetListenerCount(EventType eventType) override;
        std::size_t GetListenerCount(const char *eventName) override;

        void SortListeners(EventType eventType);

    private:
        std::mutex m_Mutex;
        std::vector<bool> m_EventStatus;
        std::vector<std::string> m_EventTypes;
        std::unordered_map<std::string, EventType> m_EventTypeMap;
        std::unordered_map<EventType, std::vector<IEventListener *>> m_EventListeners;
    };
}

#endif // BML_EVENTMANAGER_H
