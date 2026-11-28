#ifndef BML_COMMANDRECORD_H
#define BML_COMMANDRECORD_H

#include <algorithm>
#include <cfloat>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class ModContext;

namespace BML {
struct CommandInfo {
    std::string Name;
    std::string Alias;
    std::string Description;
    std::string Usage;
    std::string Category;
    bool Cheat = false;
    bool Hidden = false;
    bool Enabled = true;
    std::uint64_t Handle = 0;
};

// The registry owns this record. A borrowed implementation must outlive its
// registration; Lifetime retains callback owners through an acquired invocation.
struct CommandRecord {
    CommandInfo Info;
    void *UserData = nullptr;
    void (*ExecuteCallback)(void *, ModContext *, const std::vector<std::string> &) = nullptr;
    std::vector<std::string> (*CompleteCallback)(void *, ModContext *,
                                                 const std::vector<std::string> &) = nullptr;
    std::shared_ptr<void> Lifetime;

    void Execute(ModContext *context, const std::vector<std::string> &args) const {
        ExecuteCallback(UserData, context, args);
    }

    std::vector<std::string> Complete(ModContext *context,
                                      const std::vector<std::string> &args) const {
        return CompleteCallback ? CompleteCallback(UserData, context, args)
                                : std::vector<std::string>{};
    }
};

template <class T>
std::shared_ptr<CommandRecord> MakeCommandRecord(T *implementation, CommandInfo info,
                                                 std::shared_ptr<void> lifetime = {}) {
    if (!implementation)
        return {};
    auto record = std::make_shared<CommandRecord>();
    record->Info = std::move(info);
    record->UserData = implementation;
    record->Lifetime = std::move(lifetime);
    record->ExecuteCallback = [](void *object, ModContext *context,
                                 const std::vector<std::string> &args) {
        static_cast<T *>(object)->Execute(context, args);
    };
    if constexpr (requires(T &value, ModContext *context, const std::vector<std::string> &args) {
                      value.GetTabCompletion(context, args);
                  }) {
        record->CompleteCallback = [](void *object, ModContext *context,
                                      const std::vector<std::string> &args) {
            return static_cast<T *>(object)->GetTabCompletion(context, args);
        };
    }
    return record;
}

template <class T>
std::shared_ptr<CommandRecord> MakeCommandRecord(T *implementation,
                                                 std::shared_ptr<void> lifetime = {}) {
    if (!implementation)
        return {};
    CommandInfo info;
    info.Name = implementation->GetName();
    info.Alias = implementation->GetAlias();
    info.Description = implementation->GetDescription();
    info.Cheat = implementation->IsCheat();
    return MakeCommandRecord(implementation, std::move(info), std::move(lifetime));
}

namespace CommandArguments {
inline int ParseInteger(const std::string &text, int minimum = INT_MIN, int maximum = INT_MAX) {
    return (std::max)(minimum, (std::min)(maximum, std::atoi(text.c_str())));
}

inline float ParseFloat(const std::string &text, float minimum = -FLT_MAX,
                        float maximum = FLT_MAX) {
    return (std::max)(minimum, (std::min)(maximum, static_cast<float>(std::atof(text.c_str()))));
}

inline bool ParseBoolean(const std::string &text) {
    return text == "true" || text == "on" || text == "1";
}
} // namespace CommandArguments
} // namespace BML

#endif
