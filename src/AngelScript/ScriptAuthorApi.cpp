#include "ScriptRuntimeScope.h"

#include <angelscript.h>
#include <algorithm>
#include <new>
#include <variant>
#include <cmath>

#include "Api/ConfigApi.h"
#include "Api/ModApi.h"
#include "BML/Config.hpp"
#include "BML/Input.hpp"
#include "BML/Command.hpp"
#include "BML/Engine.hpp"
#include "BML/Gameplay.h"
#include "BML/Scene.h"
#include "BML/Time.h"
#include "BML/Speedrun.h"
#include "BML/UI.h"
#include "Api/GameplayRegistrationApi.h"
#include "Api/BuiltinCapabilities.h"
#include "ScriptMod.h"
#include "ScriptModRuntime.h"
#include "ScriptStringInterop.h"

namespace BML {
namespace {
asIScriptEngine *g_Engine = nullptr;
const CKAngelScriptAdapter::Api *g_Api = nullptr;
std::string g_Error;

bool Check(int code, const char *declaration, const char **error) {
    if (code >= 0)
        return true;
    g_Error = std::string("Script author declaration failed: ") + declaration + " (" +
              std::to_string(code) + ")";
    if (error)
        *error = g_Error.c_str();
    return false;
}

Mod::Context Native(const ScriptRuntimeAccess &access) {
    return Mod::Detail::ContextAccess::Make(access.Handle());
}

template <class T> int Publish(Result<T> result, T &out) {
    if (!result)
        return result.Code();
    out = std::move(result.Value());
    return BML_OK;
}

class ScriptView {
public:
    explicit ScriptView(const std::shared_ptr<ScriptRuntimeScope> &scope) : Ticket(scope) {}
    void AddRef() {
        ++m_References;
    }
    void Release() {
        if (!--m_References)
            delete this;
    }
    bool IsValid() const {
        return ScriptRuntimeAccess(Ticket).Code() == BML_OK;
    }
    virtual ~ScriptView() = default;
    ScriptControlTicket Ticket;

private:
    unsigned m_References = 1;
};

struct ConfigValue {
    Config::Value Value;
    ConfigValue() = default;
    explicit ConfigValue(const std::string &value) : Value(value) {
        if (!BML::Detail::IsText(value))
            throw std::invalid_argument("ConfigValue string contains NUL");
    }
    explicit ConfigValue(bool value) : Value(value) {}
    explicit ConfigValue(int value) : Value(int32_t(value)) {}
    explicit ConfigValue(float value) : Value(value) {}
    explicit ConfigValue(CKKEYBOARD value) : Value(uint32_t(value)) {}
    int Type() const {
        static constexpr uint32_t types[]{BML_CONFIG_TYPE_NONE,    BML_CONFIG_TYPE_STRING,
                                          BML_CONFIG_TYPE_BOOLEAN, BML_CONFIG_TYPE_INTEGER,
                                          BML_CONFIG_TYPE_FLOAT,   BML_CONFIG_TYPE_KEY};
        return types[Value.index()];
    }
    template <class T> int Read(T &out) const {
        const auto *value = std::get_if<T>(&Value);
        if (!value)
            return BML_ERROR_TYPE_MISMATCH;
        out = *value;
        return BML_OK;
    }
    int ReadString(std::string &out) const try {
        return Read(out);
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int ReadBoolean(bool &out) const try { return Read(out); } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int ReadInteger(int &out) const try { return Read(out); } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int ReadFloat(float &out) const try { return Read(out); } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int ReadKey(CKKEYBOARD &out) const try {
        uint32_t value = 0;
        int code = Read(value);
        if (code == BML_OK)
            out = static_cast<CKKEYBOARD>(value);
        return code;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
};

int ReadStrings(const void *array, std::vector<std::string> &out) try {
    out.clear();
    if (!array)
        return BML_OK;
    if (!g_Api || !CKAngelScriptAdapter::IsArrayOf(*g_Api, array, "string", ""))
        return BML_ERROR_TYPE_MISMATCH;
    CKDWORD count = 0;
    if (g_Api->ArrayGetSize(const_cast<void *>(array), &count) != CKAS_OK)
        return BML_ERROR_FAIL;
    out.reserve(count);
    for (CKDWORD i = 0; i < count; ++i) {
        const void *address = nullptr;
        if (g_Api->ArrayGetConstElementAddress(array, i, &address) != CKAS_OK || !address)
            return BML_ERROR_FAIL;
        std::string value;
        if (!ScriptStringInterop::ReadStringObject(g_Engine, address, value))
            return BML_ERROR_TYPE_MISMATCH;
        out.push_back(std::move(value));
    }
    return BML_OK;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}

struct PropertyDefinition {
    std::string Category, Key;
    ConfigValue DefaultValue;
    std::string Comment;
    uint32_t Editor = BML_CONFIG_EDITOR_DEFAULT;
    void *Choices = nullptr;
    PropertyDefinition() {
        if (g_Api && g_Api->CreateArrayByType && g_Engine &&
            g_Api->CreateArrayByType(g_Engine->GetTypeInfoByDecl("array<string>"), 0, &Choices) !=
                CKAS_OK)
            throw std::bad_alloc();
    }
    ~PropertyDefinition() {
        if (Choices && g_Api)
            g_Api->ArrayRelease(Choices);
    }
    PropertyDefinition(const PropertyDefinition &other)
        : Category(other.Category), Key(other.Key), DefaultValue(other.DefaultValue),
          Comment(other.Comment), Editor(other.Editor), Choices(other.Choices) {
        if (Choices)
            g_Api->ArrayAddRef(Choices);
    }
    PropertyDefinition &operator=(const PropertyDefinition &other) {
        if (this != &other) {
            PropertyDefinition copy(other);
            std::swap(Category, copy.Category);
            std::swap(Key, copy.Key);
            std::swap(DefaultValue, copy.DefaultValue);
            std::swap(Comment, copy.Comment);
            std::swap(Editor, copy.Editor);
            std::swap(Choices, copy.Choices);
        }
        return *this;
    }
    void *GetChoices() const {
        if (Choices)
            g_Api->ArrayAddRef(Choices);
        return Choices;
    }
    void SetChoices(void *array) {
        if (array)
            g_Api->ArrayAddRef(array);
        if (Choices)
            g_Api->ArrayRelease(Choices);
        Choices = array;
    }
    int Encode(Config::PropertyDefinition &out) const try {
        out = {Category, Key, DefaultValue.Value, Comment, Editor, {}};
        return ReadStrings(Choices, out.Choices);
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int Decode(const Config::PropertyDefinition &value) try {
        Category = value.Category;
        Key = value.Key;
        DefaultValue.Value = value.DefaultValue;
        Comment = value.Comment;
        Editor = value.Editor;
        if (!Choices || !g_Api)
            return BML_ERROR_UNAVAILABLE;
        if (g_Api->ArrayClear(Choices) != CKAS_OK)
            return BML_ERROR_FAIL;
        for (const auto &choice : value.Choices) {
            ScriptStringInterop::ScriptStringObject text(g_Engine, choice);
            if (!text.Get() || g_Api->ArrayInsertLast(Choices, text.Get()) != CKAS_OK)
                return BML_ERROR_OUT_OF_MEMORY;
        }
        return BML_OK;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
};
struct PropertyInfo {
    uint64_t Handle = 0;
    PropertyDefinition Definition;
    ConfigValue CurrentValue;
};

class ConfigProperty final : public ScriptView {
public:
    using ScriptView::ScriptView;
    Config::Property Property;
    bool IsValid() const {
        return ScriptView::IsValid() && Property.IsValid();
    }
    int Read(PropertyInfo &out) const try {
        ScriptRuntimeAccess access(Ticket);
        if (access.Code() != BML_OK)
            return access.Code();
        auto result = Property.Read();
        if (!result)
            return result.Code();
        PropertyInfo value;
        value.Handle = result.Value().Handle;
        int code = value.Definition.Decode(result.Value().Definition);
        if (code != BML_OK)
            return code;
        value.CurrentValue.Value = result.Value().CurrentValue;
        out = std::move(value);
        return BML_OK;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int Set(const ConfigValue &value) const try {
        ScriptRuntimeAccess access(Ticket);
        return access.Code() == BML_OK ? Property.Set(value.Value).Code() : access.Code();
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int SetDefault(const ConfigValue &value) const try {
        ScriptRuntimeAccess access(Ticket);
        return access.Code() == BML_OK ? Property.SetDefault(value.Value).Code() : access.Code();
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int SetComment(const std::string &value) const try {
        ScriptRuntimeAccess access(Ticket);
        return access.Code() == BML_OK ? Property.SetComment(value).Code() : access.Code();
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int SetEditor(uint32_t value) const try {
        ScriptRuntimeAccess access(Ticket);
        return access.Code() == BML_OK ? Property.SetEditor(value).Code() : access.Code();
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int SetChoices(const void *array) const try {
        ScriptRuntimeAccess access(Ticket);
        if (access.Code() != BML_OK)
            return access.Code();
        std::vector<std::string> values;
        int code = ReadStrings(array, values);
        return code == BML_OK ? Property.SetChoices(values).Code() : code;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    template <class T> T Get(T fallback) const {
        ScriptRuntimeAccess access(Ticket);
        return access.Code() == BML_OK ? Property.Get<T>(std::move(fallback)) : fallback;
    }
    std::string GetString(const std::string &fallback) const {
        return Get(fallback);
    }
    bool GetBoolean(bool fallback) const {
        return Get(fallback);
    }
    int GetInteger(int fallback) const try {
        return Get(fallback);
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    float GetFloat(float fallback) const {
        return Get(fallback);
    }
    CKKEYBOARD GetKey(CKKEYBOARD fallback) const {
        return static_cast<CKKEYBOARD>(Get(uint32_t(fallback)));
    }
};

class ModRef final : public ScriptView {
public:
    using ScriptView::ScriptView;
    Mod::View Mod;
    int Read(Mod::Info &out) const try {
        ScriptRuntimeAccess access(Ticket);
        return access.Code() == BML_OK ? Publish(Mod.Read(), out) : access.Code();
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    bool IsValid() const {
        Mod::Info info;
        return Read(info) == BML_OK;
    }
    int CheckDependencies(bool &out) const try {
        ScriptRuntimeAccess access(Ticket);
        if (access.Code() != BML_OK)
            return access.Code();
        auto result = Mod.Read();
        if (!result)
            return result.Code();
        auto *record = access.Context().GetModLoader().ResolveModHandle(result.Value().Handle);
        if (!record)
            return BML_ERROR_INVALID_HANDLE;
        out = access.Context().GetModLoader().CheckDependencies(record) != 0;
        return BML_OK;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
};

class KeyboardState {
public:
    std::shared_ptr<const Api::InputApi::Keyboard> Value;
    void AddRef() {
        ++m_References;
    }
    void Release() {
        if (!--m_References)
            delete this;
    }
    bool IsValid() const {
        return Value != nullptr;
    }
    bool Attached() const {
        return Value && Value->Attached;
    }
    bool RepetitionEnabled() const {
        return Value && Value->Repetition;
    }
    uint32_t KeyCount() const {
        return Value ? uint32_t(Value->Keys.size()) : 0;
    }
    uint32_t EventCount() const {
        return Value ? uint32_t(Value->Events.size()) : 0;
    }
    int ReadKey(uint32_t key, BML_InputKeyState &out) const try {
        if (!Value)
            return BML_ERROR_INVALID_HANDLE;
        if (key >= Value->Keys.size())
            return BML_ERROR_INVALID_PARAMETER;
        out = Value->Keys[key];
        return BML_OK;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int ReadEvent(uint32_t index, BML_InputKeyEvent &out) const try {
        if (!Value)
            return BML_ERROR_INVALID_HANDLE;
        if (index >= Value->Events.size())
            return BML_ERROR_NOT_FOUND;
        out = Value->Events[index];
        return BML_OK;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    bool IsKeyDown(uint32_t key) const {
        return Value && key < 256 && Value->Keys[key].Down;
    }
    bool IsKeyUp(uint32_t key) const {
        return !Value || key >= 256 || Value->Keys[key].Up;
    }
    bool IsKeyPressed(uint32_t key) const {
        return Value && key < 256 && Value->Keys[key].Pressed;
    }
    bool IsKeyReleased(uint32_t key) const {
        return Value && key < 256 && Value->Keys[key].Released;
    }
    bool IsKeyToggled(uint32_t key) const {
        return Value && key < 256 && Value->Keys[key].Toggled;
    }

private:
    unsigned m_References = 1;
};

class InputBlock final {
public:
    explicit InputBlock(const std::shared_ptr<ScriptRuntimeScope> &scope)
        : Control(std::make_shared<ScriptControlTicket>(scope)) {}
    void AddRef() {
        ++m_References;
    }
    void Release() {
        if (!--m_References)
            delete this;
    }

private:
    unsigned m_References = 1;

public:
    BML_InputBlockHandle Handle = 0;
    std::shared_ptr<ScriptControlTicket> Control;
    bool IsValid() const {
        return Control && !Control->Terminal && ScriptRuntimeAccess(*Control).Code() == BML_OK;
    }
    int ReleaseBlock() try {
        ScriptRuntimeAccess access(*Control, true);
        if (access.Code() != BML_OK || Control->Terminal)
            return access.Code();
        int code = access.Context().GetInputApi().ReleaseBlock(
            access.Context(), access.Owner().Registration(), Handle);
        if (code == BML_OK) {
            Control->Terminal = true;
            auto &blocks = access.Scope()->Blocks;
            blocks.erase(std::remove_if(blocks.begin(), blocks.end(),
                                        [&](const auto &entry) { return entry.first == Handle; }),
                         blocks.end());
        }
        return code;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
};
} // namespace

class ScriptConfigView final : public ScriptView {
public:
    using ScriptView::ScriptView;
    int DeclareProperty(const PropertyDefinition &definition, ConfigProperty *&out) const try {
        out = nullptr;
        ScriptRuntimeAccess access(Ticket);
        if (access.Code() != BML_OK)
            return access.Code();
        if (!access.Scope()->Accepting)
            return BML_ERROR_FROZEN;
        Config::PropertyDefinition native;
        int code = definition.Encode(native);
        if (code != BML_OK)
            return code;
        auto value = std::make_unique<ConfigProperty>(access.Scope());
        auto result = Native(access).Config().DeclareProperty(native);
        if (!result)
            return result.Code();
        value->Property = std::move(result.Value());
        out = value.release();
        return BML_OK;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int FindProperty(const std::string &category, const std::string &key,
                     ConfigProperty *&out) const try {
        out = nullptr;
        ScriptRuntimeAccess access(Ticket);
        if (access.Code() != BML_OK)
            return access.Code();
        auto value = std::make_unique<ConfigProperty>(access.Scope());
        auto result = Native(access).Config().FindProperty(category, key);
        if (!result)
            return result.Code();
        value->Property = std::move(result.Value());
        out = value.release();
        return BML_OK;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int HasCategory(const std::string &category, bool &out) const try {
        ScriptRuntimeAccess access(Ticket);
        return access.Code() == BML_OK ? Publish(Native(access).Config().HasCategory(category), out)
                                       : access.Code();
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int HasKey(const std::string &category, const std::string &key, bool &out) const try {
        ScriptRuntimeAccess access(Ticket);
        return access.Code() == BML_OK ? Publish(Native(access).Config().HasKey(category, key), out)
                                       : access.Code();
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int SetCategoryComment(const std::string &category, const std::string &comment) const try {
        ScriptRuntimeAccess access(Ticket);
        return access.Code() == BML_OK
                   ? Native(access).Config().SetCategoryComment(category, comment).Code()
                   : access.Code();
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
};

class ScriptInputView final : public ScriptView {
public:
    using ScriptView::ScriptView;
    int ReadKeyboard(KeyboardState *&out, uint32_t flags) const try {
        out = nullptr;
        ScriptRuntimeAccess access(Ticket);
        if (access.Code() != BML_OK)
            return access.Code();
        auto value = std::make_unique<KeyboardState>();
        int code =
            access.Context().GetInputApi().ShareKeyboard(access.Context(), flags, value->Value);
        if (code == BML_OK)
            out = value.release();
        return code;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int ReadMouse(BML_InputMouseState &out, uint32_t flags) const try {
        ScriptRuntimeAccess access(Ticket);
        return access.Code() == BML_OK ? Publish(Native(access).Input().ReadMouse(flags), out)
                                       : access.Code();
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int ReadJoystick(uint32_t index, BML_InputJoystickState &out, uint32_t flags) const try {
        ScriptRuntimeAccess access(Ticket);
        return access.Code() == BML_OK
                   ? Publish(Native(access).Input().ReadJoystick(index, flags), out)
                   : access.Code();
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int GetKeyName(uint32_t key, std::string &out) const try {
        ScriptRuntimeAccess access(Ticket);
        return access.Code() == BML_OK ? Publish(Native(access).Input().GetKeyName(key), out)
                                       : access.Code();
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int GetKeyFromName(const std::string &name, uint32_t &out) const try {
        ScriptRuntimeAccess access(Ticket);
        return access.Code() == BML_OK ? Publish(Native(access).Input().GetKeyFromName(name), out)
                                       : access.Code();
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int EnableKeyboardRepetition(bool enabled) const try {
        ScriptRuntimeAccess access(Ticket);
        return access.Code() == BML_OK
                   ? Native(access).Input().EnableKeyboardRepetition(enabled).Code()
                   : access.Code();
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int AcquireBlock(uint32_t mask, InputBlock *&out) const try {
        out = nullptr;
        ScriptRuntimeAccess access(Ticket);
        if (access.Code() != BML_OK)
            return access.Code();
        if (!access.Scope()->Accepting)
            return BML_ERROR_FROZEN;
        auto value = std::make_unique<InputBlock>(access.Scope());
        auto &blocks = access.Scope()->Blocks;
        blocks.reserve(blocks.size() + 1);
        int code = access.Context().GetInputApi().AcquireBlock(
            access.Context(), access.Owner().Registration(), mask, &value->Handle);
        if (code == BML_OK) {
            blocks.emplace_back(value->Handle, value->Control);
            out = value.release();
        }
        return code;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int ReadBlockCount(uint32_t device, uint32_t &out) const try {
        ScriptRuntimeAccess access(Ticket);
        return access.Code() == BML_OK ? Publish(Native(access).Input().ReadBlockCount(device), out)
                                       : access.Code();
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int Pause(bool enabled) const try {
        ScriptRuntimeAccess access(Ticket);
        return access.Code() == BML_OK ? Native(access).Input().Pause(enabled).Code()
                                       : access.Code();
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int ReadCursor(BML_InputCursorState &out) const try {
        ScriptRuntimeAccess access(Ticket);
        return access.Code() == BML_OK ? Publish(Native(access).Input().ReadCursor(), out)
                                       : access.Code();
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int ShowCursor(bool enabled) const try {
        ScriptRuntimeAccess access(Ticket);
        return access.Code() == BML_OK ? Native(access).Input().ShowCursor(enabled).Code()
                                       : access.Code();
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int SetSystemCursor(uint32_t shape) const try {
        ScriptRuntimeAccess access(Ticket);
        return access.Code() == BML_OK ? Native(access).Input().SetSystemCursor(shape).Code()
                                       : access.Code();
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
};

class ScriptTimersView final : public ScriptView {
public:
    using ScriptView::ScriptView;
    int Register(const ScriptTimerDefinition &definition, asIScriptFunction *callback,
                 ScriptTimerRef *&out) const try {
        out = nullptr;
        ScriptRuntimeAccess access(Ticket);
        return access.Code() == BML_OK
                   ? access.Owner().TimersForAuthoring().Register(definition, callback, true, out)
                   : access.Code();
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int Set(double ms, uint64_t ticks, bool tick, bool interval, asIScriptFunction *callback,
            ScriptTimerRef *&out, const std::string &name) const try {
        out = nullptr;
        ScriptRuntimeAccess access(Ticket);
        if (access.Code() != BML_OK)
            return access.Code();
        ScriptTimerDefinition definition;
        definition.Name = name;
        definition.Type = interval ? BML_TIMER_INTERVAL : BML_TIMER_ONCE;
        definition.TimeBase = tick ? BML_TIMER_TIMEBASE_TICK : BML_TIMER_TIMEBASE_TIME;
        definition.DelayMs = ms;
        definition.DelayTicks = ticks;
        return access.Owner().TimersForAuthoring().Register(definition, callback, interval, out);
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int SetTimeout(double ms, asIScriptFunction *cb, ScriptTimerRef *&out,
                   const std::string &name) const try {
        return Set(ms, 0, false, false, cb, out, name);
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int SetInterval(double ms, asIScriptFunction *cb, ScriptTimerRef *&out,
                    const std::string &name) const try {
        return Set(ms, 0, false, true, cb, out, name);
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int SetTimeoutTicks(uint64_t ticks, asIScriptFunction *cb, ScriptTimerRef *&out,
                        const std::string &name) const try {
        return Set(0, ticks, true, false, cb, out, name);
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int SetIntervalTicks(uint64_t ticks, asIScriptFunction *cb, ScriptTimerRef *&out,
                         const std::string &name) const try {
        return Set(0, ticks, true, true, cb, out, name);
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
};

class ScriptCommandsView final : public ScriptView {
public:
    using ScriptView::ScriptView;
    int Register(const ScriptCommandDefinition &definition, asIScriptFunction *execute,
                 ScriptCommandRef *&out, asIScriptFunction *complete) const try {
        out = nullptr;
        ScriptRuntimeAccess access(Ticket);
        if (access.Code() != BML_OK)
            return access.Code();
        if (!access.Scope()->Accepting ||
            !Api::AcceptsModWork(access.Context(), access.Owner().Registration()))
            return BML_ERROR_FROZEN;
        if (!BML::Detail::IsText(definition.Name) || !BML::Detail::IsText(definition.Alias) ||
            !BML::Detail::IsText(definition.Description) ||
            !BML::Detail::IsText(definition.Usage) || !BML::Detail::IsText(definition.Category))
            return BML_ERROR_INVALID_PARAMETER;
        if (access.Context().FindCommand(definition.Name.c_str()) ||
            (!definition.Alias.empty() && access.Context().FindCommand(definition.Alias.c_str())))
            return BML_ERROR_ALREADY_EXISTS;
        return access.Owner().CommandsForAuthoring().Register(definition, execute, complete, out);
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int Find(const std::string &name, CommandInfo &out) const try {
        ScriptRuntimeAccess access(Ticket);
        if (access.Code() != BML_OK)
            return access.Code();
        if (!BML::Detail::IsText(name) || name.empty())
            return BML_ERROR_INVALID_PARAMETER;
        CommandInfo value;
        if (!access.Context().FindCommandInfo(name.c_str(), value))
            return BML_ERROR_NOT_FOUND;
        out = std::move(value);
        return BML_OK;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
    int ExecuteLine(const std::string &line, int &out) const try {
        ScriptRuntimeAccess access(Ticket);
        if (access.Code() != BML_OK)
            return access.Code();
        return Publish(Native(access).Commands().ExecuteLine(line), out);
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
};

ScriptRuntimeScope::~ScriptRuntimeScope() {
    if (Config)
        Config->Release();
    if (Input)
        Input->Release();
    if (Timers)
        Timers->Release();
    if (Commands)
        Commands->Release();
}

bool BindScriptRuntime(ScriptMod &owner, std::shared_ptr<ScriptRuntimeScope> &scope) {
    if (!RetireScriptRuntime(scope))
        return false;
    try {
        auto state = std::make_shared<ScriptRuntimeScope>();
        auto &loader = owner.GetModContext()->GetModLoader();
        state->Owner = loader.GetModGeneration(&owner.Registration());
        if (state->Owner == 0)
            return false;
        state->Context = loader.IssueModContext(&owner.Registration());
        state->Config = new ScriptConfigView(state);
        state->Input = new ScriptInputView(state);
        state->Timers = new ScriptTimersView(state);
        state->Commands = new ScriptCommandsView(state);
        owner.BorrowContextView()->BindRuntime(state, state->Owner);
        scope = std::move(state);
        return true;
    } catch (const std::bad_alloc &) {
        return false;
    }
}

namespace {
template <class T, T *ScriptRuntimeScope::*Member>
T *ContextService(const ScriptModContextView *view) {
    ScriptRuntimeAccess access(*view);
    if (access.Code() != BML_OK)
        return nullptr;
    auto *value = access.Scope().get()->*Member;
    if (value)
        value->AddRef();
    return value;
}
bool ContextValid(const ScriptModContextView *view) {
    return ScriptRuntimeAccess(view->RuntimeScope()).Code() == BML_OK;
}
int ContextOwner(const ScriptModContextView *view, ModRef *&out) try {
    out = nullptr;
    ScriptRuntimeAccess access(*view);
    if (access.Code() != BML_OK)
        return access.Code();
    auto ref = std::make_unique<ModRef>(access.Scope());
    auto result = Native(access).Owner();
    if (!result)
        return result.Code();
    ref->Mod = result.Value();
    out = ref.release();
    return BML_OK;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int ContextFindMod(const ScriptModContextView *view, const std::string &id, ModRef *&out) try {
    out = nullptr;
    ScriptRuntimeAccess access(*view);
    if (access.Code() != BML_OK)
        return access.Code();
    auto ref = std::make_unique<ModRef>(access.Scope());
    auto result = Native(access).FindMod(id);
    if (!result)
        return result.Code();
    ref->Mod = result.Value();
    out = ref.release();
    return BML_OK;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int ContextLog(const ScriptModContextView *view, const std::string &message, uint32_t level) try {
    ScriptRuntimeAccess access(*view);
    if (access.Code() != BML_OK)
        return access.Code();
    if (!BML::Detail::IsText(message))
        return BML_ERROR_INVALID_PARAMETER;
    return Api::ModContextInterface().Log(access.Handle(), level, message.data(), message.size());
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int ContextLogInfo(const ScriptModContextView *view, const std::string &s) try {
    return ContextLog(view, s, BML_LOG_INFO);
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int ContextLogWarn(const ScriptModContextView *view, const std::string &s) try {
    return ContextLog(view, s, BML_LOG_WARN);
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int ContextLogError(const ScriptModContextView *view, const std::string &s) try {
    return ContextLog(view, s, BML_LOG_ERROR);
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int ContextDirectory(const ScriptModContextView *view, uint32_t directory, std::string &out) try {
    ScriptRuntimeAccess access(*view);
    return access.Code() == BML_OK ? Publish(Native(access).GetDirectory(directory), out)
                                   : access.Code();
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int ContextResourcePath(const ScriptModContextView *view, const std::string &path,
                        std::string &out) try {
    ScriptRuntimeAccess access(*view);
    return access.Code() == BML_OK ? Publish(Native(access).GetResourcePath(path), out)
                                   : access.Code();
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int ContextResourceExists(const ScriptModContextView *view, const std::string &path,
                          bool &out) try {
    ScriptRuntimeAccess access(*view);
    return access.Code() == BML_OK ? Publish(Native(access).ResourceExists(path), out)
                                   : access.Code();
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}

int CheckArray(asIScriptGeneric *gen, const char *element, void *&array, unsigned index = 0) try {
    const int typeId = gen->GetArgTypeId(index);
    if (typeId < 0 || (typeId & (asTYPEID_OBJHANDLE | asTYPEID_HANDLETOCONST)))
        return BML_ERROR_TYPE_MISMATCH;
    auto *type = gen->GetEngine()->GetTypeInfoById(typeId);
    auto *expected = gen->GetEngine()->GetTypeInfoByDecl(element);
    const std::string declaration = std::string("array<") + element + ">";
    auto *arrayTemplate = gen->GetEngine()->GetTypeInfoByDecl(declaration.c_str());
    if (!type || !expected || type != arrayTemplate || type->GetSubTypeCount() != 1 ||
        type->GetSubTypeId() != expected->GetTypeId() ||
        std::string(type->GetName()) != arrayTemplate->GetName() ||
        std::string(type->GetNamespace()) != arrayTemplate->GetNamespace())
        return BML_ERROR_TYPE_MISMATCH;
    array = gen->GetArgAddress(index);
    asITypeInfo *actual = nullptr;
    if (!array || !g_Api || !g_Api->ArrayGetArrayType ||
        g_Api->ArrayGetArrayType(array, &actual) != CKAS_OK || actual != type)
        return BML_ERROR_TYPE_MISMATCH;
    return BML_OK;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}

template <class T> int StoreArray(void *array, const std::vector<T> &values) {
    if (g_Api->ArrayClear(array) != CKAS_OK)
        return BML_ERROR_FAIL;
    for (const auto &value : values) {
        if (g_Api->ArrayInsertLast(array, &value) != CKAS_OK) {
            g_Api->ArrayClear(array);
            return BML_ERROR_OUT_OF_MEMORY;
        }
    }
    return BML_OK;
}
void ReadMods(asIScriptGeneric *gen) {
    void *array = nullptr;
    const int code = BML::Detail::Guard([&]() -> int {
        int status = CheckArray(gen, "BML::ModInfo", array);
        if (status != BML_OK)
            return status;
        if (g_Api->ArrayClear(array) != CKAS_OK)
            return BML_ERROR_FAIL;
        auto *view = static_cast<ScriptModContextView *>(gen->GetObject());
        ScriptRuntimeAccess access(*view);
        if (access.Code() != BML_OK)
            return access.Code();
        auto result = Native(access).ReadMods();
        return result ? StoreArray(array, result.Value()) : result.Code();
    });
    if (code != BML_OK && array)
        g_Api->ArrayClear(array);
    gen->SetReturnDWord(code);
}
void ReadCommands(asIScriptGeneric *gen) {
    void *array = nullptr;
    const int code = BML::Detail::Guard([&]() -> int {
        int status = CheckArray(gen, "BML::CommandInfo", array);
        if (status != BML_OK)
            return status;
        if (g_Api->ArrayClear(array) != CKAS_OK)
            return BML_ERROR_FAIL;
        auto *view = static_cast<ScriptCommandsView *>(gen->GetObject());
        ScriptRuntimeAccess access(view->Ticket);
        if (access.Code() != BML_OK)
            return access.Code();
        return StoreArray(array, access.Context().GetCommandContext().GetCommandSnapshot());
    });
    if (code != BML_OK && array)
        g_Api->ArrayClear(array);
    gen->SetReturnDWord(code);
}
int ReadResource(const ScriptModContextView *view, const std::string &path, void *array) try {
    ScriptRuntimeAccess access(*view);
    if (access.Code() != BML_OK)
        return access.Code();
    auto bytes = Native(access).ReadResource(path);
    if (!bytes)
        return bytes.Code();
    if (!array || !g_Api || !g_Api->ArrayResize || !g_Api->ArrayGetElementAddress)
        return BML_ERROR_UNAVAILABLE;
    if (bytes.Value().size() > UINT32_MAX ||
        g_Api->ArrayResize(array, CKDWORD(bytes.Value().size())) != CKAS_OK)
        return BML_ERROR_OUT_OF_MEMORY;
    for (CKDWORD i = 0; i < bytes.Value().size(); ++i) {
        void *out = nullptr;
        if (g_Api->ArrayGetElementAddress(array, i, &out) != CKAS_OK || !out) {
            g_Api->ArrayClear(array);
            return BML_ERROR_FAIL;
        }
        *static_cast<uint8_t *>(out) = bytes.Value()[i];
    }
    return BML_OK;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}

int DependencyAt(const Mod::Definition *definition, unsigned index, Mod::Dependency &out) try {
    if (index >= definition->Dependencies.size())
        return BML_ERROR_NOT_FOUND;
    out = definition->Dependencies[index];
    return BML_OK;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
unsigned DependencyCount(const Mod::Definition *definition) {
    return unsigned(definition->Dependencies.size());
}

int RegisterMenuPage(const ScriptModContextView *view, const ScriptMenuPageDefinition &definition,
                     asIScriptFunction *draw, ScriptMenuPageRef *&out, asIScriptFunction *enter,
                     asIScriptFunction *leave) try {
    out = nullptr;
    ScriptRuntimeAccess access(*view);
    if (access.Code() != BML_OK)
        return access.Code();
    if (!access.Scope()->Accepting ||
        !Api::AcceptsModWork(access.Context(), access.Owner().Registration()))
        return BML_ERROR_FROZEN;
    if (!BML::Detail::IsText(definition.Description))
        return BML_ERROR_INVALID_PARAMETER;
    return access.Owner().MenuPagesForAuthoring().Register(definition, draw, enter, leave, out);
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}

ConfigProperty *EventProperty(const ScriptConfigEventView *event) {
    ScriptRuntimeAccess access(event->PropertyScope());
    if (access.Code() != BML_OK || !event->PropertyHandle())
        return nullptr;
    ConfigProperty *out = nullptr;
    if (access.Scope()->Config->FindProperty(event->GetCategory(), event->GetKey(), out) != BML_OK)
        return nullptr;
    auto info = out->Property.Read();
    if (!info || info.Value().Handle != event->PropertyHandle()) {
        out->Release();
        return nullptr;
    }
    return out;
}

int CurrentScope(std::weak_ptr<ScriptRuntimeScope> &out) try {
    auto *owner = ScriptModRuntime::GetCurrentScriptMod();
    if (!owner)
        return BML_ERROR_ACCESS_DENIED;
    out = owner->BorrowContextView()->RuntimeScope();
    return BML_OK;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
template <class F> int Current(F &&body) {
    std::weak_ptr<ScriptRuntimeScope> scope;
    int code = CurrentScope(scope);
    if (code != BML_OK)
        return code;
    ScriptRuntimeAccess access(scope);
    return access.Code() == BML_OK ? body(access) : access.Code();
}
bool ObjectIsNull(const BML_ObjectRef *value) {
    return !value->Domain;
}
int ReadGameState(Gameplay::State &out) try {
    return Current([&](auto &) { return Gameplay::ReadState(out); });
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int ReadCheat(bool &out) try {
    return Current([&](auto &) { return Gameplay::ReadCheatEnabled(out); });
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int ReadSceneObject(const BML_ObjectRef &object, Scene::ObjectInfo &out) try {
    return Current([&](auto &) { return Scene::ReadObject(object, out); });
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int ReadTransform(const BML_ObjectRef &object, BML_SceneEntityTransform &out) try {
    return Current([&](auto &) { return Scene::ReadEntityTransform(object, out); });
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int FindObject(const std::string &name, BML_ObjectRef &out) try {
    return Current([&](auto &) { return Scene::FindObject(name, out); });
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int FindObjectOfClass(const std::string &name, int type, BML_ObjectRef &out) try {
    return Current([&](auto &) { return Scene::FindObject(name, type, out); });
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int ReadClock(BML_TimeClock &out) try {
    return Current([&](auto &access) {
        auto *time = access.Context().GetTimeManager();
        if (!time)
            return BML_ERROR_UNAVAILABLE;
        out = {time->GetTime(), time->GetAbsoluteTime(), time->GetLastDeltaTime(),
               time->GetMainTickCount()};
        return BML_OK;
    });
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int ReadSpeedrun(BML_SpeedrunTimerState &out) try {
    return Current([&](auto &) { return Speedrun::ReadTimerState(out); });
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int ReadHUD(UI::HUDState &out) try {
    return Current([&](auto &) { return UI::ReadHUDState(out); });
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int AddMessage(const std::string &message) {
    return Current([&](auto &) { return UI::AddMessage(message); });
}
int ClearMessages() {
    return Current([&](auto &) { return UI::ClearMessages(); });
}
int OpenModsMenu() {
    return Current([&](auto &) { return UI::OpenModsMenu(); });
}
int CloseModsMenu() {
    return Current([&](auto &) { return UI::CloseModsMenu(); });
}
int OpenMapMenu() {
    return Current([&](auto &) { return UI::OpenMapMenu(); });
}
int CloseMapMenu() {
    return Current([&](auto &) { return UI::CloseMapMenu(); });
}
int SetHUDMode(int mode) {
    return Current([&](auto &) { return UI::SetHUDMode(mode); });
}
int ShowTitle(bool visible) {
    return Current([&](auto &) { return UI::ShowTitle(visible); });
}
int ShowFPS(bool visible) {
    return Current([&](auto &) { return UI::ShowFPS(visible); });
}
int SetTimerVisible(bool visible) {
    return Current([&](auto &) { return Speedrun::SetTimerVisible(visible); });
}
int StartTimer() {
    return Current([&](auto &) { return Speedrun::StartTimer(); });
}
int PauseTimer() {
    return Current([&](auto &) { return Speedrun::PauseTimer(); });
}
int ResetTimer() {
    return Current([&](auto &) { return Speedrun::ResetTimer(); });
}

int EnableCheat(const ScriptModContextView &view, bool value) try {
    ScriptRuntimeAccess a(view);
    return a.Code() == BML_OK ? Api::EnableCheat(a.Handle(), value) : a.Code();
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int ExitGame(const ScriptModContextView &view) try {
    ScriptRuntimeAccess a(view);
    return a.Code() == BML_OK ? Api::ExitGame(a.Handle()) : a.Code();
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int SetIC(const ScriptModContextView &view, const BML_ObjectRef &object, bool hierarchy) try {
    ScriptRuntimeAccess a(view);
    return a.Code() == BML_OK ? Api::SetIC(a.Handle(), object, hierarchy) : a.Code();
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int RestoreIC(const ScriptModContextView &view, const BML_ObjectRef &object, bool hierarchy) try {
    ScriptRuntimeAccess a(view);
    return a.Code() == BML_OK ? Api::RestoreIC(a.Handle(), object, hierarchy) : a.Code();
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int Show(const ScriptModContextView &view, const BML_ObjectRef &object, uint32_t option,
         bool hierarchy) try {
    ScriptRuntimeAccess a(view);
    return a.Code() == BML_OK ? Api::Show(a.Handle(), object, option, hierarchy) : a.Code();
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int SkipNextRender(const ScriptModContextView &view) try {
    ScriptRuntimeAccess a(view);
    return a.Code() == BML_OK ? Api::SkipNextRender(a.Handle()) : a.Code();
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int AdjustFOV(const ScriptModContextView &view, bool enabled) try {
    ScriptRuntimeAccess a(view);
    return a.Code() == BML_OK ? Api::SetWidescreenAdjustment(a.Handle(), enabled) : a.Code();
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int BorrowContext(const ScriptModContextView &view, CKContext *&out) try {
    out = nullptr;
    ScriptRuntimeAccess a(view);
    if (a.Code() != BML_OK)
        return a.Code();
    out = a.Context().GetCKContext();
    return out ? BML_OK : BML_ERROR_UNAVAILABLE;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int BorrowRenderContext(const ScriptModContextView &view, CKRenderContext *&out) try {
    out = nullptr;
    ScriptRuntimeAccess a(view);
    if (a.Code() != BML_OK)
        return a.Code();
    out = a.Context().GetRenderContext();
    return out ? BML_OK : BML_ERROR_UNAVAILABLE;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int ReferenceObject(const ScriptModContextView &view, CKObject *object, BML_ObjectRef &out) try {
    ScriptRuntimeAccess a(view);
    return a.Code() == BML_OK ? Publish(Engine::ReferenceObject(Native(a), object), out) : a.Code();
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int BorrowObject(const ScriptModContextView &view, const BML_ObjectRef &object, int type,
                 CKObject *&out) try {
    out = nullptr;
    ScriptRuntimeAccess a(view);
    if (a.Code() != BML_OK)
        return a.Code();
    const auto *api = Engine::Detail::Api();
    void *value = nullptr;
    const int code =
        api ? api->BorrowObject(a.Handle(), object, type, &value) : BML_ERROR_UNAVAILABLE;
    if (code == BML_OK)
        out = static_cast<CKObject *>(value);
    return code;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int BorrowAttributeManager(const ScriptModContextView &view, CKAttributeManager *&out) try {
    out = nullptr;
    ScriptRuntimeAccess a(view);
    if (a.Code() != BML_OK)
        return a.Code();
    auto *context = a.Context().GetCKContext();
    out = context ? context->GetAttributeManager() : nullptr;
    return out ? BML_OK : BML_ERROR_UNAVAILABLE;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int BorrowBehaviorManager(const ScriptModContextView &view, CKBehaviorManager *&out) try {
    out = nullptr;
    ScriptRuntimeAccess a(view);
    if (a.Code() != BML_OK)
        return a.Code();
    auto *context = a.Context().GetCKContext();
    out = context ? context->GetBehaviorManager() : nullptr;
    return out ? BML_OK : BML_ERROR_UNAVAILABLE;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int BorrowCollisionManager(const ScriptModContextView &view, CKCollisionManager *&out) try {
    out = nullptr;
    ScriptRuntimeAccess a(view);
    if (a.Code() != BML_OK)
        return a.Code();
    auto *context = a.Context().GetCKContext();
    out = context
              ? static_cast<CKCollisionManager *>(context->GetManagerByGuid(COLLISION_MANAGER_GUID))
              : nullptr;
    return out ? BML_OK : BML_ERROR_UNAVAILABLE;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int BorrowMessageManager(const ScriptModContextView &view, CKMessageManager *&out) try {
    out = nullptr;
    ScriptRuntimeAccess a(view);
    if (a.Code() != BML_OK)
        return a.Code();
    auto *context = a.Context().GetCKContext();
    out = context ? context->GetMessageManager() : nullptr;
    return out ? BML_OK : BML_ERROR_UNAVAILABLE;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int BorrowPathManager(const ScriptModContextView &view, CKPathManager *&out) try {
    out = nullptr;
    ScriptRuntimeAccess a(view);
    if (a.Code() != BML_OK)
        return a.Code();
    auto *context = a.Context().GetCKContext();
    out = context ? context->GetPathManager() : nullptr;
    return out ? BML_OK : BML_ERROR_UNAVAILABLE;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int BorrowParameterManager(const ScriptModContextView &view, CKParameterManager *&out) try {
    out = nullptr;
    ScriptRuntimeAccess a(view);
    if (a.Code() != BML_OK)
        return a.Code();
    auto *context = a.Context().GetCKContext();
    out = context ? context->GetParameterManager() : nullptr;
    return out ? BML_OK : BML_ERROR_UNAVAILABLE;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int BorrowRenderManager(const ScriptModContextView &view, CKRenderManager *&out) try {
    out = nullptr;
    ScriptRuntimeAccess a(view);
    if (a.Code() != BML_OK)
        return a.Code();
    auto *context = a.Context().GetCKContext();
    out = context ? context->GetRenderManager() : nullptr;
    return out ? BML_OK : BML_ERROR_UNAVAILABLE;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int BorrowSoundManager(const ScriptModContextView &view, CKSoundManager *&out) try {
    out = nullptr;
    ScriptRuntimeAccess a(view);
    if (a.Code() != BML_OK)
        return a.Code();
    auto *context = a.Context().GetCKContext();
    out = context ? static_cast<CKSoundManager *>(context->GetManagerByGuid(SOUND_MANAGER_GUID))
                  : nullptr;
    return out ? BML_OK : BML_ERROR_UNAVAILABLE;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int BorrowTimeManager(const ScriptModContextView &view, CKTimeManager *&out) try {
    out = nullptr;
    ScriptRuntimeAccess a(view);
    if (a.Code() != BML_OK)
        return a.Code();
    auto *context = a.Context().GetCKContext();
    out = context ? context->GetTimeManager() : nullptr;
    return out ? BML_OK : BML_ERROR_UNAVAILABLE;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int RegisterBallType(const ScriptModContextView &view, const Gameplay::BallTypeDefinition &d) try {
    ScriptRuntimeAccess a(view);
    if (a.Code() != BML_OK)
        return a.Code();
    if (!a.Owner().IsInLoadCallback())
        return BML_ERROR_ACCESS_DENIED;
    if (!BML::Detail::IsText(d.BallFile) || !BML::Detail::IsText(d.BallId) ||
        !BML::Detail::IsText(d.BallName) || !BML::Detail::IsText(d.ObjectName) ||
        !BML::Detail::IsText(d.CollisionGroup) || !std::isfinite(d.Friction) ||
        !std::isfinite(d.Elasticity) || !std::isfinite(d.Mass) || !std::isfinite(d.LinearDamp) ||
        !std::isfinite(d.RotDamp) || !std::isfinite(d.Force) || !std::isfinite(d.Radius))
        return BML_ERROR_INVALID_PARAMETER;
    return a.Owner().RegisterScriptBallType(d.BallFile, d.BallId, d.BallName, d.ObjectName,
                                            d.Friction, d.Elasticity, d.Mass, d.CollisionGroup,
                                            d.LinearDamp, d.RotDamp, d.Force, d.Radius)
               ? BML_OK
               : BML_ERROR_INVALID_PARAMETER;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int RegisterFloorType(const ScriptModContextView &view,
                      const Gameplay::FloorTypeDefinition &d) try {
    ScriptRuntimeAccess a(view);
    if (a.Code() != BML_OK)
        return a.Code();
    if (!a.Owner().IsInLoadCallback())
        return BML_ERROR_ACCESS_DENIED;
    if (!BML::Detail::IsText(d.Name) || !BML::Detail::IsText(d.CollisionGroup) ||
        !std::isfinite(d.Friction) || !std::isfinite(d.Elasticity) || !std::isfinite(d.Mass))
        return BML_ERROR_INVALID_PARAMETER;
    return a.Owner().RegisterScriptFloorType(d.Name, d.Friction, d.Elasticity, d.Mass,
                                             d.CollisionGroup, d.EnableCollision)
               ? BML_OK
               : BML_ERROR_INVALID_PARAMETER;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int RegisterModuleBall(const ScriptModContextView &view,
                       const Gameplay::ModuleBallDefinition &d) try {
    ScriptRuntimeAccess a(view);
    if (a.Code() != BML_OK)
        return a.Code();
    if (!a.Owner().IsInLoadCallback())
        return BML_ERROR_ACCESS_DENIED;
    if (!BML::Detail::IsText(d.Name) || !BML::Detail::IsText(d.CollisionGroup) ||
        !std::isfinite(d.Friction) || !std::isfinite(d.Elasticity) || !std::isfinite(d.Mass) ||
        !std::isfinite(d.LinearDamp) || !std::isfinite(d.RotDamp) || !std::isfinite(d.Radius))
        return BML_ERROR_INVALID_PARAMETER;
    return a.Owner().RegisterScriptModulBall(d.Name, d.Fixed, d.Friction, d.Elasticity, d.Mass,
                                             d.CollisionGroup, d.StartFrozen, d.EnableCollision,
                                             d.CalcMassCenter, d.LinearDamp, d.RotDamp, d.Radius)
               ? BML_OK
               : BML_ERROR_INVALID_PARAMETER;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int RegisterModuleConvex(const ScriptModContextView &view,
                         const Gameplay::ModuleConvexDefinition &d) try {
    ScriptRuntimeAccess a(view);
    if (a.Code() != BML_OK)
        return a.Code();
    if (!a.Owner().IsInLoadCallback())
        return BML_ERROR_ACCESS_DENIED;
    if (!BML::Detail::IsText(d.Name) || !BML::Detail::IsText(d.CollisionGroup) ||
        !std::isfinite(d.Friction) || !std::isfinite(d.Elasticity) || !std::isfinite(d.Mass) ||
        !std::isfinite(d.LinearDamp) || !std::isfinite(d.RotDamp))
        return BML_ERROR_INVALID_PARAMETER;
    return a.Owner().RegisterScriptModulConvex(d.Name, d.Fixed, d.Friction, d.Elasticity, d.Mass,
                                               d.CollisionGroup, d.StartFrozen, d.EnableCollision,
                                               d.CalcMassCenter, d.LinearDamp, d.RotDamp)
               ? BML_OK
               : BML_ERROR_INVALID_PARAMETER;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int RegisterTrafo(const ScriptModContextView &view, const Gameplay::TrafoDefinition &d) try {
    ScriptRuntimeAccess a(view);
    if (a.Code() != BML_OK)
        return a.Code();
    if (!a.Owner().IsInLoadCallback())
        return BML_ERROR_ACCESS_DENIED;
    if (!BML::Detail::IsText(d.Name))
        return BML_ERROR_INVALID_PARAMETER;
    return a.Owner().RegisterScriptTrafo(d.Name) ? BML_OK : BML_ERROR_INVALID_PARAMETER;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
int RegisterModule(const ScriptModContextView &view, const Gameplay::ModuleDefinition &d) try {
    ScriptRuntimeAccess a(view);
    if (a.Code() != BML_OK)
        return a.Code();
    if (!a.Owner().IsInLoadCallback())
        return BML_ERROR_ACCESS_DENIED;
    if (!BML::Detail::IsText(d.Name))
        return BML_ERROR_INVALID_PARAMETER;
    return a.Owner().RegisterScriptModul(d.Name) ? BML_OK : BML_ERROR_INVALID_PARAMETER;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
template <class T> void Construct(asIScriptGeneric *gen) {
    ScriptStringInterop::InvokeGenericBinding("value constructor",
                                              [&] { new (gen->GetObject()) T(); });
}
template <class T> void Copy(asIScriptGeneric *gen) {
    ScriptStringInterop::InvokeGenericBinding("value copy", [&] {
        new (gen->GetObject()) T(*static_cast<const T *>(gen->GetArgAddress(0)));
    });
}
template <class T> void Destroy(asIScriptGeneric *gen) {
    static_cast<T *>(gen->GetObject())->~T();
}
template <class T> void Assign(asIScriptGeneric *gen) {
    ScriptStringInterop::InvokeGenericBinding("value assignment", [&] {
        auto *self = static_cast<T *>(gen->GetObject());
        *self = *static_cast<const T *>(gen->GetArgAddress(0));
        gen->SetReturnAddress(self);
    });
}
template <class T, class V, V T::*Member> V GetField(const T *self) {
    return self->*Member;
}
template <class T, class V, V T::*Member> void SetField(T *self, const V &value) {
    self->*Member = value;
}
template <class T, class V, V T::*Member> void SetScalar(T *self, V value) {
    self->*Member = value;
}
template <class T, uint32_t T::*Member> bool GetBool(const T *self) {
    return (self->*Member) != 0;
}

#define REQUIRE(Code, Name)                                                                        \
    do {                                                                                           \
        if (!Check((Code), (Name), errorMessage))                                                  \
            return asERROR;                                                                        \
    } while (false)
template <class T>
int ValueType(asIScriptEngine *engine, const char *name, const char **errorMessage) {
    REQUIRE(engine->RegisterObjectType(name, sizeof(T), asOBJ_VALUE | asGetTypeTraits<T>()), name);
    return asSUCCESS;
}
template <class T>
int ValueMethods(asIScriptEngine *engine, const char *name, const char **errorMessage) {
    REQUIRE(engine->RegisterObjectBehaviour(name, asBEHAVE_CONSTRUCT, "void f()",
                                            asFUNCTION(Construct<T>), asCALL_GENERIC),
            name);
    const std::string copy = std::string("void f(const ") + name + " &in)";
    REQUIRE(engine->RegisterObjectBehaviour(name, asBEHAVE_CONSTRUCT, copy.c_str(),
                                            asFUNCTION(Copy<T>), asCALL_GENERIC),
            name);
    REQUIRE(engine->RegisterObjectBehaviour(name, asBEHAVE_DESTRUCT, "void f()",
                                            asFUNCTION(Destroy<T>), asCALL_GENERIC),
            name);
    const std::string assign = std::string(name) + " &opAssign(const " + name + " &in)";
    REQUIRE(
        engine->RegisterObjectMethod(name, assign.c_str(), asFUNCTION(Assign<T>), asCALL_GENERIC),
        name);
    return asSUCCESS;
}
template <class T>
int RefMethods(asIScriptEngine *engine, const char *name, const char **errorMessage) {
    REQUIRE(engine->RegisterObjectBehaviour(name, asBEHAVE_ADDREF, "void f()", asMETHOD(T, AddRef),
                                            asCALL_THISCALL),
            name);
    REQUIRE(engine->RegisterObjectBehaviour(name, asBEHAVE_RELEASE, "void f()",
                                            asMETHOD(T, Release), asCALL_THISCALL),
            name);
    return asSUCCESS;
}
} // namespace

int RegisterScriptAuthorTypes(asIScriptEngine *engine, const CKAngelScriptAdapter::Api &api,
                              const char **errorMessage) try {
    g_Engine = engine;
    g_Api = &api;
    if (ValueType<Mod::Dependency>(engine, "ModDependency", errorMessage) < 0)
        return asERROR;
    if (ValueType<Mod::Definition>(engine, "ModDefinition", errorMessage) < 0)
        return asERROR;
    if (ValueType<Mod::Info>(engine, "ModInfo", errorMessage) < 0)
        return asERROR;
    if (ValueType<ConfigValue>(engine, "ConfigValue", errorMessage) < 0)
        return asERROR;
    if (ValueType<PropertyDefinition>(engine, "ConfigPropertyDefinition", errorMessage) < 0)
        return asERROR;
    if (ValueType<PropertyInfo>(engine, "ConfigPropertyInfo", errorMessage) < 0)
        return asERROR;
    if (ValueType<BML_InputKeyState>(engine, "KeyState", errorMessage) < 0)
        return asERROR;
    if (ValueType<BML_InputKeyEvent>(engine, "KeyEvent", errorMessage) < 0)
        return asERROR;
    if (ValueType<BML_InputMouseState>(engine, "MouseState", errorMessage) < 0)
        return asERROR;
    if (ValueType<BML_InputJoystickState>(engine, "JoystickState", errorMessage) < 0)
        return asERROR;
    if (ValueType<BML_InputCursorState>(engine, "CursorState", errorMessage) < 0)
        return asERROR;
    if (ValueType<ScriptTimerDefinition>(engine, "TimerDefinition", errorMessage) < 0)
        return asERROR;
    if (ValueType<ScriptTimerInfo>(engine, "TimerInfo", errorMessage) < 0)
        return asERROR;
    if (ValueType<CommandInfo>(engine, "CommandInfo", errorMessage) < 0)
        return asERROR;
    if (ValueType<ScriptCommandInvocation>(engine, "CommandInvocation", errorMessage) < 0)
        return asERROR;
    if (ValueType<ScriptCommandCompletionRequest>(engine, "CommandCompletionRequest",
                                                  errorMessage) < 0)
        return asERROR;
    if (ValueType<ScriptCommandCompletion>(engine, "CommandCompletion", errorMessage) < 0)
        return asERROR;
    REQUIRE(engine->RegisterObjectType("Config", 0, asOBJ_REF), "Config");
    REQUIRE(engine->RegisterObjectType("ConfigProperty", 0, asOBJ_REF), "ConfigProperty");
    REQUIRE(engine->RegisterObjectType("Input", 0, asOBJ_REF), "Input");
    REQUIRE(engine->RegisterObjectType("InputBlock", 0, asOBJ_REF), "InputBlock");
    REQUIRE(engine->RegisterObjectType("KeyboardState", 0, asOBJ_REF), "KeyboardState");
    REQUIRE(engine->RegisterObjectType("Timers", 0, asOBJ_REF), "Timers");
    REQUIRE(engine->RegisterObjectType("Commands", 0, asOBJ_REF), "Commands");
    REQUIRE(engine->RegisterObjectType("ModRef", 0, asOBJ_REF), "ModRef");
    REQUIRE(engine->SetDefaultNamespace("BML"), "BML");
    if (ValueType<BML_ObjectRef>(engine, "ObjectRef", errorMessage) < 0)
        return asERROR;
    REQUIRE(engine->SetDefaultNamespace("BML::Gameplay"), "BML::Gameplay");
    if (ValueType<Gameplay::State>(engine, "State", errorMessage) < 0)
        return asERROR;
    REQUIRE(engine->SetDefaultNamespace("BML::Scene"), "BML::Scene");
    if (ValueType<Scene::ObjectInfo>(engine, "ObjectInfo", errorMessage) < 0)
        return asERROR;
    REQUIRE(engine->SetDefaultNamespace("BML::Scene"), "BML::Scene");
    if (ValueType<BML_SceneEntityTransform>(engine, "EntityTransform", errorMessage) < 0)
        return asERROR;
    REQUIRE(engine->SetDefaultNamespace("BML::Time"), "BML::Time");
    if (ValueType<BML_TimeClock>(engine, "Clock", errorMessage) < 0)
        return asERROR;
    REQUIRE(engine->SetDefaultNamespace("BML::Speedrun"), "BML::Speedrun");
    if (ValueType<BML_SpeedrunTimerState>(engine, "TimerState", errorMessage) < 0)
        return asERROR;
    REQUIRE(engine->SetDefaultNamespace("BML::UI"), "BML::UI");
    if (ValueType<UI::HUDState>(engine, "HUDState", errorMessage) < 0)
        return asERROR;
    REQUIRE(engine->SetDefaultNamespace("BML"), "BML");
    REQUIRE(engine->RegisterEnum("InputReadFlags"), "InputReadFlags");
    REQUIRE(engine->RegisterEnumValue("InputReadFlags", "INPUT_RAW", BML_INPUT_RAW), "INPUT_RAW");
    return asSUCCESS;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}

int RegisterScriptAuthorMethods(asIScriptEngine *engine, const char **errorMessage) try {
    if (ValueMethods<Mod::Dependency>(engine, "ModDependency", errorMessage) < 0)
        return asERROR;
    if (ValueMethods<Mod::Definition>(engine, "ModDefinition", errorMessage) < 0)
        return asERROR;
    if (ValueMethods<Mod::Info>(engine, "ModInfo", errorMessage) < 0)
        return asERROR;
    if (ValueMethods<ConfigValue>(engine, "ConfigValue", errorMessage) < 0)
        return asERROR;
    if (ValueMethods<PropertyDefinition>(engine, "ConfigPropertyDefinition", errorMessage) < 0)
        return asERROR;
    if (ValueMethods<PropertyInfo>(engine, "ConfigPropertyInfo", errorMessage) < 0)
        return asERROR;
    if (ValueMethods<BML_InputKeyState>(engine, "KeyState", errorMessage) < 0)
        return asERROR;
    if (ValueMethods<BML_InputKeyEvent>(engine, "KeyEvent", errorMessage) < 0)
        return asERROR;
    if (ValueMethods<BML_InputMouseState>(engine, "MouseState", errorMessage) < 0)
        return asERROR;
    if (ValueMethods<BML_InputJoystickState>(engine, "JoystickState", errorMessage) < 0)
        return asERROR;
    if (ValueMethods<BML_InputCursorState>(engine, "CursorState", errorMessage) < 0)
        return asERROR;
    if (ValueMethods<ScriptTimerDefinition>(engine, "TimerDefinition", errorMessage) < 0)
        return asERROR;
    if (ValueMethods<ScriptTimerInfo>(engine, "TimerInfo", errorMessage) < 0)
        return asERROR;
    if (ValueMethods<CommandInfo>(engine, "CommandInfo", errorMessage) < 0)
        return asERROR;
    if (ValueMethods<ScriptCommandInvocation>(engine, "CommandInvocation", errorMessage) < 0)
        return asERROR;
    if (ValueMethods<ScriptCommandCompletionRequest>(engine, "CommandCompletionRequest",
                                                     errorMessage) < 0)
        return asERROR;
    if (ValueMethods<ScriptCommandCompletion>(engine, "CommandCompletion", errorMessage) < 0)
        return asERROR;
    if (RefMethods<ScriptConfigView>(engine, "Config", errorMessage) < 0)
        return asERROR;
    if (RefMethods<ConfigProperty>(engine, "ConfigProperty", errorMessage) < 0)
        return asERROR;
    if (RefMethods<ScriptInputView>(engine, "Input", errorMessage) < 0)
        return asERROR;
    if (RefMethods<InputBlock>(engine, "InputBlock", errorMessage) < 0)
        return asERROR;
    if (RefMethods<KeyboardState>(engine, "KeyboardState", errorMessage) < 0)
        return asERROR;
    if (RefMethods<ScriptTimersView>(engine, "Timers", errorMessage) < 0)
        return asERROR;
    if (RefMethods<ScriptCommandsView>(engine, "Commands", errorMessage) < 0)
        return asERROR;
    if (RefMethods<ModRef>(engine, "ModRef", errorMessage) < 0)
        return asERROR;
    REQUIRE(engine->RegisterObjectMethod(
                "ModDependency", "string get_Id() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<Mod::Dependency, std::string, &Mod::Dependency::Id>>(),
                asCALL_GENERIC),
            "ModDependency::Id");
    REQUIRE(engine->RegisterObjectMethod(
                "ModDependency", "string get_MinimumVersion() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<Mod::Dependency, std::string, &Mod::Dependency::MinimumVersion>>(),
                asCALL_GENERIC),
            "ModDependency::MinimumVersion");
    REQUIRE(engine->RegisterObjectMethod(
                "ModDependency", "bool get_Optional() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<Mod::Dependency, bool, &Mod::Dependency::Optional>>(),
                asCALL_GENERIC),
            "ModDependency::Optional");
    REQUIRE(engine->RegisterObjectMethod(
                "ModDefinition", "string get_Id() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<Mod::Definition, std::string, &Mod::Definition::Id>>(),
                asCALL_GENERIC),
            "ModDefinition::Id");
    REQUIRE(engine->RegisterObjectMethod(
                "ModDefinition", "string get_Name() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<Mod::Definition, std::string, &Mod::Definition::Name>>(),
                asCALL_GENERIC),
            "ModDefinition::Name");
    REQUIRE(engine->RegisterObjectMethod(
                "ModDefinition", "string get_Version() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<Mod::Definition, std::string, &Mod::Definition::Version>>(),
                asCALL_GENERIC),
            "ModDefinition::Version");
    REQUIRE(engine->RegisterObjectMethod(
                "ModDefinition", "string get_Author() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<Mod::Definition, std::string, &Mod::Definition::Author>>(),
                asCALL_GENERIC),
            "ModDefinition::Author");
    REQUIRE(engine->RegisterObjectMethod(
                "ModDefinition", "string get_Description() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<Mod::Definition, std::string, &Mod::Definition::Description>>(),
                asCALL_GENERIC),
            "ModDefinition::Description");
    REQUIRE(
        engine->RegisterObjectMethod(
            "ModDefinition", "string get_MinimumLoaderVersion() const",
            ScriptStringInterop::GenericObjectFirstFunctionPtr<
                &GetField<Mod::Definition, std::string, &Mod::Definition::MinimumLoaderVersion>>(),
            asCALL_GENERIC),
        "ModDefinition::MinimumLoaderVersion");
    REQUIRE(engine->RegisterObjectMethod("ModDefinition", "uint get_DependencyCount() const",
                                         BML_AS_GENERIC_OBJECT_FIRST_FUNCTION(&DependencyCount),
                                         asCALL_GENERIC),
            "ModDefinition::uint get_DependencyCount");
    REQUIRE(engine->RegisterObjectMethod(
                "ModDefinition",
                "int ReadDependency(uint index, ModDependency &out dependency) const",
                BML_AS_GENERIC_OBJECT_FIRST_FUNCTION(&DependencyAt), asCALL_GENERIC),
            "ModDefinition::int ReadDependency");
    REQUIRE(engine->RegisterObjectMethod("ModInfo", "uint64 get_Handle() const",
                                         ScriptStringInterop::GenericObjectFirstFunctionPtr<
                                             &GetField<Mod::Info, uint64_t, &Mod::Info::Handle>>(),
                                         asCALL_GENERIC),
            "ModInfo::Handle");
    REQUIRE(engine->RegisterObjectMethod("ModInfo", "ModKind get_Kind() const",
                                         ScriptStringInterop::GenericObjectFirstFunctionPtr<
                                             &GetField<Mod::Info, uint32_t, &Mod::Info::Kind>>(),
                                         asCALL_GENERIC),
            "ModInfo::Kind");
    REQUIRE(engine->RegisterObjectMethod("ModInfo", "ModState get_State() const",
                                         ScriptStringInterop::GenericObjectFirstFunctionPtr<
                                             &GetField<Mod::Info, uint32_t, &Mod::Info::State>>(),
                                         asCALL_GENERIC),
            "ModInfo::State");
    REQUIRE(engine->RegisterObjectMethod(
                "ModInfo", "ModDefinition get_Definition() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<Mod::Info, Mod::Definition, &Mod::Info::Definition>>(),
                asCALL_GENERIC),
            "ModInfo::Definition");
    REQUIRE(engine->RegisterObjectMethod(
                "ModInfo", "string get_Diagnostic() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<Mod::Info, std::string, &Mod::Info::Diagnostic>>(),
                asCALL_GENERIC),
            "ModInfo::Diagnostic");
    REQUIRE(engine->RegisterObjectBehaviour(
                "ConfigValue", asBEHAVE_CONSTRUCT, "void f(const string &in value)",
                asFUNCTION((+[](asIScriptGeneric *gen) {
                    ScriptStringInterop::InvokeGenericBinding("ConfigValue constructor", [&] {
                        bool ok = true;
                        ScriptStringInterop::GenericArg<const std::string &> arg(gen, 0, ok);
                        if (!ok)
                            throw std::invalid_argument("ConfigValue argument");
                        new (gen->GetObject()) ConfigValue(arg.Get());
                    });
                })),
                asCALL_GENERIC),
            "ConfigValue constructor");
    REQUIRE(engine->RegisterObjectBehaviour(
                "ConfigValue", asBEHAVE_CONSTRUCT, "void f(bool value)",
                asFUNCTION((+[](asIScriptGeneric *gen) {
                    ScriptStringInterop::InvokeGenericBinding("ConfigValue constructor", [&] {
                        bool ok = true;
                        ScriptStringInterop::GenericArg<bool> arg(gen, 0, ok);
                        if (!ok)
                            throw std::invalid_argument("ConfigValue argument");
                        new (gen->GetObject()) ConfigValue(arg.Get());
                    });
                })),
                asCALL_GENERIC),
            "ConfigValue constructor");
    REQUIRE(engine->RegisterObjectBehaviour(
                "ConfigValue", asBEHAVE_CONSTRUCT, "void f(int value)",
                asFUNCTION((+[](asIScriptGeneric *gen) {
                    ScriptStringInterop::InvokeGenericBinding("ConfigValue constructor", [&] {
                        bool ok = true;
                        ScriptStringInterop::GenericArg<int> arg(gen, 0, ok);
                        if (!ok)
                            throw std::invalid_argument("ConfigValue argument");
                        new (gen->GetObject()) ConfigValue(arg.Get());
                    });
                })),
                asCALL_GENERIC),
            "ConfigValue constructor");
    REQUIRE(engine->RegisterObjectBehaviour(
                "ConfigValue", asBEHAVE_CONSTRUCT, "void f(float value)",
                asFUNCTION((+[](asIScriptGeneric *gen) {
                    ScriptStringInterop::InvokeGenericBinding("ConfigValue constructor", [&] {
                        bool ok = true;
                        ScriptStringInterop::GenericArg<float> arg(gen, 0, ok);
                        if (!ok)
                            throw std::invalid_argument("ConfigValue argument");
                        new (gen->GetObject()) ConfigValue(arg.Get());
                    });
                })),
                asCALL_GENERIC),
            "ConfigValue constructor");
    REQUIRE(engine->RegisterObjectBehaviour(
                "ConfigValue", asBEHAVE_CONSTRUCT, "void f(CKKEYBOARD value)",
                asFUNCTION((+[](asIScriptGeneric *gen) {
                    ScriptStringInterop::InvokeGenericBinding("ConfigValue constructor", [&] {
                        bool ok = true;
                        ScriptStringInterop::GenericArg<CKKEYBOARD> arg(gen, 0, ok);
                        if (!ok)
                            throw std::invalid_argument("ConfigValue argument");
                        new (gen->GetObject()) ConfigValue(arg.Get());
                    });
                })),
                asCALL_GENERIC),
            "ConfigValue constructor");
    REQUIRE(engine->RegisterObjectMethod("ConfigValue", "ConfigPropertyType get_Type() const",
                                         BML_AS_GENERIC_METHOD(&ConfigValue::Type), asCALL_GENERIC),
            "ConfigValue::ConfigPropertyType get_Type");
    REQUIRE(engine->RegisterObjectMethod("ConfigValue", "int ReadString(string &out value) const",
                                         BML_AS_GENERIC_METHOD(&ConfigValue::ReadString),
                                         asCALL_GENERIC),
            "ConfigValue::int ReadString");
    REQUIRE(engine->RegisterObjectMethod("ConfigValue", "int ReadBoolean(bool &out value) const",
                                         BML_AS_GENERIC_METHOD(&ConfigValue::ReadBoolean),
                                         asCALL_GENERIC),
            "ConfigValue::int ReadBoolean");
    REQUIRE(engine->RegisterObjectMethod("ConfigValue", "int ReadInteger(int &out value) const",
                                         BML_AS_GENERIC_METHOD(&ConfigValue::ReadInteger),
                                         asCALL_GENERIC),
            "ConfigValue::int ReadInteger");
    REQUIRE(engine->RegisterObjectMethod("ConfigValue", "int ReadFloat(float &out value) const",
                                         BML_AS_GENERIC_METHOD(&ConfigValue::ReadFloat),
                                         asCALL_GENERIC),
            "ConfigValue::int ReadFloat");
    REQUIRE(engine->RegisterObjectMethod("ConfigValue", "int ReadKey(CKKEYBOARD &out value) const",
                                         BML_AS_GENERIC_METHOD(&ConfigValue::ReadKey),
                                         asCALL_GENERIC),
            "ConfigValue::int ReadKey");
    REQUIRE(engine->RegisterObjectMethod(
                "ConfigPropertyDefinition", "string get_Category() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<PropertyDefinition, std::string, &PropertyDefinition::Category>>(),
                asCALL_GENERIC),
            "ConfigPropertyDefinition::Category");
    REQUIRE(engine->RegisterObjectMethod(
                "ConfigPropertyDefinition", "void set_Category(const string &in)",
                BML_AS_STRING_FIELD_SETTER(PropertyDefinition, Category), asCALL_GENERIC),
            "ConfigPropertyDefinition::Category setter");
    REQUIRE(engine->RegisterObjectMethod(
                "ConfigPropertyDefinition", "string get_Key() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<PropertyDefinition, std::string, &PropertyDefinition::Key>>(),
                asCALL_GENERIC),
            "ConfigPropertyDefinition::Key");
    REQUIRE(engine->RegisterObjectMethod(
                "ConfigPropertyDefinition", "void set_Key(const string &in)",
                BML_AS_STRING_FIELD_SETTER(PropertyDefinition, Key), asCALL_GENERIC),
            "ConfigPropertyDefinition::Key setter");
    REQUIRE(
        engine->RegisterObjectMethod(
            "ConfigPropertyDefinition", "ConfigValue get_DefaultValue() const",
            ScriptStringInterop::GenericObjectFirstFunctionPtr<
                &GetField<PropertyDefinition, ConfigValue, &PropertyDefinition::DefaultValue>>(),
            asCALL_GENERIC),
        "ConfigPropertyDefinition::DefaultValue");
    REQUIRE(
        engine->RegisterObjectMethod(
            "ConfigPropertyDefinition", "void set_DefaultValue(const ConfigValue &in)",
            ScriptStringInterop::GenericObjectFirstFunctionPtr<
                &SetField<PropertyDefinition, ConfigValue, &PropertyDefinition::DefaultValue>>(),
            asCALL_GENERIC),
        "ConfigPropertyDefinition::DefaultValue setter");
    REQUIRE(engine->RegisterObjectMethod(
                "ConfigPropertyDefinition", "string get_Comment() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<PropertyDefinition, std::string, &PropertyDefinition::Comment>>(),
                asCALL_GENERIC),
            "ConfigPropertyDefinition::Comment");
    REQUIRE(engine->RegisterObjectMethod(
                "ConfigPropertyDefinition", "void set_Comment(const string &in)",
                BML_AS_STRING_FIELD_SETTER(PropertyDefinition, Comment), asCALL_GENERIC),
            "ConfigPropertyDefinition::Comment setter");
    REQUIRE(engine->RegisterObjectMethod(
                "ConfigPropertyDefinition", "ConfigPropertyEditor get_Editor() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<PropertyDefinition, uint32_t, &PropertyDefinition::Editor>>(),
                asCALL_GENERIC),
            "ConfigPropertyDefinition::Editor");
    REQUIRE(engine->RegisterObjectMethod(
                "ConfigPropertyDefinition", "void set_Editor(ConfigPropertyEditor)",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &SetScalar<PropertyDefinition, uint32_t, &PropertyDefinition::Editor>>(),
                asCALL_GENERIC),
            "ConfigPropertyDefinition::Editor setter");
    REQUIRE(engine->RegisterObjectMethod(
                "ConfigPropertyDefinition", "array<string>@ get_Choices() const",
                BML_AS_GENERIC_METHOD(&PropertyDefinition::GetChoices), asCALL_GENERIC),
            "ConfigPropertyDefinition::Choices");
    REQUIRE(engine->RegisterObjectMethod(
                "ConfigPropertyDefinition", "void set_Choices(array<string>@+ value)",
                BML_AS_GENERIC_METHOD(&PropertyDefinition::SetChoices), asCALL_GENERIC),
            "ConfigPropertyDefinition::Choices setter");
    REQUIRE(engine->RegisterObjectMethod(
                "ConfigPropertyInfo", "uint64 get_Handle() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<PropertyInfo, uint64_t, &PropertyInfo::Handle>>(),
                asCALL_GENERIC),
            "ConfigPropertyInfo::Handle");
    REQUIRE(engine->RegisterObjectMethod(
                "ConfigPropertyInfo", "ConfigPropertyDefinition get_Definition() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<PropertyInfo, PropertyDefinition, &PropertyInfo::Definition>>(),
                asCALL_GENERIC),
            "ConfigPropertyInfo::Definition");
    REQUIRE(engine->RegisterObjectMethod(
                "ConfigPropertyInfo", "ConfigValue get_CurrentValue() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<PropertyInfo, ConfigValue, &PropertyInfo::CurrentValue>>(),
                asCALL_GENERIC),
            "ConfigPropertyInfo::CurrentValue");
    REQUIRE(engine->RegisterObjectMethod("Config", "bool IsValid() const",
                                         BML_AS_GENERIC_METHOD(&ScriptConfigView::IsValid),
                                         asCALL_GENERIC),
            "Config::bool IsValid");
    REQUIRE(engine->RegisterObjectMethod("Config",
                                         "int DeclareProperty(const ConfigPropertyDefinition &in "
                                         "definition, ConfigProperty@ &out property) const",
                                         BML_AS_GENERIC_METHOD(&ScriptConfigView::DeclareProperty),
                                         asCALL_GENERIC),
            "Config::int DeclareProperty");
    REQUIRE(engine->RegisterObjectMethod("Config",
                                         "int FindProperty(const string &in category, const string "
                                         "&in key, ConfigProperty@ &out property) const",
                                         BML_AS_GENERIC_METHOD(&ScriptConfigView::FindProperty),
                                         asCALL_GENERIC),
            "Config::int FindProperty");
    REQUIRE(engine->RegisterObjectMethod(
                "Config", "int HasCategory(const string &in category, bool &out exists) const",
                BML_AS_GENERIC_METHOD(&ScriptConfigView::HasCategory), asCALL_GENERIC),
            "Config::int HasCategory");
    REQUIRE(
        engine->RegisterObjectMethod(
            "Config",
            "int HasKey(const string &in category, const string &in key, bool &out exists) const",
            BML_AS_GENERIC_METHOD(&ScriptConfigView::HasKey), asCALL_GENERIC),
        "Config::int HasKey");
    REQUIRE(engine->RegisterObjectMethod(
                "Config",
                "int SetCategoryComment(const string &in category, const string &in comment) const",
                BML_AS_GENERIC_METHOD(&ScriptConfigView::SetCategoryComment), asCALL_GENERIC),
            "Config::int SetCategoryComment");
    REQUIRE(engine->RegisterObjectMethod("ConfigProperty", "bool IsValid() const",
                                         BML_AS_GENERIC_METHOD(&ConfigProperty::IsValid),
                                         asCALL_GENERIC),
            "ConfigProperty::bool IsValid");
    REQUIRE(engine->RegisterObjectMethod(
                "ConfigProperty", "int Read(ConfigPropertyInfo &out info) const",
                BML_AS_GENERIC_METHOD(&ConfigProperty::Read), asCALL_GENERIC),
            "ConfigProperty::int Read");
    REQUIRE(
        engine->RegisterObjectMethod("ConfigProperty", "int Set(const ConfigValue &in value) const",
                                     BML_AS_GENERIC_METHOD(&ConfigProperty::Set), asCALL_GENERIC),
        "ConfigProperty::int Set");
    REQUIRE(engine->RegisterObjectMethod(
                "ConfigProperty", "int SetDefault(const ConfigValue &in value) const",
                BML_AS_GENERIC_METHOD(&ConfigProperty::SetDefault), asCALL_GENERIC),
            "ConfigProperty::int SetDefault");
    REQUIRE(engine->RegisterObjectMethod(
                "ConfigProperty", "int SetComment(const string &in comment) const",
                BML_AS_GENERIC_METHOD(&ConfigProperty::SetComment), asCALL_GENERIC),
            "ConfigProperty::int SetComment");
    REQUIRE(engine->RegisterObjectMethod(
                "ConfigProperty", "int SetEditor(ConfigPropertyEditor editor) const",
                BML_AS_GENERIC_METHOD(&ConfigProperty::SetEditor), asCALL_GENERIC),
            "ConfigProperty::int SetEditor");
    REQUIRE(engine->RegisterObjectMethod(
                "ConfigProperty", "int SetChoices(const array<string> &in choices) const",
                BML_AS_GENERIC_METHOD(&ConfigProperty::SetChoices), asCALL_GENERIC),
            "ConfigProperty::int SetChoices");
    REQUIRE(engine->RegisterObjectMethod(
                "ConfigProperty", "string GetString(const string &in fallback = \"\") const",
                BML_AS_GENERIC_METHOD(&ConfigProperty::GetString), asCALL_GENERIC),
            "ConfigProperty::string GetString");
    REQUIRE(engine->RegisterObjectMethod(
                "ConfigProperty", "bool GetBoolean(bool fallback = false) const",
                BML_AS_GENERIC_METHOD(&ConfigProperty::GetBoolean), asCALL_GENERIC),
            "ConfigProperty::bool GetBoolean");
    REQUIRE(engine->RegisterObjectMethod("ConfigProperty", "int GetInteger(int fallback = 0) const",
                                         BML_AS_GENERIC_METHOD(&ConfigProperty::GetInteger),
                                         asCALL_GENERIC),
            "ConfigProperty::int GetInteger");
    REQUIRE(engine->RegisterObjectMethod(
                "ConfigProperty", "float GetFloat(float fallback = 0.0f) const",
                BML_AS_GENERIC_METHOD(&ConfigProperty::GetFloat), asCALL_GENERIC),
            "ConfigProperty::float GetFloat");
    REQUIRE(engine->RegisterObjectMethod(
                "ConfigProperty", "CKKEYBOARD GetKey(CKKEYBOARD fallback = 0) const",
                BML_AS_GENERIC_METHOD(&ConfigProperty::GetKey), asCALL_GENERIC),
            "ConfigProperty::CKKEYBOARD GetKey");
    REQUIRE(engine->RegisterObjectMethod("Input", "bool IsValid() const",
                                         BML_AS_GENERIC_METHOD(&ScriptInputView::IsValid),
                                         asCALL_GENERIC),
            "Input::bool IsValid");
    REQUIRE(engine->RegisterObjectMethod(
                "Input", "int ReadKeyboard(KeyboardState@ &out state, uint flags = 0) const",
                BML_AS_GENERIC_METHOD(&ScriptInputView::ReadKeyboard), asCALL_GENERIC),
            "Input::int ReadKeyboard");
    REQUIRE(engine->RegisterObjectMethod(
                "Input", "int ReadMouse(MouseState &out state, uint flags = 0) const",
                BML_AS_GENERIC_METHOD(&ScriptInputView::ReadMouse), asCALL_GENERIC),
            "Input::int ReadMouse");
    REQUIRE(engine->RegisterObjectMethod(
                "Input",
                "int ReadJoystick(uint index, JoystickState &out state, uint flags = 0) const",
                BML_AS_GENERIC_METHOD(&ScriptInputView::ReadJoystick), asCALL_GENERIC),
            "Input::int ReadJoystick");
    REQUIRE(engine->RegisterObjectMethod(
                "Input", "int GetKeyName(uint key, string &out name) const",
                BML_AS_GENERIC_METHOD(&ScriptInputView::GetKeyName), asCALL_GENERIC),
            "Input::int GetKeyName");
    REQUIRE(engine->RegisterObjectMethod(
                "Input", "int GetKeyFromName(const string &in name, uint &out key) const",
                BML_AS_GENERIC_METHOD(&ScriptInputView::GetKeyFromName), asCALL_GENERIC),
            "Input::int GetKeyFromName");
    REQUIRE(engine->RegisterObjectMethod(
                "Input", "int EnableKeyboardRepetition(bool enable = true) const",
                BML_AS_GENERIC_METHOD(&ScriptInputView::EnableKeyboardRepetition), asCALL_GENERIC),
            "Input::int EnableKeyboardRepetition");
    REQUIRE(engine->RegisterObjectMethod(
                "Input", "int AcquireBlock(uint mask, InputBlock@ &out block) const",
                BML_AS_GENERIC_METHOD(&ScriptInputView::AcquireBlock), asCALL_GENERIC),
            "Input::int AcquireBlock");
    REQUIRE(engine->RegisterObjectMethod(
                "Input", "int ReadBlockCount(InputDevice device, uint &out count) const",
                BML_AS_GENERIC_METHOD(&ScriptInputView::ReadBlockCount), asCALL_GENERIC),
            "Input::int ReadBlockCount");
    REQUIRE(engine->RegisterObjectMethod("Input", "int Pause(bool pause) const",
                                         BML_AS_GENERIC_METHOD(&ScriptInputView::Pause),
                                         asCALL_GENERIC),
            "Input::int Pause");
    REQUIRE(engine->RegisterObjectMethod("Input", "int ReadCursor(CursorState &out state) const",
                                         BML_AS_GENERIC_METHOD(&ScriptInputView::ReadCursor),
                                         asCALL_GENERIC),
            "Input::int ReadCursor");
    REQUIRE(engine->RegisterObjectMethod("Input", "int ShowCursor(bool visible) const",
                                         BML_AS_GENERIC_METHOD(&ScriptInputView::ShowCursor),
                                         asCALL_GENERIC),
            "Input::int ShowCursor");
    REQUIRE(engine->RegisterObjectMethod("Input", "int SetSystemCursor(CursorPointer shape) const",
                                         BML_AS_GENERIC_METHOD(&ScriptInputView::SetSystemCursor),
                                         asCALL_GENERIC),
            "Input::int SetSystemCursor");
    REQUIRE(engine->RegisterObjectMethod("InputBlock", "bool IsValid() const",
                                         BML_AS_GENERIC_METHOD(&InputBlock::IsValid),
                                         asCALL_GENERIC),
            "InputBlock::bool IsValid");
    REQUIRE(engine->RegisterObjectMethod("InputBlock", "int Release()",
                                         BML_AS_GENERIC_METHOD(&InputBlock::ReleaseBlock),
                                         asCALL_GENERIC),
            "InputBlock::int Release");
    REQUIRE(engine->RegisterObjectMethod("KeyboardState", "bool IsValid() const",
                                         BML_AS_GENERIC_METHOD(&KeyboardState::IsValid),
                                         asCALL_GENERIC),
            "KeyboardState::bool IsValid");
    REQUIRE(engine->RegisterObjectMethod("KeyboardState", "bool get_Attached() const",
                                         BML_AS_GENERIC_METHOD(&KeyboardState::Attached),
                                         asCALL_GENERIC),
            "KeyboardState::bool get_Attached");
    REQUIRE(engine->RegisterObjectMethod("KeyboardState", "bool get_RepetitionEnabled() const",
                                         BML_AS_GENERIC_METHOD(&KeyboardState::RepetitionEnabled),
                                         asCALL_GENERIC),
            "KeyboardState::bool get_RepetitionEnabled");
    REQUIRE(engine->RegisterObjectMethod("KeyboardState", "uint get_KeyCount() const",
                                         BML_AS_GENERIC_METHOD(&KeyboardState::KeyCount),
                                         asCALL_GENERIC),
            "KeyboardState::uint get_KeyCount");
    REQUIRE(engine->RegisterObjectMethod("KeyboardState", "uint get_EventCount() const",
                                         BML_AS_GENERIC_METHOD(&KeyboardState::EventCount),
                                         asCALL_GENERIC),
            "KeyboardState::uint get_EventCount");
    REQUIRE(engine->RegisterObjectMethod(
                "KeyboardState", "int ReadKey(uint key, KeyState &out state) const",
                BML_AS_GENERIC_METHOD(&KeyboardState::ReadKey), asCALL_GENERIC),
            "KeyboardState::int ReadKey");
    REQUIRE(engine->RegisterObjectMethod(
                "KeyboardState", "int ReadEvent(uint index, KeyEvent &out event) const",
                BML_AS_GENERIC_METHOD(&KeyboardState::ReadEvent), asCALL_GENERIC),
            "KeyboardState::int ReadEvent");
    REQUIRE(engine->RegisterObjectMethod("KeyboardState", "bool IsKeyDown(uint key) const",
                                         BML_AS_GENERIC_METHOD(&KeyboardState::IsKeyDown),
                                         asCALL_GENERIC),
            "KeyboardState::bool IsKeyDown");
    REQUIRE(engine->RegisterObjectMethod("KeyboardState", "bool IsKeyUp(uint key) const",
                                         BML_AS_GENERIC_METHOD(&KeyboardState::IsKeyUp),
                                         asCALL_GENERIC),
            "KeyboardState::bool IsKeyUp");
    REQUIRE(engine->RegisterObjectMethod("KeyboardState", "bool IsKeyPressed(uint key) const",
                                         BML_AS_GENERIC_METHOD(&KeyboardState::IsKeyPressed),
                                         asCALL_GENERIC),
            "KeyboardState::bool IsKeyPressed");
    REQUIRE(engine->RegisterObjectMethod("KeyboardState", "bool IsKeyReleased(uint key) const",
                                         BML_AS_GENERIC_METHOD(&KeyboardState::IsKeyReleased),
                                         asCALL_GENERIC),
            "KeyboardState::bool IsKeyReleased");
    REQUIRE(engine->RegisterObjectMethod("KeyboardState", "bool IsKeyToggled(uint key) const",
                                         BML_AS_GENERIC_METHOD(&KeyboardState::IsKeyToggled),
                                         asCALL_GENERIC),
            "KeyboardState::bool IsKeyToggled");
    REQUIRE(engine->RegisterObjectMethod("Timers", "bool IsValid() const",
                                         BML_AS_GENERIC_METHOD(&ScriptTimersView::IsValid),
                                         asCALL_GENERIC),
            "Timers::bool IsValid");
    REQUIRE(engine->RegisterObjectMethod(
                "Timers",
                "int Register(const TimerDefinition &in definition, TimerLoopCallback@+ callback, "
                "TimerRef@ &out timer) const",
                BML_AS_GENERIC_METHOD(&ScriptTimersView::Register), asCALL_GENERIC),
            "Timers::int Register");
    REQUIRE(engine->RegisterObjectMethod(
                "Timers",
                "int SetTimeout(double delayMs, TimerCallback@+ callback, TimerRef@ &out timer, "
                "const string &in name = \"\") const",
                BML_AS_GENERIC_METHOD(&ScriptTimersView::SetTimeout), asCALL_GENERIC),
            "Timers::int SetTimeout");
    REQUIRE(engine->RegisterObjectMethod(
                "Timers",
                "int SetTimeoutTicks(uint64 ticks, TimerCallback@+ callback, TimerRef@ &out timer, "
                "const string &in name = \"\") const",
                BML_AS_GENERIC_METHOD(&ScriptTimersView::SetTimeoutTicks), asCALL_GENERIC),
            "Timers::int SetTimeoutTicks");
    REQUIRE(engine->RegisterObjectMethod(
                "Timers",
                "int SetInterval(double delayMs, TimerLoopCallback@+ callback, TimerRef@ &out "
                "timer, const string &in name = \"\") const",
                BML_AS_GENERIC_METHOD(&ScriptTimersView::SetInterval), asCALL_GENERIC),
            "Timers::int SetInterval");
    REQUIRE(engine->RegisterObjectMethod(
                "Timers",
                "int SetIntervalTicks(uint64 ticks, TimerLoopCallback@+ callback, TimerRef@ &out "
                "timer, const string &in name = \"\") const",
                BML_AS_GENERIC_METHOD(&ScriptTimersView::SetIntervalTicks), asCALL_GENERIC),
            "Timers::int SetIntervalTicks");
    REQUIRE(engine->RegisterObjectMethod("Commands", "bool IsValid() const",
                                         BML_AS_GENERIC_METHOD(&ScriptCommandsView::IsValid),
                                         asCALL_GENERIC),
            "Commands::bool IsValid");
    REQUIRE(engine->RegisterObjectMethod(
                "Commands",
                "int Register(const CommandDefinition &in definition, CommandExecute@+ execute, "
                "CommandRef@ &out command, CommandComplete@+ complete = null) const",
                BML_AS_GENERIC_METHOD(&ScriptCommandsView::Register), asCALL_GENERIC),
            "Commands::int Register");
    REQUIRE(engine->RegisterObjectMethod(
                "Commands", "int Find(const string &in name, CommandInfo &out info) const",
                BML_AS_GENERIC_METHOD(&ScriptCommandsView::Find), asCALL_GENERIC),
            "Commands::int Find");
    REQUIRE(engine->RegisterObjectMethod(
                "Commands", "int ExecuteLine(const string &in line, int &out commandStatus) const",
                BML_AS_GENERIC_METHOD(&ScriptCommandsView::ExecuteLine), asCALL_GENERIC),
            "Commands::int ExecuteLine");
    REQUIRE(engine->RegisterObjectMethod("ModRef", "bool IsValid() const",
                                         BML_AS_GENERIC_METHOD(&ModRef::IsValid), asCALL_GENERIC),
            "ModRef::bool IsValid");
    REQUIRE(engine->RegisterObjectMethod("ModRef", "int Read(ModInfo &out info) const",
                                         BML_AS_GENERIC_METHOD(&ModRef::Read), asCALL_GENERIC),
            "ModRef::int Read");
    REQUIRE(engine->RegisterObjectMethod(
                "ModRef", "int CheckDependencies(bool &out satisfied) const",
                BML_AS_GENERIC_METHOD(&ModRef::CheckDependencies), asCALL_GENERIC),
            "ModRef::int CheckDependencies");
    REQUIRE(engine->RegisterObjectMethod("TimerRef", "bool IsValid() const",
                                         BML_AS_GENERIC_METHOD(&ScriptTimerRef::IsValid),
                                         asCALL_GENERIC),
            "TimerRef::bool IsValid");
    REQUIRE(engine->RegisterObjectMethod("TimerRef", "int Read(TimerInfo &out info) const",
                                         BML_AS_GENERIC_METHOD(&ScriptTimerRef::Read),
                                         asCALL_GENERIC),
            "TimerRef::int Read");
    REQUIRE(engine->RegisterObjectMethod("TimerRef", "int Pause()",
                                         BML_AS_GENERIC_METHOD(&ScriptTimerRef::Pause),
                                         asCALL_GENERIC),
            "TimerRef::int Pause");
    REQUIRE(engine->RegisterObjectMethod("TimerRef", "int Resume()",
                                         BML_AS_GENERIC_METHOD(&ScriptTimerRef::Resume),
                                         asCALL_GENERIC),
            "TimerRef::int Resume");
    REQUIRE(engine->RegisterObjectMethod("TimerRef", "int Cancel()",
                                         BML_AS_GENERIC_METHOD(&ScriptTimerRef::Cancel),
                                         asCALL_GENERIC),
            "TimerRef::int Cancel");
    REQUIRE(engine->RegisterObjectMethod("TimerEvent", "int Read(TimerInfo &out info) const",
                                         BML_AS_GENERIC_METHOD(&ScriptTimerEventView::Read),
                                         asCALL_GENERIC),
            "TimerEvent::int Read");
    REQUIRE(engine->RegisterObjectMethod("CommandRef", "bool IsValid() const",
                                         BML_AS_GENERIC_METHOD(&ScriptCommandRef::IsValid),
                                         asCALL_GENERIC),
            "CommandRef::bool IsValid");
    REQUIRE(engine->RegisterObjectMethod("CommandRef", "int Read(CommandInfo &out info) const",
                                         BML_AS_GENERIC_METHOD(&ScriptCommandRef::Read),
                                         asCALL_GENERIC),
            "CommandRef::int Read");
    REQUIRE(engine->RegisterObjectMethod("CommandRef", "int SetEnabled(bool enabled)",
                                         BML_AS_GENERIC_METHOD(&ScriptCommandRef::SetEnabled),
                                         asCALL_GENERIC),
            "CommandRef::int SetEnabled");
    REQUIRE(engine->RegisterObjectMethod("CommandRef", "int Unregister()",
                                         BML_AS_GENERIC_METHOD(&ScriptCommandRef::Unregister),
                                         asCALL_GENERIC),
            "CommandRef::int Unregister");
    REQUIRE(engine->RegisterObjectMethod("CommandInvocation", "string get_Name() const",
                                         BML_AS_GENERIC_METHOD(&ScriptCommandInvocation::GetName),
                                         asCALL_GENERIC),
            "CommandInvocation::string get_Name");
    REQUIRE(engine->RegisterObjectMethod(
                "CommandInvocation", "string get_InvokedAs() const",
                BML_AS_GENERIC_METHOD(&ScriptCommandInvocation::GetInvokedAs), asCALL_GENERIC),
            "CommandInvocation::string get_InvokedAs");
    REQUIRE(engine->RegisterObjectMethod(
                "CommandInvocation", "uint get_ArgumentCount() const",
                BML_AS_GENERIC_METHOD(&ScriptCommandInvocation::GetArgumentCount), asCALL_GENERIC),
            "CommandInvocation::uint get_ArgumentCount");
    REQUIRE(engine->RegisterObjectMethod(
                "CommandInvocation", "int ReadArgument(uint index, string &out argument) const",
                BML_AS_GENERIC_METHOD(&ScriptCommandInvocation::ReadArgument), asCALL_GENERIC),
            "CommandInvocation::int ReadArgument");
    REQUIRE(engine->RegisterObjectMethod("CommandInvocation", "bool HasInput() const",
                                         BML_AS_GENERIC_METHOD(&ScriptCommandInvocation::HasInput),
                                         asCALL_GENERIC),
            "CommandInvocation::bool HasInput");
    REQUIRE(engine->RegisterObjectMethod("CommandInvocation", "string get_Input() const",
                                         BML_AS_GENERIC_METHOD(&ScriptCommandInvocation::GetInput),
                                         asCALL_GENERIC),
            "CommandInvocation::string get_Input");
    REQUIRE(engine->RegisterObjectMethod(
                "CommandInvocation", "int Write(const string &in text) const",
                BML_AS_GENERIC_METHOD(&ScriptCommandInvocation::Write), asCALL_GENERIC),
            "CommandInvocation::int Write");
    REQUIRE(engine->RegisterObjectMethod(
                "CommandCompletionRequest", "string get_Name() const",
                BML_AS_GENERIC_METHOD(&ScriptCommandCompletionRequest::GetName), asCALL_GENERIC),
            "CommandCompletionRequest::string get_Name");
    REQUIRE(engine->RegisterObjectMethod(
                "CommandCompletionRequest", "string get_InvokedAs() const",
                BML_AS_GENERIC_METHOD(&ScriptCommandCompletionRequest::GetInvokedAs),
                asCALL_GENERIC),
            "CommandCompletionRequest::string get_InvokedAs");
    REQUIRE(engine->RegisterObjectMethod(
                "CommandCompletionRequest", "uint get_ArgumentCount() const",
                BML_AS_GENERIC_METHOD(&ScriptCommandCompletionRequest::GetArgumentCount),
                asCALL_GENERIC),
            "CommandCompletionRequest::uint get_ArgumentCount");
    REQUIRE(
        engine->RegisterObjectMethod(
            "CommandCompletionRequest", "int ReadArgument(uint index, string &out argument) const",
            BML_AS_GENERIC_METHOD(&ScriptCommandCompletionRequest::ReadArgument), asCALL_GENERIC),
        "CommandCompletionRequest::int ReadArgument");
    REQUIRE(engine->RegisterObjectMethod(
                "CommandCompletionRequest", "uint get_ActiveArgument() const",
                BML_AS_GENERIC_METHOD(&ScriptCommandCompletionRequest::GetActiveArgument),
                asCALL_GENERIC),
            "CommandCompletionRequest::uint get_ActiveArgument");
    REQUIRE(engine->RegisterObjectMethod(
                "CommandCompletionRequest", "string get_Prefix() const",
                BML_AS_GENERIC_METHOD(&ScriptCommandCompletionRequest::GetPrefix), asCALL_GENERIC),
            "CommandCompletionRequest::string get_Prefix");
    REQUIRE(engine->RegisterObjectMethod(
                "CommandCompletion", "int Add(const string &in candidate) const",
                BML_AS_GENERIC_METHOD(&ScriptCommandCompletion::Add), asCALL_GENERIC),
            "CommandCompletion::int Add");
    REQUIRE(engine->RegisterObjectMethod(
                "CommandCompletion", "int ReadCount(uint &out count) const",
                BML_AS_GENERIC_METHOD(&ScriptCommandCompletion::ReadCount), asCALL_GENERIC),
            "CommandCompletion::int ReadCount");
    REQUIRE(engine->RegisterObjectMethod(
                "CommandCompletion", "int ReadCandidate(uint index, string &out candidate) const",
                BML_AS_GENERIC_METHOD(&ScriptCommandCompletion::ReadCandidate), asCALL_GENERIC),
            "CommandCompletion::int ReadCandidate");
    REQUIRE(engine->RegisterObjectMethod(
                "KeyState", "uint get_Key() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<BML_InputKeyState, uint32_t, &BML_InputKeyState::Key>>(),
                asCALL_GENERIC),
            "KeyState::Key");
    REQUIRE(
        engine->RegisterObjectMethod("KeyState", "bool get_Down() const",
                                     ScriptStringInterop::GenericObjectFirstFunctionPtr<
                                         &GetBool<BML_InputKeyState, &BML_InputKeyState::Down>>(),
                                     asCALL_GENERIC),
        "KeyState::Down");
    REQUIRE(engine->RegisterObjectMethod("KeyState", "bool get_Up() const",
                                         ScriptStringInterop::GenericObjectFirstFunctionPtr<
                                             &GetBool<BML_InputKeyState, &BML_InputKeyState::Up>>(),
                                         asCALL_GENERIC),
            "KeyState::Up");
    REQUIRE(engine->RegisterObjectMethod(
                "KeyState", "bool get_Toggled() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetBool<BML_InputKeyState, &BML_InputKeyState::Toggled>>(),
                asCALL_GENERIC),
            "KeyState::Toggled");
    REQUIRE(engine->RegisterObjectMethod(
                "KeyState", "bool get_Pressed() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetBool<BML_InputKeyState, &BML_InputKeyState::Pressed>>(),
                asCALL_GENERIC),
            "KeyState::Pressed");
    REQUIRE(engine->RegisterObjectMethod(
                "KeyState", "bool get_Released() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetBool<BML_InputKeyState, &BML_InputKeyState::Released>>(),
                asCALL_GENERIC),
            "KeyState::Released");
    REQUIRE(engine->RegisterObjectMethod(
                "KeyState", "uint get_DownTimestampMs() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<BML_InputKeyState, uint32_t, &BML_InputKeyState::DownTimestampMs>>(),
                asCALL_GENERIC),
            "KeyState::DownTimestampMs");
    REQUIRE(
        engine->RegisterObjectMethod(
            "KeyState", "uint get_ToggledTimestampMs() const",
            ScriptStringInterop::GenericObjectFirstFunctionPtr<
                &GetField<BML_InputKeyState, uint32_t, &BML_InputKeyState::ToggledTimestampMs>>(),
            asCALL_GENERIC),
        "KeyState::ToggledTimestampMs");
    REQUIRE(engine->RegisterObjectMethod(
                "KeyEvent", "uint get_Key() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<BML_InputKeyEvent, uint32_t, &BML_InputKeyEvent::Key>>(),
                asCALL_GENERIC),
            "KeyEvent::Key");
    REQUIRE(engine->RegisterObjectMethod(
                "KeyEvent", "InputKeyEvent get_State() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<BML_InputKeyEvent, uint32_t, &BML_InputKeyEvent::State>>(),
                asCALL_GENERIC),
            "KeyEvent::State");
    REQUIRE(engine->RegisterObjectMethod(
                "KeyEvent", "uint get_TimestampMs() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<BML_InputKeyEvent, uint32_t, &BML_InputKeyEvent::TimestampMs>>(),
                asCALL_GENERIC),
            "KeyEvent::TimestampMs");
    REQUIRE(engine->RegisterObjectMethod(
                "MouseState", "bool get_Attached() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetBool<BML_InputMouseState, &BML_InputMouseState::Attached>>(),
                asCALL_GENERIC),
            "MouseState::Attached");
    REQUIRE(
        engine->RegisterObjectMethod(
            "MouseState", "Vec2 get_ScreenPosition() const",
            ScriptStringInterop::GenericObjectFirstFunctionPtr<
                &GetField<BML_InputMouseState, BML_Vec2, &BML_InputMouseState::ScreenPosition>>(),
            asCALL_GENERIC),
        "MouseState::ScreenPosition");
    REQUIRE(
        engine->RegisterObjectMethod(
            "MouseState", "Vec2 get_WindowPosition() const",
            ScriptStringInterop::GenericObjectFirstFunctionPtr<
                &GetField<BML_InputMouseState, BML_Vec2, &BML_InputMouseState::WindowPosition>>(),
            asCALL_GENERIC),
        "MouseState::WindowPosition");
    REQUIRE(engine->RegisterObjectMethod(
                "MouseState", "Vec2 get_LastPosition() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<BML_InputMouseState, BML_Vec2, &BML_InputMouseState::LastPosition>>(),
                asCALL_GENERIC),
            "MouseState::LastPosition");
    REQUIRE(
        engine->RegisterObjectMethod(
            "MouseState", "Vec3 get_RelativePosition() const",
            ScriptStringInterop::GenericObjectFirstFunctionPtr<
                &GetField<BML_InputMouseState, BML_Vec3, &BML_InputMouseState::RelativePosition>>(),
            asCALL_GENERIC),
        "MouseState::RelativePosition");
    REQUIRE(engine->RegisterObjectMethod(
                "MouseState", "uint get_DownMask() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<BML_InputMouseState, uint32_t, &BML_InputMouseState::DownMask>>(),
                asCALL_GENERIC),
            "MouseState::DownMask");
    REQUIRE(engine->RegisterObjectMethod(
                "MouseState", "uint get_ClickedMask() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<BML_InputMouseState, uint32_t, &BML_InputMouseState::ClickedMask>>(),
                asCALL_GENERIC),
            "MouseState::ClickedMask");
    REQUIRE(engine->RegisterObjectMethod(
                "MouseState", "uint get_ToggledMask() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<BML_InputMouseState, uint32_t, &BML_InputMouseState::ToggledMask>>(),
                asCALL_GENERIC),
            "MouseState::ToggledMask");
    REQUIRE(engine->RegisterObjectMethod(
                "JoystickState", "bool get_Attached() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetBool<BML_InputJoystickState, &BML_InputJoystickState::Attached>>(),
                asCALL_GENERIC),
            "JoystickState::Attached");
    REQUIRE(
        engine->RegisterObjectMethod(
            "JoystickState", "Vec3 get_Position() const",
            ScriptStringInterop::GenericObjectFirstFunctionPtr<
                &GetField<BML_InputJoystickState, BML_Vec3, &BML_InputJoystickState::Position>>(),
            asCALL_GENERIC),
        "JoystickState::Position");
    REQUIRE(
        engine->RegisterObjectMethod(
            "JoystickState", "Vec3 get_Rotation() const",
            ScriptStringInterop::GenericObjectFirstFunctionPtr<
                &GetField<BML_InputJoystickState, BML_Vec3, &BML_InputJoystickState::Rotation>>(),
            asCALL_GENERIC),
        "JoystickState::Rotation");
    REQUIRE(
        engine->RegisterObjectMethod(
            "JoystickState", "Vec2 get_Sliders() const",
            ScriptStringInterop::GenericObjectFirstFunctionPtr<
                &GetField<BML_InputJoystickState, BML_Vec2, &BML_InputJoystickState::Sliders>>(),
            asCALL_GENERIC),
        "JoystickState::Sliders");
    REQUIRE(engine->RegisterObjectMethod(
                "JoystickState", "float get_PointOfViewAngle() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<&GetField<
                    BML_InputJoystickState, float, &BML_InputJoystickState::PointOfViewAngle>>(),
                asCALL_GENERIC),
            "JoystickState::PointOfViewAngle");
    REQUIRE(
        engine->RegisterObjectMethod(
            "JoystickState", "uint get_Buttons() const",
            ScriptStringInterop::GenericObjectFirstFunctionPtr<
                &GetField<BML_InputJoystickState, uint32_t, &BML_InputJoystickState::Buttons>>(),
            asCALL_GENERIC),
        "JoystickState::Buttons");
    REQUIRE(engine->RegisterObjectMethod(
                "CursorState", "bool get_Visible() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetBool<BML_InputCursorState, &BML_InputCursorState::Visible>>(),
                asCALL_GENERIC),
            "CursorState::Visible");
    REQUIRE(engine->RegisterObjectMethod(
                "CursorState", "CursorPointer get_Shape() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<BML_InputCursorState, uint32_t, &BML_InputCursorState::Shape>>(),
                asCALL_GENERIC),
            "CursorState::Shape");
    REQUIRE(engine->RegisterObjectMethod(
                "TimerDefinition", "string get_Name() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<ScriptTimerDefinition, std::string, &ScriptTimerDefinition::Name>>(),
                asCALL_GENERIC),
            "TimerDefinition::Name");
    REQUIRE(engine->RegisterObjectMethod("TimerDefinition", "void set_Name(const string &in)",
                                         BML_AS_STRING_FIELD_SETTER(ScriptTimerDefinition, Name),
                                         asCALL_GENERIC),
            "TimerDefinition::Name setter");
    REQUIRE(engine->RegisterObjectMethod(
                "TimerDefinition", "TimerType get_Type() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<ScriptTimerDefinition, uint32_t, &ScriptTimerDefinition::Type>>(),
                asCALL_GENERIC),
            "TimerDefinition::Type");
    REQUIRE(engine->RegisterObjectMethod(
                "TimerDefinition", "void set_Type(TimerType)",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &SetScalar<ScriptTimerDefinition, uint32_t, &ScriptTimerDefinition::Type>>(),
                asCALL_GENERIC),
            "TimerDefinition::Type setter");
    REQUIRE(engine->RegisterObjectMethod(
                "TimerDefinition", "TimerTimeBase get_TimeBase() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<ScriptTimerDefinition, uint32_t, &ScriptTimerDefinition::TimeBase>>(),
                asCALL_GENERIC),
            "TimerDefinition::TimeBase");
    REQUIRE(
        engine->RegisterObjectMethod(
            "TimerDefinition", "void set_TimeBase(TimerTimeBase)",
            ScriptStringInterop::GenericObjectFirstFunctionPtr<
                &SetScalar<ScriptTimerDefinition, uint32_t, &ScriptTimerDefinition::TimeBase>>(),
            asCALL_GENERIC),
        "TimerDefinition::TimeBase setter");
    REQUIRE(engine->RegisterObjectMethod(
                "TimerDefinition", "bool get_StartPaused() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<ScriptTimerDefinition, bool, &ScriptTimerDefinition::StartPaused>>(),
                asCALL_GENERIC),
            "TimerDefinition::StartPaused");
    REQUIRE(engine->RegisterObjectMethod(
                "TimerDefinition", "void set_StartPaused(bool)",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &SetScalar<ScriptTimerDefinition, bool, &ScriptTimerDefinition::StartPaused>>(),
                asCALL_GENERIC),
            "TimerDefinition::StartPaused setter");
    REQUIRE(engine->RegisterObjectMethod(
                "TimerDefinition", "int get_Priority() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<ScriptTimerDefinition, int, &ScriptTimerDefinition::Priority>>(),
                asCALL_GENERIC),
            "TimerDefinition::Priority");
    REQUIRE(engine->RegisterObjectMethod(
                "TimerDefinition", "void set_Priority(int)",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &SetScalar<ScriptTimerDefinition, int, &ScriptTimerDefinition::Priority>>(),
                asCALL_GENERIC),
            "TimerDefinition::Priority setter");
    REQUIRE(engine->RegisterObjectMethod(
                "TimerDefinition", "double get_DelayMs() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<ScriptTimerDefinition, double, &ScriptTimerDefinition::DelayMs>>(),
                asCALL_GENERIC),
            "TimerDefinition::DelayMs");
    REQUIRE(engine->RegisterObjectMethod(
                "TimerDefinition", "void set_DelayMs(double)",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &SetScalar<ScriptTimerDefinition, double, &ScriptTimerDefinition::DelayMs>>(),
                asCALL_GENERIC),
            "TimerDefinition::DelayMs setter");
    REQUIRE(
        engine->RegisterObjectMethod(
            "TimerDefinition", "uint64 get_DelayTicks() const",
            ScriptStringInterop::GenericObjectFirstFunctionPtr<
                &GetField<ScriptTimerDefinition, uint64_t, &ScriptTimerDefinition::DelayTicks>>(),
            asCALL_GENERIC),
        "TimerDefinition::DelayTicks");
    REQUIRE(
        engine->RegisterObjectMethod(
            "TimerDefinition", "void set_DelayTicks(uint64)",
            ScriptStringInterop::GenericObjectFirstFunctionPtr<
                &SetScalar<ScriptTimerDefinition, uint64_t, &ScriptTimerDefinition::DelayTicks>>(),
            asCALL_GENERIC),
        "TimerDefinition::DelayTicks setter");
    REQUIRE(
        engine->RegisterObjectMethod(
            "TimerDefinition", "uint64 get_RepeatCount() const",
            ScriptStringInterop::GenericObjectFirstFunctionPtr<
                &GetField<ScriptTimerDefinition, uint64_t, &ScriptTimerDefinition::RepeatCount>>(),
            asCALL_GENERIC),
        "TimerDefinition::RepeatCount");
    REQUIRE(
        engine->RegisterObjectMethod(
            "TimerDefinition", "void set_RepeatCount(uint64)",
            ScriptStringInterop::GenericObjectFirstFunctionPtr<
                &SetScalar<ScriptTimerDefinition, uint64_t, &ScriptTimerDefinition::RepeatCount>>(),
            asCALL_GENERIC),
        "TimerDefinition::RepeatCount setter");
    REQUIRE(engine->RegisterObjectMethod(
                "TimerInfo", "uint64 get_Handle() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<ScriptTimerInfo, uint64_t, &ScriptTimerInfo::Handle>>(),
                asCALL_GENERIC),
            "TimerInfo::Handle");
    REQUIRE(engine->RegisterObjectMethod(
                "TimerInfo", "string get_Name() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<ScriptTimerInfo, std::string, &ScriptTimerInfo::Name>>(),
                asCALL_GENERIC),
            "TimerInfo::Name");
    REQUIRE(engine->RegisterObjectMethod(
                "TimerInfo", "TimerState get_State() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<ScriptTimerInfo, uint32_t, &ScriptTimerInfo::State>>(),
                asCALL_GENERIC),
            "TimerInfo::State");
    REQUIRE(engine->RegisterObjectMethod(
                "TimerInfo", "TimerType get_Type() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<ScriptTimerInfo, uint32_t, &ScriptTimerInfo::Type>>(),
                asCALL_GENERIC),
            "TimerInfo::Type");
    REQUIRE(engine->RegisterObjectMethod(
                "TimerInfo", "TimerTimeBase get_TimeBase() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<ScriptTimerInfo, uint32_t, &ScriptTimerInfo::TimeBase>>(),
                asCALL_GENERIC),
            "TimerInfo::TimeBase");
    REQUIRE(engine->RegisterObjectMethod(
                "TimerInfo", "uint64 get_CompletedIterations() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<ScriptTimerInfo, uint64_t, &ScriptTimerInfo::CompletedIterations>>(),
                asCALL_GENERIC),
            "TimerInfo::CompletedIterations");
    REQUIRE(engine->RegisterObjectMethod(
                "TimerInfo", "int64 get_RemainingIterations() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<ScriptTimerInfo, int64_t, &ScriptTimerInfo::RemainingIterations>>(),
                asCALL_GENERIC),
            "TimerInfo::RemainingIterations");
    REQUIRE(engine->RegisterObjectMethod(
                "TimerInfo", "float get_Progress() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<ScriptTimerInfo, float, &ScriptTimerInfo::Progress>>(),
                asCALL_GENERIC),
            "TimerInfo::Progress");
    REQUIRE(
        engine->RegisterObjectMethod("CommandInfo", "uint64 get_Handle() const",
                                     ScriptStringInterop::GenericObjectFirstFunctionPtr<
                                         &GetField<CommandInfo, uint64_t, &CommandInfo::Handle>>(),
                                     asCALL_GENERIC),
        "CommandInfo::Handle");
    REQUIRE(
        engine->RegisterObjectMethod("CommandInfo", "string get_Name() const",
                                     ScriptStringInterop::GenericObjectFirstFunctionPtr<
                                         &GetField<CommandInfo, std::string, &CommandInfo::Name>>(),
                                     asCALL_GENERIC),
        "CommandInfo::Name");
    REQUIRE(engine->RegisterObjectMethod(
                "CommandInfo", "string get_Alias() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<CommandInfo, std::string, &CommandInfo::Alias>>(),
                asCALL_GENERIC),
            "CommandInfo::Alias");
    REQUIRE(engine->RegisterObjectMethod(
                "CommandInfo", "string get_Description() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<CommandInfo, std::string, &CommandInfo::Description>>(),
                asCALL_GENERIC),
            "CommandInfo::Description");
    REQUIRE(engine->RegisterObjectMethod(
                "CommandInfo", "string get_Usage() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<CommandInfo, std::string, &CommandInfo::Usage>>(),
                asCALL_GENERIC),
            "CommandInfo::Usage");
    REQUIRE(engine->RegisterObjectMethod(
                "CommandInfo", "string get_Category() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<CommandInfo, std::string, &CommandInfo::Category>>(),
                asCALL_GENERIC),
            "CommandInfo::Category");
    REQUIRE(engine->RegisterObjectMethod("CommandInfo", "bool get_Cheat() const",
                                         ScriptStringInterop::GenericObjectFirstFunctionPtr<
                                             &GetField<CommandInfo, bool, &CommandInfo::Cheat>>(),
                                         asCALL_GENERIC),
            "CommandInfo::Cheat");
    REQUIRE(engine->RegisterObjectMethod("CommandInfo", "bool get_Hidden() const",
                                         ScriptStringInterop::GenericObjectFirstFunctionPtr<
                                             &GetField<CommandInfo, bool, &CommandInfo::Hidden>>(),
                                         asCALL_GENERIC),
            "CommandInfo::Hidden");
    REQUIRE(engine->RegisterObjectMethod("CommandInfo", "bool get_Enabled() const",
                                         ScriptStringInterop::GenericObjectFirstFunctionPtr<
                                             &GetField<CommandInfo, bool, &CommandInfo::Enabled>>(),
                                         asCALL_GENERIC),
            "CommandInfo::Enabled");
    REQUIRE(engine->RegisterObjectMethod(
                "ModContext", "Config@ Config() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &ContextService<ScriptConfigView, &ScriptRuntimeScope::Config>>(),
                asCALL_GENERIC),
            "ModContext::Config@ Config");
    REQUIRE(engine->RegisterObjectMethod(
                "ModContext", "Input@ Input() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &ContextService<ScriptInputView, &ScriptRuntimeScope::Input>>(),
                asCALL_GENERIC),
            "ModContext::Input@ Input");
    REQUIRE(engine->RegisterObjectMethod(
                "ModContext", "Timers@ Timers() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &ContextService<ScriptTimersView, &ScriptRuntimeScope::Timers>>(),
                asCALL_GENERIC),
            "ModContext::Timers@ Timers");
    REQUIRE(engine->RegisterObjectMethod(
                "ModContext", "Commands@ Commands() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &ContextService<ScriptCommandsView, &ScriptRuntimeScope::Commands>>(),
                asCALL_GENERIC),
            "ModContext::Commands@ Commands");
    REQUIRE(engine->RegisterObjectMethod("ModContext", "bool IsValid() const",
                                         BML_AS_GENERIC_OBJECT_FIRST_FUNCTION(&ContextValid),
                                         asCALL_GENERIC),
            "ModContext::bool IsValid");
    REQUIRE(engine->RegisterObjectMethod("ModContext", "int Owner(ModRef@ &out mod) const",
                                         BML_AS_GENERIC_OBJECT_FIRST_FUNCTION(&ContextOwner),
                                         asCALL_GENERIC),
            "ModContext::int Owner");
    REQUIRE(engine->RegisterObjectMethod(
                "ModContext", "int FindMod(const string &in id, ModRef@ &out mod) const",
                BML_AS_GENERIC_OBJECT_FIRST_FUNCTION(&ContextFindMod), asCALL_GENERIC),
            "ModContext::int FindMod");
    REQUIRE(engine->RegisterObjectMethod(
                "ModContext", "int LogInfo(const string &in message) const",
                BML_AS_GENERIC_OBJECT_FIRST_FUNCTION(&ContextLogInfo), asCALL_GENERIC),
            "ModContext::int LogInfo");
    REQUIRE(engine->RegisterObjectMethod(
                "ModContext", "int LogWarn(const string &in message) const",
                BML_AS_GENERIC_OBJECT_FIRST_FUNCTION(&ContextLogWarn), asCALL_GENERIC),
            "ModContext::int LogWarn");
    REQUIRE(engine->RegisterObjectMethod(
                "ModContext", "int LogError(const string &in message) const",
                BML_AS_GENERIC_OBJECT_FIRST_FUNCTION(&ContextLogError), asCALL_GENERIC),
            "ModContext::int LogError");
    REQUIRE(engine->RegisterObjectMethod(
                "ModContext", "int GetDirectory(DirectoryType directory, string &out path) const",
                BML_AS_GENERIC_OBJECT_FIRST_FUNCTION(&ContextDirectory), asCALL_GENERIC),
            "ModContext::int GetDirectory");
    REQUIRE(engine->RegisterObjectMethod(
                "ModContext",
                "int GetResourcePath(const string &in path, string &out absolutePath) const",
                BML_AS_GENERIC_OBJECT_FIRST_FUNCTION(&ContextResourcePath), asCALL_GENERIC),
            "ModContext::int GetResourcePath");
    REQUIRE(engine->RegisterObjectMethod(
                "ModContext",
                "int ReadResource(const string &in path, array<uint8> &out bytes) const",
                BML_AS_GENERIC_OBJECT_FIRST_FUNCTION(&ReadResource), asCALL_GENERIC),
            "ModContext::int ReadResource");
    REQUIRE(engine->RegisterObjectMethod(
                "ModContext", "int ResourceExists(const string &in path, bool &out exists) const",
                BML_AS_GENERIC_OBJECT_FIRST_FUNCTION(&ContextResourceExists), asCALL_GENERIC),
            "ModContext::int ResourceExists");
    REQUIRE(engine->RegisterObjectMethod(
                "ModContext",
                "int RegisterMenuPage(const MenuPageDefinition &in definition, MenuPageDraw@+ "
                "draw, MenuPageRef@ &out page, MenuPageEnter@+ enter = null, MenuPageLeave@+ leave "
                "= null) const",
                BML_AS_GENERIC_OBJECT_FIRST_FUNCTION(&RegisterMenuPage), asCALL_GENERIC),
            "ModContext::int RegisterMenuPage");
    REQUIRE(engine->RegisterObjectMethod("ConfigEvent", "ConfigProperty@ BorrowProperty() const",
                                         BML_AS_GENERIC_OBJECT_FIRST_FUNCTION(&EventProperty),
                                         asCALL_GENERIC),
            "ConfigEvent::ConfigProperty@ BorrowProperty");
    REQUIRE(engine->RegisterObjectMethod("ModContext", "int ReadMods(? &out mods) const",
                                         asFUNCTION(ReadMods), asCALL_GENERIC),
            "ModContext::ReadMods");
    REQUIRE(engine->RegisterObjectMethod("Commands", "int ReadCommands(? &out commands) const",
                                         asFUNCTION(ReadCommands), asCALL_GENERIC),
            "Commands::ReadCommands");
    REQUIRE(engine->SetDefaultNamespace("BML"), "BML");
    if (ValueMethods<BML_ObjectRef>(engine, "ObjectRef", errorMessage) < 0)
        return asERROR;
    REQUIRE(engine->SetDefaultNamespace("BML::Gameplay"), "BML::Gameplay");
    if (ValueMethods<Gameplay::State>(engine, "State", errorMessage) < 0)
        return asERROR;
    REQUIRE(engine->SetDefaultNamespace("BML::Scene"), "BML::Scene");
    if (ValueMethods<Scene::ObjectInfo>(engine, "ObjectInfo", errorMessage) < 0)
        return asERROR;
    REQUIRE(engine->SetDefaultNamespace("BML::Scene"), "BML::Scene");
    if (ValueMethods<BML_SceneEntityTransform>(engine, "EntityTransform", errorMessage) < 0)
        return asERROR;
    REQUIRE(engine->SetDefaultNamespace("BML::Time"), "BML::Time");
    if (ValueMethods<BML_TimeClock>(engine, "Clock", errorMessage) < 0)
        return asERROR;
    REQUIRE(engine->SetDefaultNamespace("BML::Speedrun"), "BML::Speedrun");
    if (ValueMethods<BML_SpeedrunTimerState>(engine, "TimerState", errorMessage) < 0)
        return asERROR;
    REQUIRE(engine->SetDefaultNamespace("BML::UI"), "BML::UI");
    if (ValueMethods<UI::HUDState>(engine, "HUDState", errorMessage) < 0)
        return asERROR;
    REQUIRE(engine->SetDefaultNamespace("BML"), "BML");
    REQUIRE(engine->RegisterObjectMethod(
                "ObjectRef", "uint get_Domain() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<BML_ObjectRef, uint32_t, &BML_ObjectRef::Domain>>(),
                asCALL_GENERIC),
            "ObjectRef::Domain");
    REQUIRE(engine->RegisterObjectMethod(
                "ObjectRef", "uint get_Slot() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<BML_ObjectRef, uint32_t, &BML_ObjectRef::Slot>>(),
                asCALL_GENERIC),
            "ObjectRef::Slot");
    REQUIRE(engine->RegisterObjectMethod(
                "ObjectRef", "uint get_Generation() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<BML_ObjectRef, uint32_t, &BML_ObjectRef::Generation>>(),
                asCALL_GENERIC),
            "ObjectRef::Generation");
    REQUIRE(engine->SetDefaultNamespace("BML::Gameplay"), "BML::Gameplay");
    REQUIRE(engine->RegisterObjectMethod("State", "bool get_Ingame() const",
                                         ScriptStringInterop::GenericObjectFirstFunctionPtr<
                                             &GetBool<Gameplay::State, &Gameplay::State::Ingame>>(),
                                         asCALL_GENERIC),
            "State::Ingame");
    REQUIRE(engine->RegisterObjectMethod("State", "bool get_Paused() const",
                                         ScriptStringInterop::GenericObjectFirstFunctionPtr<
                                             &GetBool<Gameplay::State, &Gameplay::State::Paused>>(),
                                         asCALL_GENERIC),
            "State::Paused");
    REQUIRE(
        engine->RegisterObjectMethod("State", "bool get_Playing() const",
                                     ScriptStringInterop::GenericObjectFirstFunctionPtr<
                                         &GetBool<Gameplay::State, &Gameplay::State::Playing>>(),
                                     asCALL_GENERIC),
        "State::Playing");
    REQUIRE(engine->SetDefaultNamespace("BML::Scene"), "BML::Scene");
    REQUIRE(engine->RegisterObjectMethod(
                "ObjectInfo", "int get_Id() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<Scene::ObjectInfo, int, &Scene::ObjectInfo::Id>>(),
                asCALL_GENERIC),
            "ObjectInfo::Id");
    REQUIRE(engine->RegisterObjectMethod(
                "ObjectInfo", "int get_ClassId() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<Scene::ObjectInfo, int, &Scene::ObjectInfo::ClassId>>(),
                asCALL_GENERIC),
            "ObjectInfo::ClassId");
    REQUIRE(engine->RegisterObjectMethod(
                "ObjectInfo", "string get_Name() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<Scene::ObjectInfo, std::string, &Scene::ObjectInfo::Name>>(),
                asCALL_GENERIC),
            "ObjectInfo::Name");
    REQUIRE(engine->RegisterObjectMethod(
                "ObjectInfo", "bool get_Visible() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<Scene::ObjectInfo, bool, &Scene::ObjectInfo::Visible>>(),
                asCALL_GENERIC),
            "ObjectInfo::Visible");
    REQUIRE(engine->RegisterObjectMethod(
                "ObjectInfo", "bool get_Dynamic() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<Scene::ObjectInfo, bool, &Scene::ObjectInfo::Dynamic>>(),
                asCALL_GENERIC),
            "ObjectInfo::Dynamic");
    REQUIRE(engine->RegisterObjectMethod(
                "ObjectInfo", "bool get_NameTruncated() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<Scene::ObjectInfo, bool, &Scene::ObjectInfo::NameTruncated>>(),
                asCALL_GENERIC),
            "ObjectInfo::NameTruncated");
    REQUIRE(engine->SetDefaultNamespace("BML::Scene"), "BML::Scene");
    REQUIRE(engine->RegisterObjectMethod(
                "EntityTransform", "BML::Vec3 get_Position() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<&GetField<
                    BML_SceneEntityTransform, BML_Vec3, &BML_SceneEntityTransform::Position>>(),
                asCALL_GENERIC),
            "EntityTransform::Position");
    REQUIRE(
        engine->RegisterObjectMethod(
            "EntityTransform", "BML::Vec3 get_Scale() const",
            ScriptStringInterop::GenericObjectFirstFunctionPtr<
                &GetField<BML_SceneEntityTransform, BML_Vec3, &BML_SceneEntityTransform::Scale>>(),
            asCALL_GENERIC),
        "EntityTransform::Scale");
    REQUIRE(engine->RegisterObjectMethod(
                "EntityTransform", "BML::ObjectRef get_Parent() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<&GetField<
                    BML_SceneEntityTransform, BML_ObjectRef, &BML_SceneEntityTransform::Parent>>(),
                asCALL_GENERIC),
            "EntityTransform::Parent");
    REQUIRE(
        engine->RegisterObjectMethod(
            "EntityTransform", "int get_ChildCount() const",
            ScriptStringInterop::GenericObjectFirstFunctionPtr<
                &GetField<BML_SceneEntityTransform, int, &BML_SceneEntityTransform::ChildCount>>(),
            asCALL_GENERIC),
        "EntityTransform::ChildCount");
    REQUIRE(engine->SetDefaultNamespace("BML::Time"), "BML::Time");
    REQUIRE(
        engine->RegisterObjectMethod("Clock", "float get_TimeMs() const",
                                     ScriptStringInterop::GenericObjectFirstFunctionPtr<
                                         &GetField<BML_TimeClock, float, &BML_TimeClock::TimeMs>>(),
                                     asCALL_GENERIC),
        "Clock::TimeMs");
    REQUIRE(engine->RegisterObjectMethod(
                "Clock", "float get_AbsoluteMs() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<BML_TimeClock, float, &BML_TimeClock::AbsoluteMs>>(),
                asCALL_GENERIC),
            "Clock::AbsoluteMs");
    REQUIRE(engine->RegisterObjectMethod(
                "Clock", "float get_DeltaMs() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<BML_TimeClock, float, &BML_TimeClock::DeltaMs>>(),
                asCALL_GENERIC),
            "Clock::DeltaMs");
    REQUIRE(engine->RegisterObjectMethod(
                "Clock", "uint get_MainTickCount() const",
                ScriptStringInterop::GenericObjectFirstFunctionPtr<
                    &GetField<BML_TimeClock, uint32_t, &BML_TimeClock::MainTickCount>>(),
                asCALL_GENERIC),
            "Clock::MainTickCount");
    REQUIRE(engine->SetDefaultNamespace("BML::Speedrun"), "BML::Speedrun");
    REQUIRE(
        engine->RegisterObjectMethod(
            "TimerState", "float get_ElapsedTime() const",
            ScriptStringInterop::GenericObjectFirstFunctionPtr<
                &GetField<BML_SpeedrunTimerState, float, &BML_SpeedrunTimerState::ElapsedTime>>(),
            asCALL_GENERIC),
        "TimerState::ElapsedTime");
    REQUIRE(engine->SetDefaultNamespace("BML::UI"), "BML::UI");
    REQUIRE(engine->RegisterObjectMethod("HUDState", "int get_Mode() const",
                                         ScriptStringInterop::GenericObjectFirstFunctionPtr<
                                             &GetField<UI::HUDState, int, &UI::HUDState::Mode>>(),
                                         asCALL_GENERIC),
            "HUDState::Mode");
    REQUIRE(engine->SetDefaultNamespace("BML"), "BML");
    REQUIRE(engine->RegisterObjectMethod("ObjectRef", "bool get_IsNull() const",
                                         BML_AS_GENERIC_OBJECT_FIRST_FUNCTION(&ObjectIsNull),
                                         asCALL_GENERIC),
            "ObjectRef::IsNull");
    REQUIRE(engine->SetDefaultNamespace("BML::Gameplay"), "BML::Gameplay");
    REQUIRE(engine->RegisterGlobalFunction("int ReadCheatEnabled(bool &out enabled)",
                                           BML_AS_GENERIC_FUNCTION(&ReadCheat), asCALL_GENERIC),
            "BML::Gameplay::ReadCheat");
    REQUIRE(engine->RegisterGlobalFunction("int ReadState(State &out state)",
                                           BML_AS_GENERIC_FUNCTION(&ReadGameState), asCALL_GENERIC),
            "BML::Gameplay::ReadGameState");
    REQUIRE(engine->RegisterGlobalFunction(
                "int EnableCheat(const BML::ModContext &in ctx, bool enabled)",
                BML_AS_GENERIC_FUNCTION(&EnableCheat), asCALL_GENERIC),
            "BML::Gameplay::EnableCheat");
    REQUIRE(engine->RegisterGlobalFunction("int ExitGame(const BML::ModContext &in ctx)",
                                           BML_AS_GENERIC_FUNCTION(&ExitGame), asCALL_GENERIC),
            "BML::Gameplay::ExitGame");
    REQUIRE(engine->RegisterGlobalFunction("int RegisterBallType(const BML::ModContext &in ctx, "
                                           "const BML::BallTypeDefinition &in definition)",
                                           BML_AS_GENERIC_FUNCTION(&RegisterBallType),
                                           asCALL_GENERIC),
            "BML::Gameplay::RegisterBallType");
    REQUIRE(engine->RegisterGlobalFunction("int RegisterFloorType(const BML::ModContext &in ctx, "
                                           "const BML::FloorTypeDefinition &in definition)",
                                           BML_AS_GENERIC_FUNCTION(&RegisterFloorType),
                                           asCALL_GENERIC),
            "BML::Gameplay::RegisterFloorType");
    REQUIRE(engine->RegisterGlobalFunction("int RegisterModuleBall(const BML::ModContext &in ctx, "
                                           "const BML::ModuleBallDefinition &in definition)",
                                           BML_AS_GENERIC_FUNCTION(&RegisterModuleBall),
                                           asCALL_GENERIC),
            "BML::Gameplay::RegisterModuleBall");
    REQUIRE(engine->RegisterGlobalFunction("int RegisterModuleConvex(const BML::ModContext &in "
                                           "ctx, const BML::ModuleConvexDefinition &in definition)",
                                           BML_AS_GENERIC_FUNCTION(&RegisterModuleConvex),
                                           asCALL_GENERIC),
            "BML::Gameplay::RegisterModuleConvex");
    REQUIRE(engine->RegisterGlobalFunction("int RegisterTrafo(const BML::ModContext &in ctx, const "
                                           "BML::TrafoDefinition &in definition)",
                                           BML_AS_GENERIC_FUNCTION(&RegisterTrafo), asCALL_GENERIC),
            "BML::Gameplay::RegisterTrafo");
    REQUIRE(engine->RegisterGlobalFunction("int RegisterModule(const BML::ModContext &in ctx, "
                                           "const BML::ModuleDefinition &in definition)",
                                           BML_AS_GENERIC_FUNCTION(&RegisterModule),
                                           asCALL_GENERIC),
            "BML::Gameplay::RegisterModule");
    REQUIRE(engine->SetDefaultNamespace("BML::Scene"), "BML::Scene");
    REQUIRE(engine->RegisterGlobalFunction(
                "int FindObject(const string &in name, BML::ObjectRef &out object)",
                BML_AS_GENERIC_FUNCTION(&FindObject), asCALL_GENERIC),
            "BML::Scene::FindObject");
    REQUIRE(engine->RegisterGlobalFunction(
                "int FindObject(const string &in name, int classId, BML::ObjectRef &out object)",
                BML_AS_GENERIC_FUNCTION(&FindObjectOfClass), asCALL_GENERIC),
            "BML::Scene::FindObjectOfClass");
    REQUIRE(engine->RegisterGlobalFunction(
                "int ReadObject(const BML::ObjectRef &in object, ObjectInfo &out info)",
                BML_AS_GENERIC_FUNCTION(&ReadSceneObject), asCALL_GENERIC),
            "BML::Scene::ReadSceneObject");
    REQUIRE(engine->RegisterGlobalFunction("int ReadEntityTransform(const BML::ObjectRef &in "
                                           "object, EntityTransform &out transform)",
                                           BML_AS_GENERIC_FUNCTION(&ReadTransform), asCALL_GENERIC),
            "BML::Scene::ReadTransform");
    REQUIRE(engine->RegisterGlobalFunction("int SetIC(const BML::ModContext &in ctx, const "
                                           "BML::ObjectRef &in object, bool hierarchy = false)",
                                           BML_AS_GENERIC_FUNCTION(&SetIC), asCALL_GENERIC),
            "BML::Scene::SetIC");
    REQUIRE(engine->RegisterGlobalFunction("int RestoreIC(const BML::ModContext &in ctx, const "
                                           "BML::ObjectRef &in object, bool hierarchy = false)",
                                           BML_AS_GENERIC_FUNCTION(&RestoreIC), asCALL_GENERIC),
            "BML::Scene::RestoreIC");
    REQUIRE(engine->RegisterGlobalFunction(
                "int Show(const BML::ModContext &in ctx, const BML::ObjectRef &in object, uint "
                "option, bool hierarchy = false)",
                BML_AS_GENERIC_FUNCTION(&Show), asCALL_GENERIC),
            "BML::Scene::Show");
    REQUIRE(engine->SetDefaultNamespace("BML::Time"), "BML::Time");
    REQUIRE(engine->RegisterGlobalFunction("int ReadClock(Clock &out clock)",
                                           BML_AS_GENERIC_FUNCTION(&ReadClock), asCALL_GENERIC),
            "BML::Time::ReadClock");
    REQUIRE(engine->SetDefaultNamespace("BML::Speedrun"), "BML::Speedrun");
    REQUIRE(engine->RegisterGlobalFunction("int ReadTimerState(TimerState &out state)",
                                           BML_AS_GENERIC_FUNCTION(&ReadSpeedrun), asCALL_GENERIC),
            "BML::Speedrun::ReadSpeedrun");
    REQUIRE(engine->SetDefaultNamespace("BML::UI"), "BML::UI");
    REQUIRE(engine->RegisterGlobalFunction("int ReadHUDState(HUDState &out state)",
                                           BML_AS_GENERIC_FUNCTION(&ReadHUD), asCALL_GENERIC),
            "BML::UI::ReadHUD");
    REQUIRE(engine->RegisterGlobalFunction("int SkipNextRender(const BML::ModContext &in ctx)",
                                           BML_AS_GENERIC_FUNCTION(&SkipNextRender),
                                           asCALL_GENERIC),
            "BML::UI::SkipNextRender");
    REQUIRE(engine->RegisterGlobalFunction(
                "int SetWidescreenAdjustment(const BML::ModContext &in ctx, bool enabled)",
                BML_AS_GENERIC_FUNCTION(&AdjustFOV), asCALL_GENERIC),
            "BML::UI::AdjustFOV");
    REQUIRE(engine->SetDefaultNamespace("BML::UI"), "BML::UI");
    REQUIRE(engine->RegisterGlobalFunction("int AddMessage(const string &in message)",
                                           BML_AS_GENERIC_FUNCTION(&AddMessage), asCALL_GENERIC),
            "UI::AddMessage");
    REQUIRE(engine->RegisterGlobalFunction("int ClearMessages()",
                                           BML_AS_GENERIC_FUNCTION(&ClearMessages), asCALL_GENERIC),
            "UI::ClearMessages");
    REQUIRE(engine->RegisterGlobalFunction("int OpenModsMenu()",
                                           BML_AS_GENERIC_FUNCTION(&OpenModsMenu), asCALL_GENERIC),
            "UI::OpenModsMenu");
    REQUIRE(engine->RegisterGlobalFunction("int CloseModsMenu()",
                                           BML_AS_GENERIC_FUNCTION(&CloseModsMenu), asCALL_GENERIC),
            "UI::CloseModsMenu");
    REQUIRE(engine->RegisterGlobalFunction("int OpenMapMenu()",
                                           BML_AS_GENERIC_FUNCTION(&OpenMapMenu), asCALL_GENERIC),
            "UI::OpenMapMenu");
    REQUIRE(engine->RegisterGlobalFunction("int CloseMapMenu()",
                                           BML_AS_GENERIC_FUNCTION(&CloseMapMenu), asCALL_GENERIC),
            "UI::CloseMapMenu");
    REQUIRE(engine->RegisterGlobalFunction("int SetHUDMode(int mode)",
                                           BML_AS_GENERIC_FUNCTION(&SetHUDMode), asCALL_GENERIC),
            "UI::SetHUDMode");
    REQUIRE(engine->RegisterGlobalFunction("int ShowTitle(bool visible)",
                                           BML_AS_GENERIC_FUNCTION(&ShowTitle), asCALL_GENERIC),
            "UI::ShowTitle");
    REQUIRE(engine->RegisterGlobalFunction("int ShowFPS(bool visible)",
                                           BML_AS_GENERIC_FUNCTION(&ShowFPS), asCALL_GENERIC),
            "UI::ShowFPS");
    REQUIRE(engine->SetDefaultNamespace("BML::Speedrun"), "BML::Speedrun");
    REQUIRE(engine->RegisterGlobalFunction("int SetTimerVisible(bool visible)",
                                           BML_AS_GENERIC_FUNCTION(&SetTimerVisible),
                                           asCALL_GENERIC),
            "Speedrun::SetTimerVisible");
    REQUIRE(engine->RegisterGlobalFunction("int StartTimer()", BML_AS_GENERIC_FUNCTION(&StartTimer),
                                           asCALL_GENERIC),
            "Speedrun::StartTimer");
    REQUIRE(engine->RegisterGlobalFunction("int PauseTimer()", BML_AS_GENERIC_FUNCTION(&PauseTimer),
                                           asCALL_GENERIC),
            "Speedrun::PauseTimer");
    REQUIRE(engine->RegisterGlobalFunction("int ResetTimer()", BML_AS_GENERIC_FUNCTION(&ResetTimer),
                                           asCALL_GENERIC),
            "Speedrun::ResetTimer");
    REQUIRE(engine->SetDefaultNamespace("BML::Engine"), "BML::Engine");
    REQUIRE(engine->RegisterGlobalFunction(
                "int BorrowContext(const BML::ModContext &in ctx, CKContext@ &out context)",
                BML_AS_GENERIC_FUNCTION(&BorrowContext), asCALL_GENERIC),
            "BML::Engine::BorrowContext");
    REQUIRE(
        engine->RegisterGlobalFunction(
            "int BorrowRenderContext(const BML::ModContext &in ctx, CKRenderContext@ &out context)",
            BML_AS_GENERIC_FUNCTION(&BorrowRenderContext), asCALL_GENERIC),
        "BML::Engine::BorrowRenderContext");
    REQUIRE(engine->RegisterGlobalFunction("int ReferenceObject(const BML::ModContext &in ctx, "
                                           "CKObject@ object, BML::ObjectRef &out reference)",
                                           BML_AS_GENERIC_FUNCTION(&ReferenceObject),
                                           asCALL_GENERIC),
            "BML::Engine::ReferenceObject");
    REQUIRE(engine->RegisterGlobalFunction(
                "int BorrowObject(const BML::ModContext &in ctx, const BML::ObjectRef &in "
                "reference, int expectedClass, CKObject@ &out object)",
                BML_AS_GENERIC_FUNCTION(&BorrowObject), asCALL_GENERIC),
            "BML::Engine::BorrowObject");
    REQUIRE(engine->RegisterGlobalFunction("int BorrowAttributeManager(const BML::ModContext &in "
                                           "ctx, CKAttributeManager@ &out manager)",
                                           BML_AS_GENERIC_FUNCTION(&BorrowAttributeManager),
                                           asCALL_GENERIC),
            "BML::Engine::BorrowAttributeManager");
    REQUIRE(engine->RegisterGlobalFunction("int BorrowBehaviorManager(const BML::ModContext &in "
                                           "ctx, CKBehaviorManager@ &out manager)",
                                           BML_AS_GENERIC_FUNCTION(&BorrowBehaviorManager),
                                           asCALL_GENERIC),
            "BML::Engine::BorrowBehaviorManager");
    REQUIRE(engine->RegisterGlobalFunction("int BorrowCollisionManager(const BML::ModContext &in "
                                           "ctx, CKCollisionManager@ &out manager)",
                                           BML_AS_GENERIC_FUNCTION(&BorrowCollisionManager),
                                           asCALL_GENERIC),
            "BML::Engine::BorrowCollisionManager");
    REQUIRE(engine->RegisterGlobalFunction("int BorrowMessageManager(const BML::ModContext &in "
                                           "ctx, CKMessageManager@ &out manager)",
                                           BML_AS_GENERIC_FUNCTION(&BorrowMessageManager),
                                           asCALL_GENERIC),
            "BML::Engine::BorrowMessageManager");
    REQUIRE(engine->RegisterGlobalFunction(
                "int BorrowPathManager(const BML::ModContext &in ctx, CKPathManager@ &out manager)",
                BML_AS_GENERIC_FUNCTION(&BorrowPathManager), asCALL_GENERIC),
            "BML::Engine::BorrowPathManager");
    REQUIRE(engine->RegisterGlobalFunction("int BorrowParameterManager(const BML::ModContext &in "
                                           "ctx, CKParameterManager@ &out manager)",
                                           BML_AS_GENERIC_FUNCTION(&BorrowParameterManager),
                                           asCALL_GENERIC),
            "BML::Engine::BorrowParameterManager");
    REQUIRE(
        engine->RegisterGlobalFunction(
            "int BorrowRenderManager(const BML::ModContext &in ctx, CKRenderManager@ &out manager)",
            BML_AS_GENERIC_FUNCTION(&BorrowRenderManager), asCALL_GENERIC),
        "BML::Engine::BorrowRenderManager");
    REQUIRE(
        engine->RegisterGlobalFunction(
            "int BorrowSoundManager(const BML::ModContext &in ctx, CKSoundManager@ &out manager)",
            BML_AS_GENERIC_FUNCTION(&BorrowSoundManager), asCALL_GENERIC),
        "BML::Engine::BorrowSoundManager");
    REQUIRE(engine->RegisterGlobalFunction(
                "int BorrowTimeManager(const BML::ModContext &in ctx, CKTimeManager@ &out manager)",
                BML_AS_GENERIC_FUNCTION(&BorrowTimeManager), asCALL_GENERIC),
            "BML::Engine::BorrowTimeManager");
    REQUIRE(engine->SetDefaultNamespace("BML"), "BML");
    return asSUCCESS;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}
#undef REQUIRE
} // namespace BML
