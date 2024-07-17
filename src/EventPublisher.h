#ifndef BML_EVENTPUBLISHER_H
#define BML_EVENTPUBLISHER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>

#include "BML/IEventPublisher.h"
#include "BML/DataBox.h"
#include "BML/RefCount.h"

namespace BML {
    struct EventListenerInfo {
        IEventListener *Ptr = nullptr;
        std::string Name;
        int Priority = 0;

        EventListenerInfo(IEventListener *listener, std::string name, int priority) : Ptr(listener), Name(std::move(name)), Priority(priority) {}

        bool operator==(const EventListenerInfo &rhs) const {
            return Ptr == rhs.Ptr && Name == rhs.Name;
        }

        bool operator!=(const EventListenerInfo &rhs) const {
            return !(rhs == *this);
        }

        bool operator<(const EventListenerInfo &rhs) const {
            return Priority < rhs.Priority;
        }

        bool operator>(const EventListenerInfo &rhs) const {
            return rhs < *this;
        }

        bool operator<=(const EventListenerInfo &rhs) const {
            return !(rhs < *this);
        }

        bool operator>=(const EventListenerInfo &rhs) const {
            return !(*this < rhs);
        }
    };

    class EventPublisher final : public IEventPublisher {
    public:
        static EventPublisher *GetInstance(const std::string &name);
        static EventPublisher *Create(std::string name);

        EventPublisher(const EventPublisher &rhs) = delete;
        EventPublisher(EventPublisher &&rhs) noexcept = delete;

        ~EventPublisher() override;

        EventPublisher &operator=(const EventPublisher &rhs) = delete;
        EventPublisher &operator=(EventPublisher &&rhs) noexcept = delete;

        int AddRef() const override;
        int Release() const override;

        const char *GetName() const override;

        EventType AddEventType(const char *name) override;

        EventType GetEventType(const char *name) const override;
        const char *GetEventName(EventType type) const override;
        size_t GetEventCount() const override;

        bool RenameEvent(EventType type, const char *name) override;
        bool RenameEvent(const char *oldName, const char *newName) override;

        bool SendEvent(Event *event, IEventListener *listener = nullptr) override;
        bool SendEvent(EventType type, uint32_t value = 0, uintptr_t param1 = 0, uintptr_t param2 = 0, IEventListener *listener = nullptr) override;
        bool SendEvent(const char *name, uint32_t value = 0, uintptr_t param1 = 0, uintptr_t param2 = 0, IEventListener *listener = nullptr) override;

        bool AddListener(EventType eventType, IEventListener *listener, const char *name, int priority) override;
        bool AddListener(const char *eventName, IEventListener *listener, const char *name, int priority) override;

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

        void *GetUserData(size_t type) const override;
        void *SetUserData(void *data, size_t type) override;

        void SortListeners(EventType eventType);

    private:
        explicit EventPublisher(std::string name);

        std::string m_Name;
        mutable RefCount m_RefCount;
        std::mutex m_Mutex;
        std::vector<bool> m_EventStatus;
        std::vector<std::string> m_EventTypes;
        std::unordered_map<std::string, EventType> m_EventTypeMap;
        std::unordered_map<EventType, std::vector<EventListenerInfo>> m_EventListeners;
        DataBox m_UserData;

        static std::mutex s_MapMutex;
        static std::unordered_map<std::string, EventPublisher *> s_EventPublishers;
    };
}

#endif // BML_EVENTPUBLISHER_H
