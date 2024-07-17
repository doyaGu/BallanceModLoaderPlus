#include <algorithm>
#include <utility>
#include "EventPublisher.h"

using namespace BML;

std::mutex EventPublisher::s_MapMutex;
std::unordered_map<std::string, EventPublisher *> EventPublisher::s_EventPublishers;

EventPublisher *EventPublisher::GetInstance(const std::string &name) {
    auto it = s_EventPublishers.find(name);
    if (it == s_EventPublishers.end()) {
        return Create(name);
    }
    return it->second;
}

EventPublisher *EventPublisher::Create(std::string name) {
    return new EventPublisher(std::move(name));
}

EventPublisher::~EventPublisher() {
    std::lock_guard<std::mutex> lock{s_MapMutex};
    s_EventPublishers.erase(m_Name);
}

int EventPublisher::AddRef() const {
    return m_RefCount.AddRef();
}

int EventPublisher::Release() const {
    int r = m_RefCount.Release();
    if (r == 0) {
        std::atomic_thread_fence(std::memory_order_acquire);
        delete const_cast<EventPublisher *>(this);
    }
    return r;
}

const char *EventPublisher::GetName() const {
    return m_Name.c_str();
}

EventType EventPublisher::AddEventType(const char *name) {
    if (!name)
        return -1;

    std::lock_guard<std::mutex> lock{m_Mutex};

    std::string eventName = name;
    auto it = m_EventTypeMap.find(eventName);
    if (it != m_EventTypeMap.end())
        return it->second;

    EventType type = m_EventTypes.size();
    m_EventTypeMap[eventName] = type;
    m_EventTypes.emplace_back(std::move(eventName));
    m_EventStatus.push_back(false);
    return type;
}

EventType EventPublisher::GetEventType(const char *name) const {
    if (!name)
        return -1;
    auto it = m_EventTypeMap.find(name);
    if (it == m_EventTypeMap.end())
        return -1;
    return it->second;
}

const char *EventPublisher::GetEventName(EventType type) const {
    if (type >= m_EventTypes.size())
        return nullptr;
    return m_EventTypes[type].c_str();
}

size_t EventPublisher::GetEventCount() const {
    return m_EventTypes.size();
}

bool EventPublisher::RenameEvent(EventType type, const char *name) {
    if (type >= m_EventTypes.size() || !name)
        return false;

    std::lock_guard<std::mutex> lock{m_Mutex};

    std::string newName = name;
    auto it = m_EventTypeMap.find(newName);
    if (it == m_EventTypeMap.end())
        return false;

    auto oldName = m_EventTypes[type];
    m_EventTypeMap.erase(oldName);
    m_EventTypeMap[newName] = type;
    m_EventTypes[type] = std::move(newName);

    auto &listeners = m_EventListeners[type];
    for (auto &info : listeners) {
        info.Ptr->OnEventRenamed(type, newName.c_str(), oldName.c_str());
    }

    return true;
}

bool EventPublisher::RenameEvent(const char *oldName, const char *newName) {
    if (!oldName || !newName)
        return false;

    std::lock_guard<std::mutex> lock{m_Mutex};

    std::string name = newName;
    auto it = m_EventTypeMap.find(name);
    if (it == m_EventTypeMap.end())
        return false;

    auto type = GetEventType(oldName);
    if (type == -1)
        return false;

    m_EventTypeMap.erase(oldName);
    m_EventTypeMap[name] = type;
    m_EventTypes[type] = std::move(name);
    return true;
}

bool EventPublisher::SendEvent(Event *event, IEventListener *listener) {
    if (!event)
        return false;

    if (event->type >= m_EventTypes.size())
        return false;

    if (m_EventStatus[event->type])
        return false;

    m_EventStatus[event->type] = true;
    if (listener) {
        listener->OnEvent(event);
    } else {
        auto &listeners = m_EventListeners[event->type];
        for (auto &info : listeners) {
            info.Ptr->OnEvent(event);
        }
    }
    m_EventStatus[event->type] = false;

    return true;
}

