#ifndef BML_SCRIPTSTATEBAG_H
#define BML_SCRIPTSTATEBAG_H

#include <map>
#include <string>

namespace BML {

enum class ScriptStateValueType {
    Empty = 0,
    Bool = 1,
    Int = 2,
    Float = 3,
    String = 4,
};

class ScriptStateBag {
public:
    ScriptStateBag() = default;
    ScriptStateBag(const ScriptStateBag &other);
    ScriptStateBag &operator=(const ScriptStateBag &other);

    void AddRef();
    void Release();

    bool IsScriptAccessEnabled() const { return m_ScriptAccessEnabled; }
    void SetScriptAccessEnabled(bool enabled) { m_ScriptAccessEnabled = enabled; }
    bool IsReloadState() const { return m_ReloadState; }
    void SetReloadState(bool reloadState) { m_ReloadState = reloadState; }
    int GetStoredCount() const;

    bool Has(const std::string &key) const;
    bool Remove(const std::string &key);
    void Clear();
    int GetCount() const;
    std::string GetKey(int index) const;
    ScriptStateValueType GetType(const std::string &key) const;

    void SetBool(const std::string &key, bool value);
    bool GetBool(const std::string &key, bool defaultValue) const;
    void SetInt(const std::string &key, int value);
    int GetInt(const std::string &key, int defaultValue) const;
    void SetFloat(const std::string &key, float value);
    float GetFloat(const std::string &key, float defaultValue) const;
    void SetString(const std::string &key, const std::string &value);
    std::string GetString(const std::string &key, const std::string &defaultValue) const;

    ScriptStateBag *Clone() const;
    ScriptStateBag *CloneNoThrow() const;

private:
    struct Value {
        ScriptStateValueType Type = ScriptStateValueType::Empty;
        bool BoolValue = false;
        int IntValue = 0;
        float FloatValue = 0.0f;
        std::string StringValue;
    };

    std::map<std::string, Value> m_Values;
    int m_RefCount = 1;
    bool m_ScriptAccessEnabled = true;
    bool m_ReloadState = false;
};

class ScriptStateBagHandle {
public:
    ScriptStateBagHandle() = default;
    explicit ScriptStateBagHandle(ScriptStateBag *bag) : m_Bag(bag) {}
    ~ScriptStateBagHandle();

    ScriptStateBagHandle(const ScriptStateBagHandle &) = delete;
    ScriptStateBagHandle &operator=(const ScriptStateBagHandle &) = delete;

    ScriptStateBagHandle(ScriptStateBagHandle &&other) noexcept;
    ScriptStateBagHandle &operator=(ScriptStateBagHandle &&other) noexcept;

    ScriptStateBag *Get() const { return m_Bag; }
    ScriptStateBag *operator->() const { return m_Bag; }
    explicit operator bool() const { return m_Bag != nullptr; }
    ScriptStateBag *Release();
    void Reset(ScriptStateBag *bag = nullptr);

private:
    ScriptStateBag *m_Bag = nullptr;
};

} // namespace BML

#endif
