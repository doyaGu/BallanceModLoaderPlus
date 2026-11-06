#ifndef BML_API_TIMERAPI_H
#define BML_API_TIMERAPI_H

#include "BML/Timer.h"
#include "Timer/TimerRegistry.h"

class ModContext;
class IMod;

namespace BML::Api {
class TimerApi {
public:
    int Register(ModContext &context, IMod &owner, BML_ModContextHandle contextHandle,
                 const BML_TimerDefinition *definition, BML_TimerCallback callback, void *userData,
                 BML_ReleaseCallback release, BML_TimerHandle *outTimer);
    int Read(IMod &owner, BML_TimerHandle timer, BML_TimerVisitor visitor, void *userData);
    int Control(IMod &owner, BML_TimerHandle timer, uint32_t operation);
    void FinishProcess();
    void CleanupOwner(IMod &owner);

    TimerRegistry &Registry() noexcept { return m_Registry; }

private:
    TimerRegistry m_Registry;
};

const BML_TimerInterface &TimerInterface();
} // namespace BML::Api
#endif
