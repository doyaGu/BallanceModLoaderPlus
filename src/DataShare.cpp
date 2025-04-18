#include "DataShare.h"

#include <utility>

using namespace BML;

std::mutex DataShare::s_MapMutex;
std::unordered_map<std::string, DataShare *> DataShare::s_DataShares;

DataShare *DataShare::GetInstance(const std::string &name) {
    auto it = s_DataShares.find(name);
    if (it == s_DataShares.end()) {
        return new DataShare(name);
    }
    return it->second;
}

DataShare::DataShare(std::string name) : m_Name(std::move(name)) {
    std::lock_guard<std::mutex> lock{s_MapMutex};
    s_DataShares[m_Name] = this;
}

DataShare::~DataShare() {
    std::lock_guard<std::mutex> lock{s_MapMutex};
    s_DataShares.erase(m_Name);
}

int DataShare::AddRef() const {
    return m_RefCount.AddRef();
}

int DataShare::Release() const {
    int r = m_RefCount.Release();
    if (r == 0) {
        std::atomic_thread_fence(std::memory_order_acquire);
        delete const_cast<DataShare *>(this);
    }
    return r;
}

const char *DataShare::GetName() const {
    return m_Name.c_str();
}

void DataShare::Request(const char *key, DataShareCallback callback, void *userdata) const {
    if (!ValidateKey(key))
        return;

    std::lock_guard<std::mutex> guard(m_RWLock);

    auto it = m_DataMap.find(key);
    if (it == m_DataMap.end()) {
        AddCallbacks(key, callback, userdata);
    } else {
        auto &data = it->second;
        TriggerCallbacks(key, data.data(), data.size());
    }
}

const void *DataShare::Get(const char *key, size_t *size) const {
    if (!ValidateKey(key))
        return nullptr;

    std::lock_guard<std::mutex> guard(m_RWLock);

    auto it = m_DataMap.find(key);
    if (it == m_DataMap.end())
        return nullptr;

    auto &data = it->second;
    if (size)
        *size = data.size();
    return data.data();
}

bool DataShare::Copy(const char *key, void *buf, size_t size) const {
    if (!buf || size == 0)
        return false;

    if (!ValidateKey(key))
        return false;

    std::lock_guard<std::mutex> guard(m_RWLock);

    auto it = m_DataMap.find(key);
    if (it == m_DataMap.end())
        return false;

    auto &data = it->second;
    memcpy(buf, data.data(), std::min(data.size(), size));
    return true;
}

bool DataShare::Set(const char *key, const void *buf, size_t size) {
    if (!ValidateKey(key))
        return false;

    std::lock_guard<std::mutex> guard(m_RWLock);

    auto it = m_DataMap.find(key);
    if (it != m_DataMap.end()) {
        auto &data = it->second;
        data.resize(size);
        memcpy(data.data(), buf, size);
    } else {
        auto result = m_DataMap.emplace(key, std::vector<uint8_t>());
        if (!result.second)
            return false;
        it = result.first;
        auto &data = it->second;
        data.resize(size);
        memcpy(data.data(), buf, size);
    }

    auto &data = it->second;
    TriggerCallbacks(key, data.data(), data.size());
    return true;
}

bool DataShare::Put(const char *key, const void *buf, size_t size) {
    if (!ValidateKey(key))
        return false;

    std::lock_guard<std::mutex> guard(m_RWLock);

    auto it = m_DataMap.find(key);
    if (it != m_DataMap.end())
        return false;

    auto result = m_DataMap.emplace(key, std::vector<uint8_t>());
    if (!result.second)
        return false;
    it = result.first;
    auto &data = it->second;
    data.resize(size);
    memcpy(data.data(), buf, size);
    TriggerCallbacks(key, data.data(), data.size());
    return true;
}

bool DataShare::Remove(const char *key) {
    if (!ValidateKey(key))
        return false;

    std::lock_guard<std::mutex> guard(m_RWLock);

    auto it = m_DataMap.find(key);
    if (it == m_DataMap.end())
        return false;

    m_DataMap.erase(it);
    return true;
}

void *DataShare::GetUserData(size_t type) const {
    return m_UserData.GetData(type);
}

void *DataShare::SetUserData(void *data, size_t type) {
    return m_UserData.SetData(data, type);
}

bool DataShare::AddCallbacks(const char *key, DataShareCallback callback, void *userdata) const {
    auto it = m_CallbackMap.find(key);
    if (it == m_CallbackMap.end()) {
        m_CallbackMap[key] = {Callback(callback, userdata)};
    } else {
        it->second.emplace_back(callback, userdata);
    }
    return true;
}

void DataShare::TriggerCallbacks(const char *key, const void *data, size_t size) const {
    auto it = m_CallbackMap.find(key);
    if (it != m_CallbackMap.end()) {
        for (auto &cb : it->second) {
            cb.callback(key, data, size, cb.userdata);
        }
        m_CallbackMap.erase(it);
    }
}

bool DataShare::ValidateKey(const char *key) {
    if (!key || key[0] == '\0')
        return false;
    return true;
}