#ifndef BML_MOD_HPP
#define BML_MOD_HPP

#include "BML/Mod.h"
#include "BML/Detail/Api.hpp"

#include <memory>
#include <functional>
#include <optional>
#include <stdexcept>
#include <span>
#include <type_traits>
#include <utility>

namespace BML::Config {
class View;
}
namespace BML::Input {
class View;
}
namespace BML::Timer {
class View;
}
namespace BML::Command {
class View;
}

namespace BML::Mod {

struct Dependency {
    std::string Id, MinimumVersion;
    bool Optional = false;
};
struct Definition {
    std::string Id, Name, Version, Author, Description, MinimumLoaderVersion;
    std::vector<Dependency> Dependencies;
};
struct Info {
    BML_ModHandle Handle = 0;
    uint32_t Kind = 0, State = 0;
    BML::Mod::Definition Definition;
    std::string Diagnostic;
};

namespace Detail {
struct ContextAccess;
const BML_ModContextInterface *ContextApi() noexcept;
} // namespace Detail
class View;
class Context {
  public:
    Context() noexcept = default;
    [[nodiscard]] BML::Config::View Config() const noexcept;
    [[nodiscard]] BML::Input::View Input() const noexcept;
    [[nodiscard]] BML::Timer::View Timers() const noexcept;
    [[nodiscard]] BML::Command::View Commands() const noexcept;
    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] BML_ModContextHandle Handle() const noexcept { return m_Handle; }
    [[nodiscard]] Result<View> Owner() const;
    [[nodiscard]] Result<View> FindMod(std::string_view id) const;
    [[nodiscard]] Result<std::vector<Info>> ReadMods() const;
    int LogInfo(std::string_view message) const noexcept { return Log(BML_LOG_INFO, message); }
    int LogWarn(std::string_view message) const noexcept { return Log(BML_LOG_WARN, message); }
    int LogError(std::string_view message) const noexcept { return Log(BML_LOG_ERROR, message); }
    [[nodiscard]] Result<std::string> GetDirectory(uint32_t directory) const;
    [[nodiscard]] Result<std::string> GetResourcePath(std::string_view path) const;
    [[nodiscard]] Result<std::vector<uint8_t>> ReadResource(std::string_view path) const;
    [[nodiscard]] Result<bool> ResourceExists(std::string_view path) const;

  private:
    friend class View;
    friend struct Detail::ContextAccess;
    explicit Context(BML_ModContextHandle handle) noexcept
        : m_Handle(handle), m_Api(Detail::ContextApi()) {}
    int Log(uint32_t level, std::string_view message) const noexcept {
        if (!BML::Detail::IsText(message))
            return BML_ERROR_INVALID_PARAMETER;
        return m_Api ? m_Api->Log(m_Handle, level, message.data(), message.size())
                     : BML_ERROR_UNAVAILABLE;
    }
    BML_ModContextHandle m_Handle = 0;
    const BML_ModContextInterface *m_Api = nullptr;
};

class View {
  public:
    View() noexcept = default;
    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] BML_ModHandle Handle() const noexcept { return m_Handle; }
    [[nodiscard]] Result<Info> Read() const;

  private:
    friend class Context;
    View(Context context, BML_ModHandle handle) noexcept : m_Context(context), m_Handle(handle) {}
    Context m_Context;
    BML_ModHandle m_Handle = 0;
};

namespace Detail {
struct ContextAccess {
    static Context Make(BML_ModContextHandle handle) noexcept { return Context(handle); }
};
inline std::string_view Text(const char *value) noexcept { return value ? value : ""; }
inline Definition CopyDefinition(const BML_ModDefinition &value) {
    Definition result{std::string(Text(value.Id)),
                      std::string(Text(value.Name)),
                      std::string(Text(value.Version)),
                      std::string(Text(value.Author)),
                      std::string(Text(value.Description)),
                      std::string(Text(value.MinimumLoaderVersion)),
                      {}};
    result.Dependencies.reserve(value.DependencyCount);
    for (size_t i = 0; i < value.DependencyCount; ++i) {
        const auto &dep = *reinterpret_cast<const BML_ModDependency *>(
            reinterpret_cast<const unsigned char *>(value.Dependencies) +
            i * value.DependencyStride);
        result.Dependencies.push_back(
            {std::string(Text(dep.Id)), std::string(Text(dep.MinimumVersion)), dep.Optional != 0});
    }
    return result;
}
inline Info CopyInfo(const BML_ModInfo &value) {
    return {value.Handle, value.Kind, value.State, CopyDefinition(*value.Definition),
            std::string(Text(value.Diagnostic))};
}
} // namespace Detail

