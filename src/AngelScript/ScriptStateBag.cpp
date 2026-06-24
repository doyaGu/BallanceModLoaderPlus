#include "ScriptStateBag.h"

#include <iterator>
#include <new>

namespace BML {

ScriptStateBag::ScriptStateBag(const ScriptStateBag &other)
    : m_Values(other.m_Values),
      m_ScriptAccessEnabled(other.m_ScriptAccessEnabled),
      m_ReloadState(other.m_ReloadState) {}

ScriptStateBag &ScriptStateBag::operator=(const ScriptStateBag &other) {
    if (this != &other) {
        m_Values = other.m_Values;
        m_ScriptAccessEnabled = other.m_ScriptAccessEnabled;
        m_ReloadState = other.m_ReloadState;
    }
    return *this;
}

void ScriptStateBag::AddRef() {
    ++m_RefCount;
}

void ScriptStateBag::Release() {
    if (--m_RefCount == 0)
        delete this;
}

int ScriptStateBag::GetStoredCount() const {
    return static_cast<int>(m_Values.size());
}

bool ScriptStateBag::Has(const std::string &key) const {
    if (!m_ScriptAccessEnabled)
        return false;
    return m_Values.find(key) != m_Values.end();
}

bool ScriptStateBag::Remove(const std::string &key) {
    if (!m_ScriptAccessEnabled)
        return false;
    return m_Values.erase(key) != 0;
}

void ScriptStateBag::Clear() {
    if (!m_ScriptAccessEnabled)
        return;
    m_Values.clear();
}

int ScriptStateBag::GetCount() const {
    if (!m_ScriptAccessEnabled)
        return 0;
    return static_cast<int>(m_Values.size());
}

std::string ScriptStateBag::GetKey(int index) const {
    if (!m_ScriptAccessEnabled)
        return {};
    if (index < 0 || index >= GetStoredCount())
        return {};
    auto it = m_Values.begin();
    std::advance(it, index);
    return it->first;
}

ScriptStateValueType ScriptStateBag::GetType(const std::string &key) const {
    if (!m_ScriptAccessEnabled)
        return ScriptStateValueType::Empty;
    const auto it = m_Values.find(key);
    return it == m_Values.end() ? ScriptStateValueType::Empty : it->second.Type;
}

void ScriptStateBag::SetBool(const std::string &key, bool value) {
    if (!m_ScriptAccessEnabled)
        return;
    Value &entry = m_Values[key];
    entry = Value();
    entry.Type = ScriptStateValueType::Bool;
    entry.BoolValue = value;
}

bool ScriptStateBag::GetBool(const std::string &key, bool defaultValue) const {
    if (!m_ScriptAccessEnabled)
        return defaultValue;
    const auto it = m_Values.find(key);
    return it != m_Values.end() && it->second.Type == ScriptStateValueType::Bool
               ? it->second.BoolValue
               : defaultValue;
}

void ScriptStateBag::SetInt(const std::string &key, int value) {
    if (!m_ScriptAccessEnabled)
        return;
    Value &entry = m_Values[key];
    entry = Value();
    entry.Type = ScriptStateValueType::Int;
    entry.IntValue = value;
}

int ScriptStateBag::GetInt(const std::string &key, int defaultValue) const {
    if (!m_ScriptAccessEnabled)
        return defaultValue;
    const auto it = m_Values.find(key);
    return it != m_Values.end() && it->second.Type == ScriptStateValueType::Int
               ? it->second.IntValue
               : defaultValue;
}

void ScriptStateBag::SetFloat(const std::string &key, float value) {
    if (!m_ScriptAccessEnabled)
        return;
    Value &entry = m_Values[key];
    entry = Value();
    entry.Type = ScriptStateValueType::Float;
    entry.FloatValue = value;
}

float ScriptStateBag::GetFloat(const std::string &key, float defaultValue) const {
    if (!m_ScriptAccessEnabled)
        return defaultValue;
    const auto it = m_Values.find(key);
    return it != m_Values.end() && it->second.Type == ScriptStateValueType::Float
               ? it->second.FloatValue
               : defaultValue;
}

void ScriptStateBag::SetString(const std::string &key, const std::string &value) {
    if (!m_ScriptAccessEnabled)
        return;
    Value &entry = m_Values[key];
    entry = Value();
    entry.Type = ScriptStateValueType::String;
    entry.StringValue = value;
}

std::string ScriptStateBag::GetString(const std::string &key, const std::string &defaultValue) const {
    if (!m_ScriptAccessEnabled)
        return defaultValue;
    const auto it = m_Values.find(key);
    return it != m_Values.end() && it->second.Type == ScriptStateValueType::String
               ? it->second.StringValue
               : defaultValue;
}

ScriptStateBag *ScriptStateBag::Clone() const {
    return new ScriptStateBag(*this);
}

ScriptStateBag *ScriptStateBag::CloneNoThrow() const {
    try {
        return new (std::nothrow) ScriptStateBag(*this);
    } catch (...) {
        return nullptr;
    }
}

ScriptStateBagHandle::~ScriptStateBagHandle() {
    Reset();
}

ScriptStateBagHandle::ScriptStateBagHandle(ScriptStateBagHandle &&other) noexcept
    : m_Bag(other.m_Bag) {
    other.m_Bag = nullptr;
}

ScriptStateBagHandle &ScriptStateBagHandle::operator=(ScriptStateBagHandle &&other) noexcept {
    if (this != &other) {
        Reset();
        m_Bag = other.m_Bag;
        other.m_Bag = nullptr;
    }
    return *this;
}

ScriptStateBag *ScriptStateBagHandle::Release() {
    ScriptStateBag *bag = m_Bag;
    m_Bag = nullptr;
    return bag;
}

void ScriptStateBagHandle::Reset(ScriptStateBag *bag) {
    if (m_Bag)
        m_Bag->Release();
    m_Bag = bag;
}

} // namespace BML