bool EventPublisher::SendEvent(EventType type, uint32_t value, uintptr_t param1, uintptr_t param2, IEventListener *listener) {
    if (type >= m_EventTypes.size())
        return false;

    if (m_EventStatus[type])
        return false;

    m_EventStatus[type] = true;
    Event event = {type, value, param1, param2};
    if (listener) {
        listener->OnEvent(&event);
    } else {
        auto &listeners = m_EventListeners[type];
        for (auto &info : listeners) {
            info.Ptr->OnEvent(&event);
        }
    }
    m_EventStatus[type] = false;

    return true;
}

bool EventPublisher::SendEvent(const char *name, uint32_t value, uintptr_t param1, uintptr_t param2, IEventListener *listener) {
    return SendEvent(GetEventType(name), value, param1, param2, listener);
}

bool EventPublisher::AddListener(EventType eventType, IEventListener *listener, const char *name, int priority) {
    if (eventType >= m_EventTypes.size() || !listener || !name || name[0] == '\0')
        return false;

    if (m_EventStatus[eventType])
        return false;

    std::lock_guard<std::mutex> lock{m_Mutex};

    auto it = m_EventListeners.find(eventType);
    if (it == m_EventListeners.end())
        return false;

    auto &listeners = it->second;
    auto lit = std::find_if(listeners.begin(), listeners.end(), [name](const EventListenerInfo &e) {
        return strcmp(name, e.Name.c_str()) == 0;
    });
    if (lit != listeners.end())
        return false;
    listeners.emplace_back(listener, name, priority);

    SortListeners(eventType);

    listener->OnRegister(eventType, GetEventName(eventType));
    return true;
}

bool EventPublisher::AddListener(const char *eventName, IEventListener *listener, const char *name, int priority) {
    if (!eventName || !listener)
        return false;

    auto type = GetEventType(eventName);
    if (type == -1)
        return false;

    if (m_EventStatus[type])
        return false;

    std::lock_guard<std::mutex> lock{m_Mutex};

    auto it = m_EventListeners.find(type);
    if (it == m_EventListeners.end())
        return false;

    auto &listeners = it->second;
    auto lit = std::find_if(listeners.begin(), listeners.end(), [name](const EventListenerInfo &e) {
        return e.Name == name;
    });
    if (lit != listeners.end())
        return false;
    listeners.emplace_back(listener, name, priority);

    SortListeners(type);

    listener->OnRegister(type, eventName);
    return true;
}

bool EventPublisher::RemoveListener(EventType eventType, IEventListener *listener) {
    if (eventType >= m_EventTypes.size() || !listener)
        return false;

    if (m_EventStatus[eventType])
        return false;

    std::lock_guard<std::mutex> lock{m_Mutex};

    auto it = m_EventListeners.find(eventType);
    if (it == m_EventListeners.end())
        return false;

    auto &listeners = it->second;
    const auto i = std::remove_if(listeners.begin(), listeners.end(), [listener](const EventListenerInfo &e) {
        return listener == e.Ptr;
    });
    if (i == listeners.end())
        return false;
    listeners.erase(i, listeners.end());

    listener->OnUnregister(eventType, GetEventName(eventType));
    return true;
}

bool EventPublisher::RemoveListener(const char *eventName, IEventListener *listener) {
    if (!eventName || !listener)
        return false;

    auto type = GetEventType(eventName);
    if (type == -1)
        return false;

    if (m_EventStatus[type])
        return false;

    std::lock_guard<std::mutex> lock{m_Mutex};

    auto it = m_EventListeners.find(type);
    if (it == m_EventListeners.end())
        return false;

    auto &listeners = it->second;
    const auto i = std::remove_if(listeners.begin(), listeners.end(), [listener](const EventListenerInfo &e) {
        return listener == e.Ptr;
    });
    if (i == listeners.end())
        return false;
    listeners.erase(i, listeners.end());

    listener->OnUnregister(type, eventName);
    return true;
}

