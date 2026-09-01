#ifndef BML_BEHAVIOR_PATCHES_H
#define BML_BEHAVIOR_PATCHES_H

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#include "Behavior/CKEdit.h"
#include "Behavior/Sessions.h"

namespace BML::Behavior {

using PatchId = std::uintptr_t;

struct PatchInfo {
    PatchState State = PatchState::Closed;
    Status Diagnostic;
};

// Owns live graph Patches for Mod generations. CKEdit remains the CK2 graph
// transaction adapter; this collection supplies the Loader lifetime boundary.
class Patches final {
public:
    Patches(CKContext *context, Runtime &runtime, PrototypeCatalog *catalog,
            GraphSource &graph);
    ~Patches();

    Patches(const Patches &) = delete;
    Patches &operator=(const Patches &) = delete;

    Status Begin(const SessionOwner &owner, CKBehavior *graph,
                 std::string name, Edit &out);
    Status Use(Edit &edit, CKBehavior *behavior, Node &out);
    Status Use(Edit &edit, CKBehaviorLink *link, Link &out);
    Status Add(Edit &edit, Spec block, Node &out);
    Status Apply(const SessionOwner &owner, const Edit &edit, PatchId &out);
    Status Read(const SessionOwner &owner, PatchId patch,
                PatchInfo &out) const;
    Status Close(const SessionOwner &owner, PatchId patch);

    Status RetireOwner(const std::string &ownerId);
    void ObjectsToBeDeleted(const CK_ID *ids, int count);
    void ResetWorld();
    void ProcessFrame();

private:
    struct OwnedPatch {
        PatchId Id = 0;
        SessionOwner Owner;
        CK_ID Graph = 0;
        Patch Value;
    };

    [[nodiscard]] Status Ready() const;
    [[nodiscard]] PatchId NextId();
    Status Close(OwnedPatch &patch);
    void Collect();

    CKEdit m_Edit;
    std::thread::id m_Thread;
    mutable std::recursive_mutex m_Mutex;
    PatchId m_NextId = 1;
    std::map<PatchId, OwnedPatch> m_Patches;
};

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_PATCHES_H
