#ifndef BML_TIMER_H
#define BML_TIMER_H

#include "BML/Mod.h"

BML_BEGIN_CDECLS
#define BML_TIMER_DEFINITION_1_0_SIZE                                                              \
    (offsetof(BML_TimerDefinition, RepeatCount) + sizeof(((BML_TimerDefinition *)0)->RepeatCount))
#define BML_TIMER_DEFINITION_INIT {.StructSize = sizeof(BML_TimerDefinition)}
#define BML_TIMER_EVENT_1_0_SIZE                                                                   \
    (offsetof(BML_TimerEvent, Progress) + sizeof(((BML_TimerEvent *)0)->Progress))
#define BML_TIMER_EVENT_INIT {.StructSize = sizeof(BML_TimerEvent)}
#define BML_TIMER_INTERFACE_1_0_SIZE                                                               \
    (offsetof(BML_TimerInterface, Cancel) + sizeof(((BML_TimerInterface *)0)->Cancel))
#pragma pack(push, 8)
#define BML_TIMER_INTERFACE_ID "bml.timer"
#define BML_TIMER_INTERFACE_MAJOR 1u
#define BML_TIMER_ONCE 0u
#define BML_TIMER_LOOP 1u
#define BML_TIMER_REPEAT 2u
#define BML_TIMER_INTERVAL 3u
#define BML_TIMER_DEBOUNCE 4u
#define BML_TIMER_THROTTLE 5u
#define BML_TIMER_TIMEBASE_TICK 0u
#define BML_TIMER_TIMEBASE_TIME 1u
#define BML_TIMER_TIMEBASE_REALTIME 2u
#define BML_TIMER_IDLE 0u
#define BML_TIMER_RUNNING 1u
#define BML_TIMER_PAUSED 2u
#define BML_TIMER_COMPLETED 3u
#define BML_TIMER_CANCELLED 4u
typedef uint64_t BML_TimerHandle;

typedef struct BML_TimerDefinition {
    size_t StructSize;
    const char *Name;
    uint32_t Type;
    uint32_t TimeBase;
    uint32_t StartPaused;
    int32_t Priority;
    double DelayMs;
    uint64_t DelayTicks;
    uint64_t RepeatCount;
} BML_TimerDefinition;

typedef struct BML_TimerEvent {
    size_t StructSize;
    BML_TimerHandle Handle;
    const char *Name;
    uint32_t State;
    uint32_t Type;
    uint32_t TimeBase;
    uint64_t CompletedIterations;
    int64_t RemainingIterations;
    float Progress;
} BML_TimerEvent;

typedef int(BML_CDECL *BML_TimerCallback)(void *userData, BML_ModContextHandle context,
                                          const BML_TimerEvent *event, uint32_t *continueLoop);
typedef void(BML_CDECL *BML_ReleaseCallback)(void *userData);
typedef int(BML_CDECL *BML_TimerVisitor)(void *userData, const BML_TimerEvent *event);

typedef struct BML_TimerInterface {
    BML_InterfaceHeader Header;
    int(BML_CDECL *Register)(BML_ModContextHandle context, const BML_TimerDefinition *definition,
                             BML_TimerCallback callback, void *userData,
                             BML_ReleaseCallback release, BML_TimerHandle *outTimer);
    int(BML_CDECL *Read)(BML_ModContextHandle context, BML_TimerHandle timer,
                         BML_TimerVisitor visitor, void *userData);
    int(BML_CDECL *Pause)(BML_ModContextHandle context, BML_TimerHandle timer);
    int(BML_CDECL *Resume)(BML_ModContextHandle context, BML_TimerHandle timer);
    int(BML_CDECL *Cancel)(BML_ModContextHandle context, BML_TimerHandle timer);
} BML_TimerInterface;

#pragma pack(pop)
BML_END_CDECLS

#endif