bool EventPublisher::RemoveListeners(EventType eventType) {
    if (eventType >= m_EventTypes.size())
        return false;

    if (m_EventStatus[eventType])
        return false;

    std::lock_guard<std::mutex> lock{m_Mutex};

    auto it = m_EventListeners.find(eventType);
    if (it == m_EventListeners.end())
        return false;

    auto &listeners = it->second;
    for (auto &info : listeners) {
        info.Ptr->OnUnregister(eventType, GetEventName(eventType));
    }

    m_EventListeners[eventType].clear();
    return true;
}

bool EventPublisher::RemoveListeners(const char *eventName) {
    if (!eventName)
        return false;

    auto type = GetEventType(eventName);
    if (type == -1)
        return false;

    if (m_EventStatus[type])
        return false;

    std::lock_guard<std::mutex> lock{m_Mutex};

    auto it = m_EventListeners.find(type);
    if (it == m_EventListeners.end())
        return false;

    auto &listeners = it->second;
    for (auto &info : listeners) {
        info.Ptr->OnUnregister(type, eventName);
    }

    m_EventListeners[type].clear();
    return true;
}

IEventListener *EventPublisher::GetListener(EventType eventType, const char *name) {
    if (eventType >= m_EventTypes.size())
        return nullptr;

    if (!name || name[0] == '\0')
        return nullptr;

    auto it = m_EventListeners.find(eventType);
    if (it == m_EventListeners.end())
        return nullptr;

    auto &listeners = it->second;
    auto lit = std::find_if(listeners.begin(), listeners.end(),
                            [name](const EventListenerInfo &e) {
                                return e.Name == name;
                            });
    if (lit == listeners.end())
        return nullptr;

    return lit->Ptr;
}

IEventListener *EventPublisher::GetListener(const char *eventName, const char *name) {
    auto type = GetEventType(eventName);
    if (type == -1)
        return nullptr;

    return GetListener(type, name);
}

IEventListener *EventPublisher::GetListener(EventType eventType, std::size_t index) {
    if (eventType >= m_EventTypes.size())
        return nullptr;

    auto it = m_EventListeners.find(eventType);
    if (it == m_EventListeners.end())
        return nullptr;

    auto &listeners = it->second;
    if (index >= listeners.size())
        return nullptr;

    return listeners[index].Ptr;
}

IEventListener *EventPublisher::GetListener(const char *eventName, std::size_t index) {
    auto type = GetEventType(eventName);
    if (type == -1)
        return nullptr;

    return GetListener(type, index);
}

std::size_t EventPublisher::GetListenerCount(EventType eventType) {
    if (eventType >= m_EventTypes.size())
        return 0;

    auto it = m_EventListeners.find(eventType);
    if (it == m_EventListeners.end())
        return 0;

    auto &listeners = it->second;
    return listeners.size();
}

std::size_t EventPublisher::GetListenerCount(const char *eventName) {
    auto type = GetEventType(eventName);
    if (type == -1)
        return 0;

    return GetListenerCount(type);
}

void *EventPublisher::GetUserData(size_t type) const {
    return m_UserData.GetData(type);
}

void *EventPublisher::SetUserData(void *data, size_t type) {
    return m_UserData.SetData(data, type);
}

void EventPublisher::SortListeners(EventType eventType) {
    auto it = m_EventListeners.find(eventType);
    if (it != m_EventListeners.end()) {
        auto &listeners = it->second;
        std::sort(listeners.begin(), listeners.end());
    }
}

EventPublisher::EventPublisher(std::string name) : m_Name(std::move(name)) {
    std::lock_guard<std::mutex> lock{s_MapMutex};
    s_EventPublishers[m_Name] = this;
}