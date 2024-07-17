#include "DataShare.h"

using namespace BML;

std::mutex DataShare::s_MapMutex;
std::unordered_map<std::string, DataShare *> DataShare::s_DataShares;

DataShare *DataShare::GetInstance(const std::string &name) {
    auto it = s_DataShares.find(name);
    if (it == s_DataShares.end()) {
        return Create(name);
    }
    return it->second;
}

DataShare *DataShare::Create(std::string name) {
    return new DataShare(std::move(name));
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
    if (!ValidateKey(key)) return;

    std::lock_guard<std::mutex> guard(m_RWLock);

    auto it = m_DataMap.find(key);
    if (it == m_DataMap.end()) {
        AddCallbacks(key, callback, userdata);
    } else {
        TriggerCallbacks(key, it->second);
    }
}

void *DataShare::Get(const char *key) const {
    if (!ValidateKey(key)) return nullptr;

    std::lock_guard<std::mutex> guard(m_RWLock);

    auto it = m_DataMap.find(key);
    if (it == m_DataMap.end())
        return nullptr;
    return it->second;
}

void *DataShare::Set(const char *key, void *data) {
    if (!ValidateKey(key)) return nullptr;

    std::lock_guard<std::mutex> guard(m_RWLock);

    auto it = m_DataMap.find(key);
    if (it != m_DataMap.end()) {
        auto prev = it->second;
        m_DataMap[key] = data;
        return prev;
    } else {
        m_DataMap.emplace(key, data);
    }

    TriggerCallbacks(key, data);
    return nullptr;
}

void *DataShare::Insert(const char *key, void *data) {
    if (!ValidateKey(key)) return nullptr;

    std::lock_guard<std::mutex> guard(m_RWLock);

    auto it = m_DataMap.find(key);
    if (it != m_DataMap.end()) {
        return it->second;
    }
    m_DataMap.emplace(key, data);

    TriggerCallbacks(key, data);
    return nullptr;
}

void *DataShare::Remove(const char *key) {
    if (!ValidateKey(key)) return nullptr;

    std::lock_guard<std::mutex> guard(m_RWLock);

    auto it = m_DataMap.find(key);
    if (it == m_DataMap.end())
        return nullptr;

    auto prev = it->second;
    m_DataMap.erase(it);
    return prev;
}

void *DataShare::GetUserData(size_t type) const {
    return m_UserData.GetData(type);
}

void *DataShare::SetUserData(void *data, size_t type) {
    return m_UserData.SetData(data, type);
}

DataShare::DataShare(std::string name) : m_Name(std::move(name)) {
    std::lock_guard<std::mutex> lock{s_MapMutex};
    s_DataShares[m_Name] = this;
}

bool DataShare::AddCallbacks(const char *key, DataShareCallback callback, void *userdata) const {
    auto it = m_CallbackMap.find(key);
    if (it == m_CallbackMap.end()) return false;
    it->second.emplace_back(callback, userdata);
    return true;
}

void DataShare::TriggerCallbacks(const char *key, void *data) const {
    auto it = m_CallbackMap.find(key);
    if (it != m_CallbackMap.end()) {
        for (auto &cb : it->second) {
            cb.callback(key, data, cb.userdata);
        }
        m_CallbackMap.erase(it);
    }
}

bool DataShare::ValidateKey(const char *key) {
    if (!key || key[0] == '\0')
        return false;
    return true;
}