inline bool Context::IsValid() const noexcept {
    BML_ModHandle owner = 0;
    return m_Api && m_Api->GetOwner(m_Handle, &owner) == BML_OK;
}
inline Result<View> Context::Owner() const try {
    BML_ModHandle owner = 0;
    const int code = m_Api ? m_Api->GetOwner(m_Handle, &owner) : BML_ERROR_UNAVAILABLE;
    return code == BML_OK ? Result<View>::Success(View(*this, owner)) : Result<View>::Failure(code);
} catch (const std::bad_alloc &) {
    return Result<View>::Failure(BML_ERROR_OUT_OF_MEMORY);
} catch (...) {
    return Result<View>::Failure(BML_ERROR_FAIL);
}
inline Result<View> Context::FindMod(std::string_view id) const try {
    if (!BML::Detail::IsText(id))
        return Result<View>::Failure(BML_ERROR_INVALID_PARAMETER);
    const std::string text(id);
    BML_ModHandle found = 0;
    const int code = m_Api ? m_Api->FindMod(m_Handle, text.c_str(), &found) : BML_ERROR_UNAVAILABLE;
    return code == BML_OK ? Result<View>::Success(View(*this, found)) : Result<View>::Failure(code);
} catch (const std::bad_alloc &) {
    return Result<View>::Failure(BML_ERROR_OUT_OF_MEMORY);
} catch (...) {
    return Result<View>::Failure(BML_ERROR_FAIL);
}
inline Result<std::vector<Info>> Context::ReadMods() const try {
    std::vector<Info> values;
    const int code =
        m_Api ? m_Api->VisitMods(m_Handle,
                                 BML::Detail::Cdecl<[](void *data, const BML_ModInfo *info) -> int {
                                     return BML::Detail::Guard([&] {
                                         static_cast<std::vector<Info> *>(data)->push_back(
                                             Detail::CopyInfo(*info));
                                         return BML_OK;
                                     });
                                 }>::Call,
                                 &values)
              : BML_ERROR_UNAVAILABLE;
    return code == BML_OK ? Result<std::vector<Info>>::Success(std::move(values))
                          : Result<std::vector<Info>>::Failure(code);
} catch (const std::bad_alloc &) {
    return Result<std::vector<Info>>::Failure(BML_ERROR_OUT_OF_MEMORY);
} catch (...) {
    return Result<std::vector<Info>>::Failure(BML_ERROR_FAIL);
}
inline Result<std::string> Context::GetDirectory(uint32_t directory) const try {
    return BML::Detail::ReadBuffer<std::string>([&](char *p, size_t n, size_t *required) {
        return m_Api ? m_Api->GetDirectory(m_Handle, directory, p, n, required)
                     : BML_ERROR_UNAVAILABLE;
    });
} catch (const std::bad_alloc &) {
    return Result<std::string>::Failure(BML_ERROR_OUT_OF_MEMORY);
} catch (...) {
    return Result<std::string>::Failure(BML_ERROR_FAIL);
}
inline Result<std::string> Context::GetResourcePath(std::string_view path) const try {
    if (!BML::Detail::IsText(path))
        return Result<std::string>::Failure(BML_ERROR_INVALID_PARAMETER);
    const std::string text(path);
    return BML::Detail::ReadBuffer<std::string>([&](char *p, size_t n, size_t *required) {
        return m_Api ? m_Api->GetResourcePath(m_Handle, text.c_str(), p, n, required)
                     : BML_ERROR_UNAVAILABLE;
    });
} catch (const std::bad_alloc &) {
    return Result<std::string>::Failure(BML_ERROR_OUT_OF_MEMORY);
} catch (...) {
    return Result<std::string>::Failure(BML_ERROR_FAIL);
}
inline Result<std::vector<uint8_t>> Context::ReadResource(std::string_view path) const try {
    if (!BML::Detail::IsText(path))
        return Result<std::vector<uint8_t>>::Failure(BML_ERROR_INVALID_PARAMETER);
    const std::string text(path);
    return BML::Detail::ReadBuffer<std::vector<uint8_t>>([&](void *p, size_t n, size_t *required) {
        return m_Api ? m_Api->ReadResource(m_Handle, text.c_str(), p, n, required)
                     : BML_ERROR_UNAVAILABLE;
    });
} catch (const std::bad_alloc &) {
    return Result<std::vector<uint8_t>>::Failure(BML_ERROR_OUT_OF_MEMORY);
} catch (...) {
    return Result<std::vector<uint8_t>>::Failure(BML_ERROR_FAIL);
}
inline Result<bool> Context::ResourceExists(std::string_view path) const try {
    if (!BML::Detail::IsText(path))
        return Result<bool>::Failure(BML_ERROR_INVALID_PARAMETER);
    const std::string text(path);
    uint32_t exists = 0;
    const int code =
        m_Api ? m_Api->ResourceExists(m_Handle, text.c_str(), &exists) : BML_ERROR_UNAVAILABLE;
    return code == BML_OK ? Result<bool>::Success(exists != 0) : Result<bool>::Failure(code);
} catch (const std::bad_alloc &) {
    return Result<bool>::Failure(BML_ERROR_OUT_OF_MEMORY);
} catch (...) {
    return Result<bool>::Failure(BML_ERROR_FAIL);
}
inline bool View::IsValid() const noexcept {
    return m_Context.m_Api &&
           m_Context.m_Api->ReadMod(
               m_Context.Handle(), m_Handle,
               BML::Detail::Cdecl<[](void *, const BML_ModInfo *) -> int { return BML_OK; }>::Call,
               nullptr) == BML_OK;
}
inline Result<Info> View::Read() const try {
    Info value;
    const int code = m_Context.m_Api
                         ? m_Context.m_Api->ReadMod(
                               m_Context.Handle(), m_Handle,
                               BML::Detail::Cdecl<[](void *data, const BML_ModInfo *info) -> int {
                                   return BML::Detail::Guard([&] {
                                       *static_cast<Info *>(data) = Detail::CopyInfo(*info);
                                       return BML_OK;
                                   });
                               }>::Call,
                               &value)
                         : BML_ERROR_UNAVAILABLE;
    return code == BML_OK ? Result<Info>::Success(std::move(value)) : Result<Info>::Failure(code);
} catch (const std::bad_alloc &) {
    return Result<Info>::Failure(BML_ERROR_OUT_OF_MEMORY);
} catch (...) {
    return Result<Info>::Failure(BML_ERROR_FAIL);
}

