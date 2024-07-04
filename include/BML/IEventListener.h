/**
 * @file IEventListener.h
 * @brief The interface of event listener.
 */
#ifndef BML_IEVENTLISTENER_H
#define BML_IEVENTLISTENER_H

#include <cstddef>
#include <cstdint>

namespace BML {
    inline namespace v1 {
        typedef size_t EventType;

        struct Event {
            EventType type;
            uint32_t value;
            uintptr_t param1;
            uintptr_t param2;
        };

        /**
         * @brief Interface for event listeners.
         *
         * This class represents an interface for event listeners. EventManager calls OnEvent if an event has occurred.
         */
        class IEventListener {
        public:
            virtual const char *GetName() const = 0;
            virtual int GetPriority() const { return 0; }

            virtual void OnRegister(EventType eventType, const char *eventName) = 0;
            virtual void OnUnregister(EventType eventType, const char *eventName) = 0;
            virtual void OnEventRenamed(EventType eventType, const char *newName, const char *oldName) = 0;

            /**
             * @brief Called by EventManager when an event occurs.
             *
             * @param event Pointer to the event that occurred.
             * @return An integer representing the result of the event handling.
             */
            virtual void OnEvent(const Event *event) = 0;

            /**
              * @brief Retrieves the user data associated with the specified type.
              * @param type The type of the user data to retrieve.
              * @return A pointer to the user data, or nullptr if not found.
              */
            virtual void *GetUserData(size_t type = 0) const { return nullptr; };

            /**
             * @brief Sets the user data associated with the specified type.
             * @param data A pointer to the user data to set.
             * @param type The type of the user data to set.
             * @return A pointer to the previous user data associated with the type, or nullptr if not found.
             */
            virtual void *SetUserData(void *data, size_t type = 0) { return nullptr; };
        };
    }
}

#endif // BML_IEVENTLISTENER_H
