#ifndef BML_BEHAVIOR_SCRIPT_H
#define BML_BEHAVIOR_SCRIPT_H

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Behavior/ObjectRef.h"
#include "Behavior/Sessions.h"
#include "Behavior/Status.h"

class CKContext;

namespace BML::Behavior {
class GraphEdit;
class Patches;
}

namespace BML::Behavior::Internal {

using ScriptId = std::uintptr_t;
using ScriptBodyId = std::uintptr_t;

enum class ScriptState {
    Ready,
    Closing,
    Failed,
};

struct ScriptObject {
    std::uint64_t Id = 0;
    std::uintptr_t Address = 0;
    ObjectRef Reference;

    [[nodiscard]] explicit operator bool() const noexcept {
        return Id != 0 && Address != 0 && !Reference.IsNull();
    }
};

struct ScriptIdentity {
    ScriptObject Root;
    ScriptObject Owner;
    ScriptObject Scene;
};

struct ScriptInfo {
    ScriptState State = ScriptState::Ready;
    ScriptIdentity Identity;
    bool Active = false;
    bool RequestedActive = false;
    int Priority = 0;
    Status LastStatus;
};

struct ScriptResult {
    Status Result;
    ScriptId Id = 0;
    ScriptInfo Info;

    [[nodiscard]] explicit operator bool() const noexcept {
        return Id != 0 && static_cast<bool>(Result);
    }
};

// The Script module deals only in stable identities. CKScriptWorld is the one
// adapter that knows how a top-level Virtools Script joins an owner and scene.
class ScriptWorld {
public:
    virtual ~ScriptWorld() = default;

    [[nodiscard]] virtual bool InDispatch() const noexcept = 0;
    virtual Status Create(void *owner, std::string_view name, int priority,
                          ScriptIdentity &out) = 0;
    // Defines the initial graph before the Script is published or activated.
    // The returned body stays owned by the Script and closes before its root.
    virtual Status Define(const SessionOwner &owner,
                          const ScriptIdentity &script,
                          GraphEdit body, ScriptBodyId &out) = 0;
    virtual Status Read(const ScriptIdentity &script, bool &active) = 0;
    virtual Status SetActive(const ScriptIdentity &script, bool active,
                             bool reset) = 0;
    virtual Status CloseBody(const SessionOwner &owner,
                             ScriptBodyId body) = 0;
    // Success means the root no longer exists. A failure keeps the Script in
    // Closing so the next safe point can retry without reusing its handle.
    virtual Status Destroy(const ScriptIdentity &script) = 0;
};

// Owns top-level Scripts for active Native Mod generations. A Session only
// authenticates admission; this module owns the world-bound native lifetime.
class Scripts final {
public:
    using Loaded = std::function<void(std::string_view, const ObjectRef &)>;

    explicit Scripts(std::unique_ptr<ScriptWorld> world,
                     Loaded loaded = {});
    ~Scripts();

    Scripts(const Scripts &) = delete;
    Scripts &operator=(const Scripts &) = delete;

    ScriptResult Create(const SessionOwner &owner, std::uintptr_t session,
                        void *nativeOwner, std::string name, int priority,
                        GraphEdit body);
    Status Read(const SessionOwner &owner, ScriptId script,
                ScriptInfo &out);
    Status SetActive(const SessionOwner &owner, ScriptId script, bool active,
                     bool reset, ScriptInfo &out);
    Status Close(const SessionOwner &owner, ScriptId script);
    void CloseSession(std::uintptr_t session);
    Status RetireOwner(std::string_view owner);

    void ObjectToBeDeleted(std::uint64_t object);
    void ProcessFrame();
    void ResetWorld();

private:
    struct Entry {
        ScriptId Id = 0;
        std::uintptr_t Session = 0;
        SessionOwner Owner;
        ScriptInfo Info;
        ScriptBodyId Body = 0;
        bool ResetOnActivation = false;
    };

    [[nodiscard]] Status Ready() const;
    [[nodiscard]] ScriptId NextId() noexcept;
    [[nodiscard]] bool OwnedBy(const Entry &entry,
                               const SessionOwner &owner) const noexcept;
    void Close(Entry &entry) noexcept;
    Status Retire(Entry &entry);
    void Process(const std::shared_ptr<Entry> &entry);

    std::unique_ptr<ScriptWorld> m_World;
    Loaded m_Loaded;
    std::thread::id m_Thread;
    mutable std::recursive_mutex m_Mutex;
    ScriptId m_NextId = 1;
    std::unordered_map<ScriptId, std::shared_ptr<Entry>> m_Scripts;
    // Native roots whose initial graph was rejected and whose cleanup must
    // retry. They were never published and are not addressable by a handle.
    std::vector<std::shared_ptr<Entry>> m_Retiring;
    std::vector<std::shared_ptr<Entry>> m_FrameEntries;
    bool m_Processing = false;
};

[[nodiscard]] std::unique_ptr<ScriptWorld> MakeCKScriptWorld(
    CKContext *context, Patches &patches,
    std::function<ObjectRef(const void *)> issueObjectRef);

} // namespace BML::Behavior::Internal

#endif // BML_BEHAVIOR_SCRIPT_H
