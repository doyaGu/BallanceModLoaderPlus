#ifndef BML_TESTS_PLAYER_BEHAVIORLIFECYCLEFIXTUREAPI_H
#define BML_TESTS_PLAYER_BEHAVIORLIFECYCLEFIXTUREAPI_H

#include <cstdint>

#include "CKTypes.h"

class CKBehavior;

inline const CKGUID BML_LIFECYCLE_FIXTURE_GUID(
    0x6f736d3a, 0x741ec901);

enum class BMLLifecycleFixtureMode : std::uint32_t {
    Normal = 0,
    CloseOnEdited = 1,
    // Runs the close hook from the RESET, DETACH and DELETE callbacks, that
    // is from inside the native teardown the Loader drives for the block.
    CloseOnTeardown = 2,
    NormalizeOnEdited = 3,
    FailFirstEdited = 4,
    InsertPinOnSettingsEdited = 5,
};

struct BMLLifecycleFixtureEvent {
    std::uint32_t Message = 0;
    std::uint32_t BehaviorId = 0;
    std::int32_t SettingValue = 0;
    std::uint32_t OwnerVisible = 0;
    std::uint32_t ParentVisible = 0;
    std::uint32_t LinkVisible = 0;
    std::uint32_t SourceVisible = 0;
    std::uint32_t InputCount = 0;
    std::uint32_t OutputCount = 0;
    std::uint32_t PinCount = 0;
    std::uint32_t PoutCount = 0;
    std::uint32_t LocalCount = 0;
    std::int32_t LocalValue = 0;
    std::uint32_t BoundSourceCount = 0;
};

struct BMLLifecycleFixtureTrace {
    std::uint32_t Size = sizeof(BMLLifecycleFixtureTrace);
    std::uint32_t Version = 1;
    std::uint32_t EventCount = 0;
    BMLLifecycleFixtureEvent Events[32]{};
    std::uint32_t CreateCount = 0;
    std::uint32_t AttachCount = 0;
    std::uint32_t SettingsEditedCount = 0;
    std::uint32_t EditedCount = 0;
    std::uint32_t ResetCount = 0;
    std::uint32_t DetachCount = 0;
    std::uint32_t DeleteCount = 0;
    std::int32_t SettingsEditedObserved = 0;
    std::int32_t FinalNormalizedValue = 0;
    std::uint32_t CloseHookCalls = 0;
    std::uint32_t CloseHookAccepted = 0;
    // Executions of the block function. RunTimes keeps the time of the first
    // eight executions; two equal entries are two executions inside one
    // engine frame, which is how a double-driven Block is detected.
    std::uint32_t RunCount = 0;
    float RunTimes[8]{};
};

using BMLLifecycleFixtureCloseHook = int (*)(CKBehavior *, void *);
using BMLLifecycleFixtureEditedHook = int (*)(CKBehavior *, void *);
using BMLLifecycleFixtureResetTraceFn = void (*)();
using BMLLifecycleFixtureSetModeFn = void (*)(BMLLifecycleFixtureMode);
using BMLLifecycleFixtureSetContinuationFn = void (*)(std::int32_t frames);
using BMLLifecycleFixtureSetCloseHookFn = void (*)(
    BMLLifecycleFixtureCloseHook, void *);
using BMLLifecycleFixtureSetEditedHookFn = void (*)(
    BMLLifecycleFixtureEditedHook, void *);
using BMLLifecycleFixtureReadTraceFn = int (*)(
    BMLLifecycleFixtureTrace *);

#endif // BML_TESTS_PLAYER_BEHAVIORLIFECYCLEFIXTUREAPI_H
