/**
 * @file IEventPublisher.h
 * @brief The interface of event publisher.
 */
#ifndef BML_IEVENTMANAGER_H
#define BML_IEVENTMANAGER_H

#include <cstddef>
#include <cstdint>

#include "BML/IEventListener.h"

namespace BML {
    inline namespace v1 {
        /**
         * @brief Interface for event publisher.
         *
         * This class represents an interface for managing events and event types.
         */
        class IEventPublisher {
        public:
            /**
             * @brief Increase the reference count of the event publisher object.
             * @return The new reference count.
             */
            virtual int AddRef() const = 0;

            /**
             * @brief Decrease the reference count of the event publisher object.
             * @return The new reference count.
             */
            virtual int Release() const = 0;

            /**
             * @brief Gets the name of the event publisher object.
             * @return The name as a null-terminated string.
             */
            virtual const char *GetName() const = 0;

            /**
             * @brief Add a new event type with the given name.
             *
             * @param name The name of the event type.
             * @return The event type identifier associated with the added event type.
             */
            virtual EventType AddEventType(const char *name) = 0;
            /**
             * @brief Get the event type identifier associated with the given event type name.
             *
             * @param name The name of the event type.
             * @return The event type identifier.
             */
            virtual EventType GetEventType(const char *name) const = 0;

            /**
             * @brief Get the name of the event type associated with the given event type identifier.
             *
             * @param type The event type identifier.
             * @return The name of the event type as a null-terminated string.
             */
            virtual const char *GetEventName(EventType type) const = 0;

            /**
             * @brief Get the total number of registered event types.
             *
             * @return The count of registered event types.
             */
            virtual size_t GetEventCount() const = 0;

            /**
             * @brief Rename an existing event type with the given name.
             *
             * @param type The event type identifier to rename.
             * @param name The new name for the event type.
             * @return True if the renaming was successful, false otherwise.
             */
            virtual bool RenameEvent(EventType type, const char *name) = 0;

            /**
             * @brief Rename an existing event type with the given new name.
             *
             * @param oldName The current name of the event type.
             * @param newName The new name for the event type.
             * @return True if the renaming was successful, false otherwise.
             */
            virtual bool RenameEvent(const char *oldName, const char *newName) = 0;

            /**
             * @brief Send the given event to all appropriate listeners.
             *
             * @param event Pointer to the event to be sent.
             * @return True if the event was sent successfully, false otherwise.
             */
            virtual bool SendEvent(Event *event, IEventListener *listener = nullptr) = 0;

            /**
             * @brief Send an event of the specified event type to all appropriate listeners.
             *
             * @param type The event type identifier.
             * @return True if the event was sent successfully, false otherwise.
             */
            virtual bool SendEvent(EventType type, uint32_t value = 0, uintptr_t param1 = 0, uintptr_t param2 = 0, IEventListener *listener = nullptr) = 0;

            /**
             * @brief Send an event with the specified event type name to all appropriate listeners.
             *
             * @param name The name of the event type.
             * @return True if the event was sent successfully, false otherwise.
             */
            virtual bool SendEvent(const char *name, uint32_t value = 0, uintptr_t param1 = 0, uintptr_t param2 = 0, IEventListener *listener = nullptr) = 0;

            /**
             * @brief Add a listener for the specified event type.
             *
             * @param eventType The event type identifier to listen for.
             * @param listener Pointer to the listener object to be added.
             * @return True if the listener was added successfully, false otherwise.
             */
            virtual bool AddListener(EventType eventType, IEventListener *listener, const char *name, int priority = 0) = 0;

            /**
             * @brief Add a listener for the specified event name.
             *
             * @param eventName The name of the event to listen for.
             * @param listener Pointer to the listener object to be added.
             * @return True if the listener was added successfully, false otherwise.
             */
            virtual bool AddListener(const char *eventName, IEventListener *listener, const char *name, int priority = 0) = 0;

            /**
             * @brief Remove a listener for the specified event type.
             *
             * @param eventType The event type identifier to remove the listener from.
             * @param listener Pointer to the listener object to be removed.
             * @return True if the listener was removed successfully, false otherwise.
             */
            virtual bool RemoveListener(EventType eventType, IEventListener *listener) = 0;

            /**
             * @brief Remove a listener for the specified event name.
             *
             * @param eventName The name of the event to remove the listener from.
             * @param listener Pointer to the listener object to be removed.
             * @return True if the listener was removed successfully, false otherwise.
             */
            virtual bool RemoveListener(const char *eventName, IEventListener *listener) = 0;

            virtual bool RemoveListeners(EventType eventType) = 0;
            virtual bool RemoveListeners(const char *eventName) = 0;

            virtual IEventListener *GetListener(EventType eventType, const char *name) = 0;
            virtual IEventListener *GetListener(const char *eventName, const char *name) = 0;

            virtual IEventListener *GetListener(EventType eventType, std::size_t index) = 0;
            virtual IEventListener *GetListener(const char *eventName, std::size_t index) = 0;

            virtual std::size_t GetListenerCount(EventType eventType) = 0;
            virtual std::size_t GetListenerCount(const char *eventName) = 0;

            /**
             * @brief Retrieves the user data associated with the specified type.
             * @param type The type of the user data to retrieve.
             * @return A pointer to the user data, or nullptr if not found.
             */
            virtual void *GetUserData(size_t type = 0) const = 0;

            /**
             * @brief Sets the user data associated with the specified type.
             * @param data A pointer to the user data to set.
             * @param type The type of the user data to set.
             * @return A pointer to the previous user data associated with the type, or nullptr if not found.
             */
            virtual void *SetUserData(void *data, size_t type = 0) = 0;

        protected:
            virtual ~IEventPublisher() = default;
        };
    }
}

#endif // BML_IEVENTMANAGER_H
