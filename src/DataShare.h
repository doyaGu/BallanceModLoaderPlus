#ifndef BML_DATASHARE_H
#define BML_DATASHARE_H

#include <mutex>
#include <string>
#include <unordered_map>

#include "BML/IDataShare.h"
#include "BML/DataBox.h"

namespace BML {
    class DataShare final : public IDataShare {
    public:
        DataShare();

        DataShare(const DataShare &rhs) = delete;
        DataShare(DataShare &&rhs) noexcept = delete;

        ~DataShare();

        DataShare &operator=(const DataShare &rhs) = delete;
        DataShare &operator=(DataShare &&rhs) noexcept = delete;

        void Request(const char *key, DataShareCallback callback, void *userdata) const override;

        void *Get(const char *key) const override;
        void *Set(const char *key, void *data) override;
        void *Insert(const char *key, void *data) override;
        void *Remove(const char *key) override;

        void *GetUserData(size_t type) const override;
        void *SetUserData(void *data, size_t type) override;

    private:
        struct Callback {
            DataShareCallback callback;
            void *userdata;

            Callback(DataShareCallback cb, void *data) : callback(cb), userdata(data) {}

            bool operator==(const Callback &rhs) const {
                return callback == rhs.callback && userdata == rhs.userdata;
            }

            bool operator!=(const Callback &rhs) const {
                return !(rhs == *this);
            }
        };

        bool AddCallbacks(const char *key, DataShareCallback callback, void *userdata) const;
        void TriggerCallbacks(const char *key, void *data) const;

        static bool ValidateKey(const char *key);

        mutable std::mutex m_RWLock;
        std::unordered_map<std::string, void *> m_DataMap;
        mutable std::unordered_map<std::string, std::vector<Callback>> m_CallbackMap;
        DataBox m_UserData;
    };
}

#endif // BML_DATASHARE_H