using GameEvent = BML_GameEvent;
using CommandEventPhase = BML_CommandEventPhase;
struct RenderEvent {
    uint32_t Flags;
};
struct CheatEvent {
    bool Enabled;
};
struct LoadObjectEvent {
    std::string_view Filename;
    bool IsMap;
    std::string_view MasterName;
    uint32_t FilterClass;
    bool AddToScene, ReuseMeshes, ReuseMaterials, Dynamic;
    std::span<const BML_ObjectRef> Objects;
    BML_ObjectRef MasterObject;
};
struct LoadScriptEvent {
    std::string_view Filename;
    BML_ObjectRef Script;
};
struct CommandEvent {
    CommandEventPhase Phase;
    std::string_view Name, InvokedAs;
    std::span<const char *const> Arguments;
    bool Cheat;
};
struct ConfigEvent {
    BML_ModHandle Mod;
    std::string_view ModId, Category, Key;
    uint32_t Type;
    BML_ConfigPropertyHandle Property;
};
struct PhysicalizeEvent {
    BML_ObjectRef Target;
    bool Fixed;
    float Friction, Elasticity, Mass;
    std::optional<std::string_view> CollisionGroup;
    bool StartFrozen, EnableCollision, CalcMassCenter;
    float LinearDamp, RotDamp;
    std::optional<std::string_view> CollisionSurface;
    BML_Vec3 MassCenter;
    std::span<const BML_ObjectRef> ConvexMeshes;
    std::span<const BML_Vec3> SphereCenters;
    std::span<const float> SphereRadii;
    std::span<const BML_ObjectRef> ConcaveMeshes;
};
struct ObjectEvent {
    BML_ObjectRef Object;
};

class Instance {
  public:
    virtual ~Instance() noexcept = default;
    virtual void OnLoad(const Context &) {}
    virtual void OnUnload(const Context &) {}
    virtual void OnProcess(const Context &) {}
    virtual void OnRender(const Context &, const RenderEvent &) {}
    virtual void OnGameEvent(const Context &, GameEvent);
    virtual void OnCheatEnabled(const Context &, const CheatEvent &) {}
    virtual void OnLoadObject(const Context &, const LoadObjectEvent &) {}
    virtual void OnLoadScript(const Context &, const LoadScriptEvent &) {}
    virtual void OnCommandEvent(const Context &ctx, const CommandEvent &event) {
        if (event.Phase == BML_COMMAND_EVENT_PRE)
            OnPreCommandExecute(ctx, event);
        else if (event.Phase == BML_COMMAND_EVENT_POST)
            OnPostCommandExecute(ctx, event);
    }
    virtual void OnModifyConfig(const Context &, const ConfigEvent &) {}
    virtual void OnPhysicalize(const Context &, const PhysicalizeEvent &) {}
    virtual void OnUnphysicalize(const Context &, const ObjectEvent &) {}
    virtual void OnPreCommandExecute(const Context &, const CommandEvent &) {}
    virtual void OnPostCommandExecute(const Context &, const CommandEvent &) {}
    virtual void OnPreStartMenu(const Context &) {}
    virtual void OnPostStartMenu(const Context &) {}
    virtual void OnExitGame(const Context &) {}
    virtual void OnPreLoadLevel(const Context &) {}
    virtual void OnPostLoadLevel(const Context &) {}
    virtual void OnStartLevel(const Context &) {}
    virtual void OnPreResetLevel(const Context &) {}
    virtual void OnPostResetLevel(const Context &) {}
    virtual void OnPauseLevel(const Context &) {}
    virtual void OnUnpauseLevel(const Context &) {}
    virtual void OnPreExitLevel(const Context &) {}
    virtual void OnPostExitLevel(const Context &) {}
    virtual void OnPreNextLevel(const Context &) {}
    virtual void OnPostNextLevel(const Context &) {}
    virtual void OnDead(const Context &) {}
    virtual void OnPreEndLevel(const Context &) {}
    virtual void OnPostEndLevel(const Context &) {}
    virtual void OnCounterActive(const Context &) {}
    virtual void OnCounterInactive(const Context &) {}
    virtual void OnBallNavActive(const Context &) {}
    virtual void OnBallNavInactive(const Context &) {}
    virtual void OnCamNavActive(const Context &) {}
    virtual void OnCamNavInactive(const Context &) {}
    virtual void OnBallOff(const Context &) {}
    virtual void OnPreCheckpointReached(const Context &) {}
    virtual void OnPostCheckpointReached(const Context &) {}
    virtual void OnLevelFinish(const Context &) {}
    virtual void OnGameOver(const Context &) {}
    virtual void OnExtraPoint(const Context &) {}
    virtual void OnPreSubLife(const Context &) {}
    virtual void OnPostSubLife(const Context &) {}
    virtual void OnPreLifeUp(const Context &) {}
    virtual void OnPostLifeUp(const Context &) {}
};

