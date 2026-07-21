#ifndef BML_INTEROPSESSIONSERVICE_H
#define BML_INTEROPSESSIONSERVICE_H

#include <cstdint>
#include <memory>
#include <string>

#include "BML/Defines.h"
#include "InteropContextInternal.h"

namespace BML {

/* Ownership identities for consumer and provider resources.  Records,
 * streams, and cursors live in InteropRegistry; this service knows
 * nothing about their concrete handle types. */
class InteropSessionService {
public:
    struct State;

    InteropSessionService();
    ~InteropSessionService();

    InteropSessionService(const InteropSessionService &) = delete;
    InteropSessionService &operator=(const InteropSessionService &) = delete;

    void RegisterMod(const char *ownerId);
    void InvalidateMod(const char *ownerId);
    void RotateMod(const char *ownerId);

    BML_InteropCallContext CreateContextForOwner(const std::string &ownerId) const;
    int ValidateContext(const BML_InteropCallContext *context, bool requireSession) const;
    uint64_t GetSessionId(const BML_InteropCallContext *context) const;

private:
    std::unique_ptr<State> m_State;
};

} // namespace BML

#endif // BML_INTEROPSESSIONSERVICE_H
