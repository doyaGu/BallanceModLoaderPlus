#ifndef BML_SCRIPTENGINECALLS_H
#define BML_SCRIPTENGINECALLS_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "Behavior/Block.h"
#include "BML/Behavior/Blocks/ObjectLoad.hpp"
#include "Behavior/Sessions.h"

namespace BML {

// Private Behavior adapter for script bindings. Owns operations that outlive one
// call in the Loader's Session, which retires with the built-in Mod.
class ScriptEngineCalls final {
public:
    explicit ScriptEngineCalls(Behavior::Internal::Sessions &sessions) : m_Sessions(sessions) {}

    // Opens the Loader's Session for the built-in Mod's owner.
    Behavior::Internal::Status Open(const std::string &ownerId);

    Behavior::Internal::RunResult Run(CKBeObject *owner, const Behavior::Internal::BlockSpec &spec,
                                      int input = 0);
    // The returned array belongs to the load's Block, which stays open until
    // the next load replaces it.
    std::pair<XObjectArray *, CKObject *>
    LoadObjects(const Behavior::Blocks::ObjectLoad::Options &options, bool rename);
    CKBehavior *CreateUnmanaged(CKBehavior *parent, const Behavior::Internal::BlockSpec &spec);
    void ProcessFrame();
    void Reset();

private:
    Behavior::Internal::Sessions &m_Sessions;
    std::uintptr_t m_Session = 0;
    std::uintptr_t m_LastObjectLoad = 0;
    std::vector<std::uintptr_t> m_Tasks;
    // Never reset: an object that survives a world reset keeps its suffix, so
    // a later load must not hand the same one out again.
    unsigned int m_LoadCount = 0;
};

} // namespace BML

#endif // BML_SCRIPTENGINECALLS_H
