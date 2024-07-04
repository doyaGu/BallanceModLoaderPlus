#include <algorithm>
#include "EventManager.h"

using namespace BML;

EventManager::EventManager() = default;

EventManager::~EventManager() = default;

void EventManager::Reset() {
    m_EventListeners.clear();
    m_EventTypeMap.clear();
    m_EventTypes.clear();
    m_EventStatus.clear();
}

EventType EventManager::AddEventType(const char *name) {
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

EventType EventManager::GetEventType(const char *name) const {
    if (!name)
        return -1;
    auto it = m_EventTypeMap.find(name);
    if (it == m_EventTypeMap.end())
        return -1;
    return it->second;
}

const char *EventManager::GetEventName(EventType type) const {
    if (type >= m_EventTypes.size())
        return nullptr;
    return m_EventTypes[type].c_str();
}

size_t EventManager::GetEventCount() const {
    return m_EventTypes.size();
}

bool EventManager::RenameEvent(EventType type, const char *name) {
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
    for (auto *l : listeners) {
        l->OnEventRenamed(type, newName.c_str(), oldName.c_str());
    }

    return true;
}

bool EventManager::RenameEvent(const char *oldName, const char *newName) {
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

bool EventManager::SendEvent(Event *event, IEventListener *listener) {
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
        for (auto *l : listeners) {
            l->OnEvent(event);
        }
    }
    m_EventStatus[event->type] = false;

    return true;
}

bool EventManager::SendEvent(EventType type, uint32_t value, uintptr_t param1, uintptr_t param2, IEventListener *listener) {
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
        for (auto *l : listeners) {
            l->OnEvent(&event);
        }
    }
    m_EventStatus[type] = false;

    return true;
}

bool EventManager::SendEvent(const char *name, uint32_t value, uintptr_t param1, uintptr_t param2, IEventListener *listener) {
    return SendEvent(GetEventType(name), value, param1, param2, listener);
}

bool EventManager::AddListener(EventType eventType, IEventListener *listener) {
    if (eventType >= m_EventTypes.size() || !listener)
        return false;

    if (m_EventStatus[eventType])
        return false;

    std::lock_guard<std::mutex> lock{m_Mutex};

    auto it = m_EventListeners.find(eventType);
    if (it == m_EventListeners.end())
        return false;

    auto &listeners = it->second;
    listeners.push_back(listener);

    SortListeners(eventType);

    listener->OnRegister(eventType, GetEventName(eventType));
    return true;
}

bool EventManager::AddListener(const char *eventName, IEventListener *listener) {
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
    listeners.push_back(listener);

    SortListeners(type);

    listener->OnRegister(type, eventName);
    return true;
}

bool EventManager::RemoveListener(EventType eventType, IEventListener *listener) {
    if (eventType >= m_EventTypes.size() || !listener)
        return false;

    if (m_EventStatus[eventType])
        return false;

    std::lock_guard<std::mutex> lock{m_Mutex};

    auto it = m_EventListeners.find(eventType);
    if (it == m_EventListeners.end())
        return false;

    auto &listeners = it->second;
    const auto i = std::remove(listeners.begin(), listeners.end(), listener);
    if (i == listeners.end())
        return false;
    listeners.erase(i, listeners.end());

    listener->OnUnregister(eventType, GetEventName(eventType));
    return true;
}

bool EventManager::RemoveListener(const char *eventName, IEventListener *listener) {
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
    const auto i = std::remove(listeners.begin(), listeners.end(), listener);
    if (i == listeners.end())
        return false;
    listeners.erase(i, listeners.end());

    listener->OnUnregister(type, eventName);
    return true;
}

bool EventManager::RemoveListeners(EventType eventType) {
    if (eventType >= m_EventTypes.size())
        return false;

    if (m_EventStatus[eventType])
        return false;

    std::lock_guard<std::mutex> lock{m_Mutex};

    auto it = m_EventListeners.find(eventType);
    if (it == m_EventListeners.end())
        return false;

    auto &listeners = it->second;
    for (auto *l : listeners) {
        l->OnUnregister(eventType, GetEventName(eventType));
    }

    m_EventListeners[eventType].clear();
    return true;
}

bool EventManager::RemoveListeners(const char *eventName) {
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
    for (auto *l : listeners) {
        l->OnUnregister(type, eventName);
    }

    m_EventListeners[type].clear();
    return true;
}

IEventListener *EventManager::GetListener(EventType eventType, const char *name) {
    if (eventType >= m_EventTypes.size())
        return nullptr;

    if (!name || name[0] == '\0')
        return nullptr;

    auto it = m_EventListeners.find(eventType);
    if (it == m_EventListeners.end())
        return nullptr;

    auto &listeners = it->second;
    auto lit = std::find_if(listeners.begin(), listeners.end(),
                            [name](const IEventListener *l) {
                                return strcmp(l->GetName(), name) == 0;
                            });
    if (lit == listeners.end())
        return nullptr;

    return *lit;
}

IEventListener *EventManager::GetListener(const char *eventName, const char *name) {
    auto type = GetEventType(eventName);
    if (type == -1)
        return nullptr;

    return GetListener(type, name);
}

IEventListener *EventManager::GetListener(EventType eventType, std::size_t index) {
    if (eventType >= m_EventTypes.size())
        return nullptr;

    auto it = m_EventListeners.find(eventType);
    if (it == m_EventListeners.end())
        return nullptr;

    auto &listeners = it->second;
    if (index >= listeners.size())
        return nullptr;

    return listeners[index];
}

IEventListener *EventManager::GetListener(const char *eventName, std::size_t index) {
    auto type = GetEventType(eventName);
    if (type == -1)
        return nullptr;

    return GetListener(type, index);
}

std::size_t EventManager::GetListenerCount(EventType eventType) {
    if (eventType >= m_EventTypes.size())
        return 0;

    auto it = m_EventListeners.find(eventType);
    if (it == m_EventListeners.end())
        return 0;

    auto &listeners = it->second;
    return listeners.size();
}

std::size_t EventManager::GetListenerCount(const char *eventName) {
    auto type = GetEventType(eventName);
    if (type == -1)
        return 0;

    return GetListenerCount(type);
}

void EventManager::SortListeners(EventType eventType) {
    auto it = m_EventListeners.find(eventType);
    if (it != m_EventListeners.end()) {
        auto &listeners = it->second;
        std::sort(listeners.begin(), listeners.end(),
                  [](const IEventListener *lhs, const IEventListener *rhs) {
                      return lhs->GetPriority() > rhs->GetPriority();
                  });
    }
}