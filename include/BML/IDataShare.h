/**
 * @file IDataShare.h
 * @brief The interface for sharing data between mods.
 */
#ifndef BML_IDATASHARE_H
#define BML_IDATASHARE_H

namespace BML {
    inline namespace v1 {
        /**
         * @brief The callback function for data sharing.
         * @param key The key associated with the data.
         * @param data A pointer to the data.
         * @param userdata A pointer to user-defined data.
         */
        typedef void(*DataShareCallback)(const char *key, void *data, void *userdata);

        /**
         * @brief The interface for data sharing.
         */
        class IDataShare {
        public:
            /**
             * @brief Requests data associated with the specified key.
             * @param key The key associated with the requested data.
             * @param callback The callback function to be called when the data is available.
             * @param userdata A pointer to user-defined data to be passed to the callback function.
             */
            virtual void Request(const char *key, DataShareCallback callback, void *userdata) const = 0;

            /**
             * @brief Retrieves the data associated with the specified key.
             * @param key The key associated with the data to retrieve.
             * @return A pointer to the data, or nullptr if not found.
             */
            virtual void *Get(const char *key) const = 0;

            /**
             * @brief Sets the data associated with the specified key.
             * @param key The key associated with the data to set.
             * @param data A pointer to the data to set.
             * @return A pointer to the previous data associated with the key, or nullptr if not found.
             */
            virtual void *Set(const char *key, void *data) = 0;

            /**
             * @brief Inserts the data associated with the specified key.
             * @param key The key associated with the data to insert.
             * @param data A pointer to the data to insert.
             * @return A pointer to the previous data associated with the key, or nullptr if not found.
             */
            virtual void *Insert(const char *key, void *data) = 0;

            /**
             * @brief Removes the data associated with the specified key.
             * @param key The key associated with the data to remove.
             * @return A pointer to the removed data, or nullptr if not found.
             */
            virtual void *Remove(const char *key) = 0;

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
        };
    }
}

#endif // BML_IDATASHARE_H