inline void Instance::OnGameEvent(const Context &ctx, GameEvent event) {
    switch (event) {
    case BML_GAME_EVENT_PRE_START_MENU:
        OnPreStartMenu(ctx);
        break;
    case BML_GAME_EVENT_POST_START_MENU:
        OnPostStartMenu(ctx);
        break;
    case BML_GAME_EVENT_EXIT_GAME:
        OnExitGame(ctx);
        break;
    case BML_GAME_EVENT_PRE_LOAD_LEVEL:
        OnPreLoadLevel(ctx);
        break;
    case BML_GAME_EVENT_POST_LOAD_LEVEL:
        OnPostLoadLevel(ctx);
        break;
    case BML_GAME_EVENT_START_LEVEL:
        OnStartLevel(ctx);
        break;
    case BML_GAME_EVENT_PRE_RESET_LEVEL:
        OnPreResetLevel(ctx);
        break;
    case BML_GAME_EVENT_POST_RESET_LEVEL:
        OnPostResetLevel(ctx);
        break;
    case BML_GAME_EVENT_PAUSE_LEVEL:
        OnPauseLevel(ctx);
        break;
    case BML_GAME_EVENT_UNPAUSE_LEVEL:
        OnUnpauseLevel(ctx);
        break;
    case BML_GAME_EVENT_PRE_EXIT_LEVEL:
        OnPreExitLevel(ctx);
        break;
    case BML_GAME_EVENT_POST_EXIT_LEVEL:
        OnPostExitLevel(ctx);
        break;
    case BML_GAME_EVENT_PRE_NEXT_LEVEL:
        OnPreNextLevel(ctx);
        break;
    case BML_GAME_EVENT_POST_NEXT_LEVEL:
        OnPostNextLevel(ctx);
        break;
    case BML_GAME_EVENT_DEAD:
        OnDead(ctx);
        break;
    case BML_GAME_EVENT_PRE_END_LEVEL:
        OnPreEndLevel(ctx);
        break;
    case BML_GAME_EVENT_POST_END_LEVEL:
        OnPostEndLevel(ctx);
        break;
    case BML_GAME_EVENT_COUNTER_ACTIVE:
        OnCounterActive(ctx);
        break;
    case BML_GAME_EVENT_COUNTER_INACTIVE:
        OnCounterInactive(ctx);
        break;
    case BML_GAME_EVENT_BALL_NAV_ACTIVE:
        OnBallNavActive(ctx);
        break;
    case BML_GAME_EVENT_BALL_NAV_INACTIVE:
        OnBallNavInactive(ctx);
        break;
    case BML_GAME_EVENT_CAM_NAV_ACTIVE:
        OnCamNavActive(ctx);
        break;
    case BML_GAME_EVENT_CAM_NAV_INACTIVE:
        OnCamNavInactive(ctx);
        break;
    case BML_GAME_EVENT_BALL_OFF:
        OnBallOff(ctx);
        break;
    case BML_GAME_EVENT_PRE_CHECKPOINT_REACHED:
        OnPreCheckpointReached(ctx);
        break;
    case BML_GAME_EVENT_POST_CHECKPOINT_REACHED:
        OnPostCheckpointReached(ctx);
        break;
    case BML_GAME_EVENT_LEVEL_FINISH:
        OnLevelFinish(ctx);
        break;
    case BML_GAME_EVENT_GAME_OVER:
        OnGameOver(ctx);
        break;
    case BML_GAME_EVENT_EXTRA_POINT:
        OnExtraPoint(ctx);
        break;
    case BML_GAME_EVENT_PRE_SUB_LIFE:
        OnPreSubLife(ctx);
        break;
    case BML_GAME_EVENT_POST_SUB_LIFE:
        OnPostSubLife(ctx);
        break;
    case BML_GAME_EVENT_PRE_LIFE_UP:
        OnPreLifeUp(ctx);
        break;
    case BML_GAME_EVENT_POST_LIFE_UP:
        OnPostLifeUp(ctx);
        break;
    default:
        break;
    }
}

