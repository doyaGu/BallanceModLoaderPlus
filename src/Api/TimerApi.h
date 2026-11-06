#ifndef BML_API_TIMERAPI_H
#define BML_API_TIMERAPI_H
#include "BML/Timer.h"
#include "Timer/TimerRegistry.h"
class ModContext;
class IMod;
namespace BML::Api {
class TimerApi {
  public:
    int Register(ModContext &, IMod &, BML_ModContextHandle, const BML_TimerDefinition *,
                 BML_TimerCallback, void *, BML_ReleaseCallback, BML_TimerHandle *);
    int Read(IMod &, BML_TimerHandle, BML_TimerVisitor, void *);
    int Control(IMod &, BML_TimerHandle, uint32_t operation);
    void FinishProcess();
    void CleanupOwner(IMod &);
    TimerRegistry &Registry() noexcept { return m_Registry; }

  private:
    TimerRegistry m_Registry;
};
const BML_TimerInterface &TimerInterface();
} // namespace BML::Api
#endif
