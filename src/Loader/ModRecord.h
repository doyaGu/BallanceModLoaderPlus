#ifndef BML_MODRECORD_H
#define BML_MODRECORD_H

#include <memory>
#include "BML/Mod.h"
#include "Loader/ModCallbacks.h"
#include "Loader/ModVersion.h"
#include "Logging/Logger.h"

class Config;
class ModContext;

namespace BML {
class ScriptMod;
}

// A loader-owned registration identity, composed by each implementation. Metadata
// borrows its implementation's storage; refresh it whenever that storage changes.
// Config belongs to ModContext. Backends and callbacks are borrowed until unregister.
class ModRecord final {
public:
    explicit ModRecord(ModContext *context = nullptr) : m_Context(context) {}

    ModRecord(const ModRecord &) = delete;
    ModRecord &operator=(const ModRecord &) = delete;

    template <class T> void Bind(T &implementation) {
        m_Implementation = &implementation;
        BindCallbacks<T>(std::make_index_sequence<std::tuple_size_v<ModCallbackTable>>{});
        RefreshMetadata(implementation);
    }

    template <class T> void RefreshMetadata(T &implementation) {
        m_Definition = {sizeof(BML_ModDefinition),
                        implementation.GetID(),
                        implementation.GetName(),
                        implementation.GetVersion(),
                        implementation.GetAuthor(),
                        implementation.GetDescription(),
                        nullptr,
                        nullptr,
                        0,
                        0};
        m_MinimumVersion = implementation.GetBMLVersion();
    }

    const char *GetID() const noexcept { return m_Definition.Id; }

    const char *GetName() const noexcept { return m_Definition.Name; }

    const char *GetVersion() const noexcept { return m_Definition.Version; }

    const char *GetAuthor() const noexcept { return m_Definition.Author; }

    const char *GetDescription() const noexcept { return m_Definition.Description; }

    BMLVersion GetBMLVersion() const noexcept { return m_MinimumVersion; }

    template <ModEvent Event> bool Subscribed() const noexcept {
        return std::get<static_cast<std::size_t>(Event)>(m_Callbacks) != nullptr;
    }

    template <ModEvent Event, class... Args> void Notify(Args &&...args) const {
        if (auto callback = std::get<static_cast<std::size_t>(Event)>(m_Callbacks))
            callback(m_Implementation, std::forward<Args>(args)...);
    }

    Logger *GetLogger();
    Config *GetConfig();
    BML::ScriptMod *Script = nullptr;

private:
    friend class ModContext;

    template <class T, std::size_t... Index> void BindCallbacks(std::index_sequence<Index...>) {
        ((std::get<Index>(m_Callbacks) =
              ModEventTraits<static_cast<ModEvent>(Index)>::template Bind<T>()),
         ...);
    }

    ModContext *m_Context;
    BML_ModDefinition m_Definition{};
    BMLVersion m_MinimumVersion;
    void *m_Implementation = nullptr;
    ModCallbackTable m_Callbacks{};
    std::unique_ptr<Logger> m_Logger;
    Config *m_Config = nullptr;
};

#endif