namespace Detail {
inline const BML_ModContextInterface *ContextApi() noexcept {
    return BML::Detail::Bind<BML_ModContextInterface>(
        BML_MOD_CONTEXT_INTERFACE_ID, 1, BML_MOD_CONTEXT_INTERFACE_1_0_SIZE,
        +[](const BML_ModContextInterface *table) {
            return BML_IFACE_HAS(table, BML_ModContextInterface, GetOwner) &&
                   BML_IFACE_HAS(table, BML_ModContextInterface, ReadMod) &&
                   BML_IFACE_HAS(table, BML_ModContextInterface, FindMod) &&
                   BML_IFACE_HAS(table, BML_ModContextInterface, VisitMods) &&
                   BML_IFACE_HAS(table, BML_ModContextInterface, Log) &&
                   BML_IFACE_HAS(table, BML_ModContextInterface, GetDirectory) &&
                   BML_IFACE_HAS(table, BML_ModContextInterface, GetResourcePath) &&
                   BML_IFACE_HAS(table, BML_ModContextInterface, ReadResource) &&
                   BML_IFACE_HAS(table, BML_ModContextInterface, ResourceExists);
        });
}
struct DefinitionStorage {
    const Definition Value;
    std::vector<BML_ModDependency> Dependencies;
    BML_ModDefinition Record{};
    explicit DefinitionStorage(Definition value) : Value(std::move(value)) {
        const auto valid = [](const std::string &text) {
            if (!BML::Detail::IsText(text))
                throw std::invalid_argument("embedded NUL in Mod definition");
        };
        valid(Value.Id);
        valid(Value.Name);
        valid(Value.Version);
        valid(Value.Author);
        valid(Value.Description);
        valid(Value.MinimumLoaderVersion);
        Dependencies.reserve(Value.Dependencies.size());
        for (const auto &dep : Value.Dependencies) {
            valid(dep.Id);
            valid(dep.MinimumVersion);
            Dependencies.push_back({sizeof(BML_ModDependency), dep.Id.c_str(),
                                    dep.MinimumVersion.c_str(), dep.Optional ? 1u : 0u});
        }
        Record = {sizeof(Record),
                  Value.Id.c_str(),
                  Value.Name.c_str(),
                  Value.Version.c_str(),
                  Value.Author.c_str(),
                  Value.Description.c_str(),
                  Value.MinimumLoaderVersion.c_str(),
                  Dependencies.data(),
                  Dependencies.size(),
                  sizeof(BML_ModDependency)};
    }
    DefinitionStorage(const DefinitionStorage &) = delete;
    DefinitionStorage &operator=(const DefinitionStorage &) = delete;
};

template <class T> struct Export {
    static_assert(std::is_base_of_v<Instance, T> && std::is_default_constructible_v<T>);
    static int BML_CDECL Create(BML_ModContextHandle, void **out) noexcept {
        if (!out)
            return BML_ERROR_INVALID_PARAMETER;
        *out = nullptr;
        return BML::Detail::Guard([&] {
            *out = new T;
            return BML_OK;
        });
    }
    static void BML_CDECL Destroy(void *instance) noexcept { delete static_cast<T *>(instance); }
    template <class Body> static int Callback(BML_ModContextHandle handle, Body &&body) noexcept {
        try {
            return body();
        } catch (const std::bad_alloc &) {
            (void)ContextAccess::Make(handle).LogError("Native callback ran out of memory");
            return BML_ERROR_OUT_OF_MEMORY;
        } catch (const std::exception &error) {
            (void)ContextAccess::Make(handle).LogError(error.what());
            return BML_ERROR_FAIL;
        } catch (...) {
            (void)ContextAccess::Make(handle).LogError(
                "Native callback threw an unknown exception");
            return BML_ERROR_FAIL;
        }
    }
    static int BML_CDECL OnLoad(void *p, BML_ModContextHandle h) noexcept {
        return Callback(h, [&] {
            static_cast<T *>(p)->OnLoad(ContextAccess::Make(h));
            return BML_OK;
        });
    }
    static int BML_CDECL OnUnload(void *p, BML_ModContextHandle h) noexcept {
        return Callback(h, [&] {
            static_cast<T *>(p)->OnUnload(ContextAccess::Make(h));
            return BML_OK;
        });
    }
    static int BML_CDECL OnProcess(void *p, BML_ModContextHandle h) noexcept {
        return Callback(h, [&] {
            static_cast<T *>(p)->OnProcess(ContextAccess::Make(h));
            return BML_OK;
        });
    }
    static int BML_CDECL OnGameEvent(void *p, BML_ModContextHandle h, BML_GameEvent e) noexcept {
        return Callback(h, [&] {
            static_cast<T *>(p)->OnGameEvent(ContextAccess::Make(h), e);
            return BML_OK;
        });
    }
    static int BML_CDECL OnRender(void *p, BML_ModContextHandle h,
                                  const BML_RenderEvent *e) noexcept {
        if (!e || e->StructSize < BML_RENDER_EVENT_1_0_SIZE)
            return BML_ERROR_INVALID_PARAMETER;
        return Callback(h, [&] {
            const RenderEvent event{e->Flags};
            static_cast<T *>(p)->OnRender(ContextAccess::Make(h), event);
            return BML_OK;
        });
    }
    static int BML_CDECL OnCheatEnabled(void *p, BML_ModContextHandle h,
                                        const BML_CheatEvent *e) noexcept {
        if (!e || e->StructSize < BML_CHEAT_EVENT_1_0_SIZE)
            return BML_ERROR_INVALID_PARAMETER;
        return Callback(h, [&] {
            const CheatEvent event{e->Enabled != 0};
            static_cast<T *>(p)->OnCheatEnabled(ContextAccess::Make(h), event);
            return BML_OK;
        });
    }
    static int BML_CDECL OnLoadObject(void *p, BML_ModContextHandle h,
                                      const BML_LoadObjectEvent *e) noexcept {
        if (!e || e->StructSize < BML_LOAD_OBJECT_EVENT_1_0_SIZE)
            return BML_ERROR_INVALID_PARAMETER;
        return Callback(h, [&] {
            const LoadObjectEvent event{
                Text(e->Filename),      e->IsMap != 0,      Text(e->MasterName),
                e->FilterClass,         e->AddToScene != 0, e->ReuseMeshes != 0,
                e->ReuseMaterials != 0, e->Dynamic != 0,    {e->Objects, e->ObjectCount},
                e->MasterObject};
            static_cast<T *>(p)->OnLoadObject(ContextAccess::Make(h), event);
            return BML_OK;
        });
    }
    static int BML_CDECL OnLoadScript(void *p, BML_ModContextHandle h,
                                      const BML_LoadScriptEvent *e) noexcept {
        if (!e || e->StructSize < BML_LOAD_SCRIPT_EVENT_1_0_SIZE)
            return BML_ERROR_INVALID_PARAMETER;
        return Callback(h, [&] {
            const LoadScriptEvent event{Text(e->Filename), e->Script};
            static_cast<T *>(p)->OnLoadScript(ContextAccess::Make(h), event);
            return BML_OK;
        });
    }
    static int BML_CDECL OnCommandEvent(void *p, BML_ModContextHandle h,
                                        const BML_CommandEvent *e) noexcept {
        if (!e || e->StructSize < BML_COMMAND_EVENT_1_0_SIZE)
            return BML_ERROR_INVALID_PARAMETER;
        return Callback(h, [&] {
            const CommandEvent event{e->Phase,
                                     Text(e->Name),
                                     Text(e->InvokedAs),
                                     {e->Arguments, e->ArgumentCount},
                                     e->Cheat != 0};
            static_cast<T *>(p)->OnCommandEvent(ContextAccess::Make(h), event);
            return BML_OK;
        });
    }
    static int BML_CDECL OnModifyConfig(void *p, BML_ModContextHandle h,
                                        const BML_ConfigEvent *e) noexcept {
        if (!e || e->StructSize < BML_CONFIG_EVENT_1_0_SIZE)
            return BML_ERROR_INVALID_PARAMETER;
        return Callback(h, [&] {
            const ConfigEvent event{e->Mod,       Text(e->ModId), Text(e->Category),
                                    Text(e->Key), e->Type,        e->Property};
            static_cast<T *>(p)->OnModifyConfig(ContextAccess::Make(h), event);
            return BML_OK;
        });
    }
    static int BML_CDECL OnPhysicalize(void *p, BML_ModContextHandle h,
                                       const BML_PhysicalizeEvent *e) noexcept {
        if (!e || e->StructSize < BML_PHYSICALIZE_EVENT_1_0_SIZE)
            return BML_ERROR_INVALID_PARAMETER;
        return Callback(h, [&] {
            const PhysicalizeEvent event{
                e->Target,
                e->Fixed != 0,
                e->Friction,
                e->Elasticity,
                e->Mass,
                e->CollisionGroup ? std::optional<std::string_view>(e->CollisionGroup)
                                  : std::nullopt,
                e->StartFrozen != 0,
                e->EnableCollision != 0,
                e->CalcMassCenter != 0,
                e->LinearDamp,
                e->RotDamp,
                e->CollisionSurface ? std::optional<std::string_view>(e->CollisionSurface)
                                    : std::nullopt,
                e->MassCenter,
                {e->ConvexMeshes, e->ConvexCount},
                {e->SphereCenters, e->SphereCount},
                {e->SphereRadii, e->SphereCount},
                {e->ConcaveMeshes, e->ConcaveCount}};
            static_cast<T *>(p)->OnPhysicalize(ContextAccess::Make(h), event);
            return BML_OK;
        });
    }
    static int BML_CDECL OnUnphysicalize(void *p, BML_ModContextHandle h,
                                         const BML_ObjectEvent *e) noexcept {
        if (!e || e->StructSize < BML_OBJECT_EVENT_1_0_SIZE)
            return BML_ERROR_INVALID_PARAMETER;
        return Callback(h, [&] {
            const ObjectEvent event{e->Object};
            static_cast<T *>(p)->OnUnphysicalize(ContextAccess::Make(h), event);
            return BML_OK;
        });
    }
    static constexpr BML_ModCallbacks Callbacks() noexcept {
        return {
            sizeof(BML_ModCallbacks),
            (!std::is_same_v<decltype(&T::OnLoad), decltype(&Instance::OnLoad)>) ? &OnLoad
                                                                                 : nullptr,
            (!std::is_same_v<decltype(&T::OnUnload), decltype(&Instance::OnUnload)>) ? &OnUnload
                                                                                     : nullptr,
            (!std::is_same_v<decltype(&T::OnProcess), decltype(&Instance::OnProcess)>) ? &OnProcess
                                                                                       : nullptr,
            (!std::is_same_v<decltype(&T::OnRender), decltype(&Instance::OnRender)>) ? &OnRender
                                                                                     : nullptr,
            (!std::is_same_v<decltype(&T::OnGameEvent), decltype(&Instance::OnGameEvent)> ||
             !std::is_same_v<decltype(&T::OnPreStartMenu), decltype(&Instance::OnPreStartMenu)> ||
             !std::is_same_v<decltype(&T::OnPostStartMenu), decltype(&Instance::OnPostStartMenu)> ||
             !std::is_same_v<decltype(&T::OnExitGame), decltype(&Instance::OnExitGame)> ||
             !std::is_same_v<decltype(&T::OnPreLoadLevel), decltype(&Instance::OnPreLoadLevel)> ||
             !std::is_same_v<decltype(&T::OnPostLoadLevel), decltype(&Instance::OnPostLoadLevel)> ||
             !std::is_same_v<decltype(&T::OnStartLevel), decltype(&Instance::OnStartLevel)> ||
             !std::is_same_v<decltype(&T::OnPreResetLevel), decltype(&Instance::OnPreResetLevel)> ||
             !std::is_same_v<decltype(&T::OnPostResetLevel),
                             decltype(&Instance::OnPostResetLevel)> ||
             !std::is_same_v<decltype(&T::OnPauseLevel), decltype(&Instance::OnPauseLevel)> ||
             !std::is_same_v<decltype(&T::OnUnpauseLevel), decltype(&Instance::OnUnpauseLevel)> ||
             !std::is_same_v<decltype(&T::OnPreExitLevel), decltype(&Instance::OnPreExitLevel)> ||
             !std::is_same_v<decltype(&T::OnPostExitLevel), decltype(&Instance::OnPostExitLevel)> ||
             !std::is_same_v<decltype(&T::OnPreNextLevel), decltype(&Instance::OnPreNextLevel)> ||
             !std::is_same_v<decltype(&T::OnPostNextLevel), decltype(&Instance::OnPostNextLevel)> ||
             !std::is_same_v<decltype(&T::OnDead), decltype(&Instance::OnDead)> ||
             !std::is_same_v<decltype(&T::OnPreEndLevel), decltype(&Instance::OnPreEndLevel)> ||
             !std::is_same_v<decltype(&T::OnPostEndLevel), decltype(&Instance::OnPostEndLevel)> ||
             !std::is_same_v<decltype(&T::OnCounterActive), decltype(&Instance::OnCounterActive)> ||
             !std::is_same_v<decltype(&T::OnCounterInactive),
                             decltype(&Instance::OnCounterInactive)> ||
             !std::is_same_v<decltype(&T::OnBallNavActive), decltype(&Instance::OnBallNavActive)> ||
             !std::is_same_v<decltype(&T::OnBallNavInactive),
                             decltype(&Instance::OnBallNavInactive)> ||
             !std::is_same_v<decltype(&T::OnCamNavActive), decltype(&Instance::OnCamNavActive)> ||
             !std::is_same_v<decltype(&T::OnCamNavInactive),
                             decltype(&Instance::OnCamNavInactive)> ||
             !std::is_same_v<decltype(&T::OnBallOff), decltype(&Instance::OnBallOff)> ||
             !std::is_same_v<decltype(&T::OnPreCheckpointReached),
                             decltype(&Instance::OnPreCheckpointReached)> ||
             !std::is_same_v<decltype(&T::OnPostCheckpointReached),
                             decltype(&Instance::OnPostCheckpointReached)> ||
             !std::is_same_v<decltype(&T::OnLevelFinish), decltype(&Instance::OnLevelFinish)> ||
             !std::is_same_v<decltype(&T::OnGameOver), decltype(&Instance::OnGameOver)> ||
             !std::is_same_v<decltype(&T::OnExtraPoint), decltype(&Instance::OnExtraPoint)> ||
             !std::is_same_v<decltype(&T::OnPreSubLife), decltype(&Instance::OnPreSubLife)> ||
             !std::is_same_v<decltype(&T::OnPostSubLife), decltype(&Instance::OnPostSubLife)> ||
             !std::is_same_v<decltype(&T::OnPreLifeUp), decltype(&Instance::OnPreLifeUp)> ||
             !std::is_same_v<decltype(&T::OnPostLifeUp), decltype(&Instance::OnPostLifeUp)>)
                ? &OnGameEvent
                : nullptr,
            (!std::is_same_v<decltype(&T::OnCheatEnabled), decltype(&Instance::OnCheatEnabled)>)
                ? &OnCheatEnabled
                : nullptr,
            (!std::is_same_v<decltype(&T::OnLoadObject), decltype(&Instance::OnLoadObject)>)
                ? &OnLoadObject
                : nullptr,
            (!std::is_same_v<decltype(&T::OnLoadScript), decltype(&Instance::OnLoadScript)>)
                ? &OnLoadScript
                : nullptr,
            (!std::is_same_v<decltype(&T::OnCommandEvent), decltype(&Instance::OnCommandEvent)> ||
             !std::is_same_v<decltype(&T::OnPreCommandExecute),
                             decltype(&Instance::OnPreCommandExecute)> ||
             !std::is_same_v<decltype(&T::OnPostCommandExecute),
                             decltype(&Instance::OnPostCommandExecute)>)
                ? &OnCommandEvent
                : nullptr,
            (!std::is_same_v<decltype(&T::OnModifyConfig), decltype(&Instance::OnModifyConfig)>)
                ? &OnModifyConfig
                : nullptr,
            (!std::is_same_v<decltype(&T::OnPhysicalize), decltype(&Instance::OnPhysicalize)>)
                ? &OnPhysicalize
                : nullptr,
            (!std::is_same_v<decltype(&T::OnUnphysicalize), decltype(&Instance::OnUnphysicalize)>)
                ? &OnUnphysicalize
                : nullptr};
    }
};

inline Definition ResolveDefinition(Definition definition) { return definition; }
template <class Factory>
    requires std::is_invocable_r_v<Definition, Factory>
Definition ResolveDefinition(Factory &&factory) {
    return std::invoke(std::forward<Factory>(factory));
}

template <class T, auto Factory>
int GetInterface(uint16_t major, const BML_ModInterface **out) noexcept {
    if (!out)
        return BML_ERROR_INVALID_PARAMETER;
    *out = nullptr;
    if (major != BML_MOD_INTERFACE_MAJOR)
        return BML_ERROR_VERSION_MISMATCH;
    return BML::Detail::Guard([&] {
        // Function-local initialization retries after exceptions. Metadata views
        // are wired only after their owning strings have reached final storage.
        static const DefinitionStorage metadata(Factory());
        static constexpr BML_ModCallbacks callbacks = Export<T>::Callbacks();
        static const BML_ModInterface table{
            BML_IFACE_HEADER(BML_ModInterface, BML_MOD_INTERFACE_ID, 1, 0), &metadata.Record,
            &callbacks, &Export<T>::Create, &Export<T>::Destroy};
        *out = &table;
        return BML_OK;
    });
}
} // namespace Detail

} // namespace BML::Mod

// Accept an inline Definition initializer or an existing no-argument factory.
// Both are evaluated lazily inside the exception-safe C entry, never at DLL load.
#define BML_DEFINE_MOD(Type, ...)                                                                  \
    BML_MOD_ENTRY(int) BML_GetModInterface(uint16_t major, const BML_ModInterface **out) {         \
        return ::BML::Mod::Detail::GetInterface<Type, []() -> ::BML::Mod::Definition {             \
            return ::BML::Mod::Detail::ResolveDefinition(__VA_ARGS__);                             \
        }>(major, out);                                                                            \
    }
// The common authoring surface is self-contained. Engine and ImGui remain opt-in.
#include "BML/Config.hpp"
#include "BML/Input.hpp"
#include "BML/Timer.hpp"
#include "BML/Command.hpp"
#endif
